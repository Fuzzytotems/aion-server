// CM_REMOVE_ALTERED_STATE (P5-16, m5b2-plan.md P-03/P-05): C_TURN_OFF_ABNORMAL_STATUS, what the client sends when the player right-clicks one
// of his own effect icons - the only client-driven way to end an effect early.
//
// Java: game-server/src/com/aionemu/gameserver/network/aion/clientpackets/CM_REMOVE_ALTERED_STATE.java:23-42.
//
// runImpl asks the player's EffectController for the effect of the skill (findBySkillId, EffectController.java:327-332), which M5b-2 part 2
// ported (m5b2-plan.md K-02), so its four arms are driven here: a skill id without a template is Java's NullPointerException out of
// findBySkillId; a skill of which the player has no effect does nothing; a DEBUFF stays and leaves the audit line "tried to remove a debuff:
// <id> <name> (effector: <creature>)"; anything else is ended.
//
// The effects are real Effects (Effect::create -> initialize -> addToEffectedController, what Skill.applyEffect does for an effected creature)
// whose templates carry the ProbeEffect of the effect lane's test support (tests/skills/P5-02b/EffectTestSupport.h): the leaf effect classes
// are part 3's, and a probe's journal says whether the effect was ended. Effect.startEffect and endEffect end in the effected creature's
// instance handler, so the player stands in a Poeta map instance with the real GeneralInstanceHandler.

#include "../cm_ak/InWorldPacketRunSupport.h"
#include "../skills/P5-02b/EffectTestSupport.h"

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "aion/commons/utils/ByteBuffer.h"
#include "aion/gameserver/configs/main/LoggingConfig.h"
#include "aion/gameserver/configs/main/PunishmentConfig.h"
#include "aion/gameserver/controllers/effect/PlayerEffectController.h"
#include "aion/gameserver/instance/handlers/GeneralInstanceHandler.h"
#include "aion/gameserver/network/aion/clientpackets/CM_REMOVE_ALTERED_STATE.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/skillengine/model/Effect.h"
#include "aion/gameserver/world/WorldMap.h"
#include "aion/gameserver/world/WorldMap2DInstance.h"
#include "aion/gameserver/world/WorldPosition.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets {

/** The friend CM_REMOVE_ALTERED_STATE.h declares: the skill id readImpl decoded, which Java keeps private */
struct CM_REMOVE_ALTERED_STATETestAccess {
	static int32_t skillId(const CM_REMOVE_ALTERED_STATE& p) { return p.skillId; }
};

namespace testing {
namespace {

namespace et = skillengine::effecttest;
using Access = CM_REMOVE_ALTERED_STATETestAccess;
using network::test::LogCapture;
using network::test::PacketWriter;

/** the decoded opcode of ClientPacketInfo.gen.inc:47 (Java AionClientPacketFactory: packets[35] = CM_REMOVE_ALTERED_STATE, State.IN_GAME) */
constexpr int32_t OPCODE = 35;

const char* BASE_CLIENT_PACKET_LOGGER = "com.aionemu.commons.network.packet.BaseClientPacket";
const char* AUDIT_LOGGER = "AUDIT_LOG"; // AuditLogger.cpp:22

/** Java readImpl: UH skill id, C, C (the second "seen 1 with skillId 3573") */
std::vector<uint8_t> body(int32_t skillId, int32_t first = 0, int32_t second = 1) {
	return PacketWriter().H(skillId).C(first).C(second).data;
}

/** A fresh packet that has read `data`; nullptr if read() failed */
std::unique_ptr<CM_REMOVE_ALTERED_STATE> readPacket(const std::vector<uint8_t>& data, int32_t& unreadBytes) {
	std::vector<uint8_t> copy = data;
	auto packet = std::make_unique<CM_REMOVE_ALTERED_STATE>(OPCODE, StateSet{AionConnection_State::IN_GAME});
	packet->setBuffer(commons::utils::ByteBuffer::wrap(copy));
	if (!packet->read())
		return nullptr;
	unreadBytes = packet->getRemainingBytes();
	return packet;
}

TEST(RemoveAlteredStateReadTest, ASkillIdAndTwoBytes) {
	for (int32_t skillId : {3195, 0xFFFF}) {
		SCOPED_TRACE("skill " + std::to_string(skillId));
		LogCapture capture({BASE_CLIENT_PACKET_LOGGER});
		int32_t unread = -1;
		std::unique_ptr<CM_REMOVE_ALTERED_STATE> p = readPacket(body(skillId, 0x7F, 1), unread);
		ASSERT_NE(p, nullptr);
		EXPECT_EQ(Access::skillId(*p), skillId) << "readUH: 0xFFFF is 65535";
		EXPECT_EQ(unread, 0) << "2 + 1 + 1 bytes";
		EXPECT_FALSE(capture.contains("Missing")) << capture.dump();
	}
	{ // the second byte missing: the last readC underflows
		LogCapture capture({BASE_CLIENT_PACKET_LOGGER});
		int32_t unread = -1;
		ASSERT_NE(readPacket(PacketWriter().H(3195).C(0).data, unread), nullptr);
		EXPECT_TRUE(capture.contains("Missing C")) << capture.dump();
	}
	{ // a spare byte is left: readImpl reads nothing after the second C
		std::vector<uint8_t> data = body(3195);
		data.push_back(0);
		int32_t unread = -1;
		ASSERT_NE(readPacket(data, unread), nullptr);
		EXPECT_EQ(unread, 1);
	}
}

/** a BUFF (Effect.getSkillSubType() != DEBUFF: the arm that ends the effect) and a DEBUFF (the audited arm); 3195 has no template at all */
constexpr int32_t BUFF_SKILL = 9301;
constexpr int32_t DEBUFF_SKILL = 9302;
constexpr int32_t UNKNOWN_SKILL = 3195;

std::string skillTemplateXml(int32_t skillId, std::string_view subType, std::string_view targetSlot) {
	return R"(<skill_template skill_id=")" + std::to_string(skillId) + R"(" name="probe )" + std::to_string(skillId) + R"(" nameId="1" stack="RAS)" +
		std::to_string(skillId) + R"(" lvl="1" skilltype="MAGICAL" skillsubtype=")" + std::string(subType) + R"(" tslot=")" +
		std::string(targetSlot) + R"(" activation="ACTIVE" duration="0"/>)";
}

/** AuditLogger only writes its line with gameserver.log.audit on, and must not reach AutoBan (PunishmentConfig off) */
class AuditScope {
public:
	AuditScope() {
		configs::main::PunishmentConfig::PUNISHMENT_ENABLE.store(false);
		configs::main::LoggingConfig::LOG_AUDIT.store(true);
	}
	~AuditScope() { configs::main::LoggingConfig::LOG_AUDIT.store(false); }
	AuditScope(const AuditScope&) = delete;
	AuditScope& operator=(const AuditScope&) = delete;
};

class RemoveAlteredStateRunTest : public InWorldPacketTest {
protected:
	void SetUp() override {
		InWorldPacketTest::SetUp();
		et::publishWorldStaticDataOnce();
		dataholders::DataManager::SKILL_DATA.resetForTests(); // the fixture published an empty holder
		dataholders::DataManager::SKILL_DATA.publish(xml::bindString<dataholders::SkillData>(skillContext,
			"<skill_data>" + skillTemplateXml(BUFF_SKILL, "BUFF", "BUFF") + skillTemplateXml(DEBUFF_SKILL, "DEBUFF", "DEBUFF") +
				"</skill_data>"));
		et::injectProbes(skillTemplate(BUFF_SKILL), lastingProbe("buff"));
		et::injectProbes(skillTemplate(DEBUFF_SKILL), lastingProbe("debuff"));

		map = world::WorldMap::create(dataholders::DataManager::WORLD_MAPS_DATA->getTemplate(et::POETA));
		mapInstance = world::WorldMap2DInstance::create(*map, 1, 0, 0, [](world::WorldMapInstance& instance) {
			return runtime::Ref<::aion::gameserver::instance::handlers::InstanceHandler>(
				::aion::gameserver::instance::handlers::GeneralInstanceHandler::create(instance));
		});
		actor = makePlayer(320001, 9401, "Buffed");
		actor.player->setPosition(world::WorldPosition::create(et::POETA, 500.0f, 500.0f, 100.0f, int8_t{0}, mapInstance->getRegion(500, 500, 100)));
		actor.player->getPosition()->setIsSpawned(true);
		client = std::make_unique<TestClient>();
		client->enterWorld(actor);
		(*client)->clearSent();
	}

	void TearDown() override {
		if (actor.player) {
			actor.player->getEffectController()->removeAllEffects(true); // an effect holds its effector: the cycle is cut here
			actor.player->setClientConnection(nullptr);
		}
		client.reset();
		actor = {};
		mapInstance = nullptr;
		map = nullptr;
		InWorldPacketTest::TearDown(); // forgets SKILL_DATA, and with it the probes
	}

	const skillengine::model::SkillTemplate* skillTemplate(int32_t skillId) {
		return dataholders::DataManager::SKILL_DATA->getSkillTemplate(skillId);
	}

	/** One probe that succeeds and lasts a minute (its end task waits on the manual clock), recording into `journal` */
	std::vector<std::unique_ptr<skillengine::effect::EffectTemplate>> lastingProbe(std::string name) {
		std::unique_ptr<et::ProbeEffect> probe = et::probe(std::move(name), &journal, 1);
		probe->duration2 = 60000;
		return et::probeList(std::move(probe));
	}

	/** Java Skill.applyEffect for the player as his own effector: new Effect(...), initialize(), addToEffectedController() */
	void putOnThePlayer(int32_t skillId) {
		runtime::Ref<skillengine::model::Effect> effect =
			skillengine::model::Effect::create(*actor.player, runtime::Ptr<model::gameobjects::Creature>(*actor.player), skillTemplate(skillId), 1);
		effect->initialize();
		effect->addToEffectedController();
		ASSERT_NE(actor.player->getEffectController()->findBySkillId(skillId), nullptr) << "the effect of " << skillId << " is on the player";
		journal.clear();
		(*client)->clearSent();
	}

	void removeAlteredState(int32_t skillId) {
		Driver<CM_REMOVE_ALTERED_STATE> packet(OPCODE);
		packet.readAndRun(body(skillId), client->get());
	}

	xml::LoadContext skillContext;
	et::Journal journal;
	runtime::Ref<world::WorldMap> map;
	runtime::Ref<world::WorldMapInstance> mapInstance;
	PlayerFixture actor;
	std::unique_ptr<TestClient> client;
};

TEST_F(RemoveAlteredStateRunTest, ASkillIdWithoutATemplateThrowsLikeJava) {
	ASSERT_EQ(skillTemplate(UNKNOWN_SKILL), nullptr);

	// CM_REMOVE_ALTERED_STATE.java:33 -> EffectController.findBySkillId, which throws for an id SKILL_DATA does not know
	// (EffectController.java:328-330); the client chooses the id, and Java's packet processor logs the exception
	try {
		removeAlteredState(UNKNOWN_SKILL);
		FAIL() << "findBySkillId must throw for a skill without a template";
	} catch (const runtime::NullPointerException& npe) {
		EXPECT_EQ(std::string(npe.what()), "Skill with ID 3195 does not exist");
	}
	EXPECT_TRUE((*client)->sentBytes().empty());
}

TEST_F(RemoveAlteredStateRunTest, ASkillThePlayerHasNoEffectOfDoesNothing) {
	putOnThePlayer(DEBUFF_SKILL); // another skill's effect, which the lookup must not find
	AuditScope auditOn;
	LogCapture audit({AUDIT_LOGGER});

	EXPECT_NO_THROW(removeAlteredState(BUFF_SKILL));

	// :34, `if (effect != null)`: nothing to end and nothing to audit
	EXPECT_TRUE(journal.empty());
	EXPECT_TRUE((*client)->sentBytes().empty());
	EXPECT_FALSE(audit.contains("tried to remove a debuff")) << audit.dump();
	EXPECT_NE(actor.player->getEffectController()->findBySkillId(DEBUFF_SKILL), nullptr);
}

TEST_F(RemoveAlteredStateRunTest, ABuffIsEndedAtThePlayersRequest) {
	putOnThePlayer(BUFF_SKILL);
	AuditScope auditOn;
	LogCapture audit({AUDIT_LOGGER});

	EXPECT_NO_THROW(removeAlteredState(BUFF_SKILL));

	// :37-38, effect.endEffect(): the effect's templates are ended and the effect leaves the controller
	EXPECT_EQ(journal, (et::Journal{"buff.end"}));
	EXPECT_EQ(actor.player->getEffectController()->findBySkillId(BUFF_SKILL), nullptr);
	EXPECT_FALSE(audit.contains("tried to remove a debuff")) << audit.dump();
}

TEST_F(RemoveAlteredStateRunTest, ADebuffStaysAndTheAttemptIsAudited) {
	putOnThePlayer(DEBUFF_SKILL);
	AuditScope auditOn;
	LogCapture audit({AUDIT_LOGGER});

	EXPECT_NO_THROW(removeAlteredState(DEBUFF_SKILL));

	// :35-37: a client that asks to drop a debuff is cheating; the effect is not touched
	EXPECT_EQ(audit.count("tried to remove a debuff: " + std::to_string(DEBUFF_SKILL) + " probe " + std::to_string(DEBUFF_SKILL) +
				  " (effector: " + actor.player->toString() + ")"),
		1)
		<< audit.dump();
	EXPECT_TRUE(journal.empty()) << "no endEffect";
	EXPECT_NE(actor.player->getEffectController()->findBySkillId(DEBUFF_SKILL), nullptr);
	EXPECT_TRUE((*client)->sentBytes().empty());
}

} // namespace
} // namespace testing
} // namespace aion::gameserver::network::aion::clientpackets
