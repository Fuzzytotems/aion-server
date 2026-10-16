// CM_TOGGLE_SKILL_DEACTIVATE (P5-16, m5e-plan.md C-04): C_TURN_OFF_TOGGLE_SKILL, what the client sends when the player clicks a toggle or
// stance skill that is on - the only way to switch one off before its timer runs out (m5e-plan.md W-08).
//
// Java: game-server/src/com/aionemu/gameserver/network/aion/clientpackets/CM_TOGGLE_SKILL_DEACTIVATE.java:23-41.
//
// runImpl looks the skill up in SKILL_DATA: an id without a template, or a skill that is neither a toggle nor a stance, leaves the audit line
// "tried to remove non-toggle skill effect (<id>) through CM_TOGGLE_SKILL_DEACTIVATE"; otherwise the player's effect of that skill is removed
// (EffectController.removeEffect: findBySkillId, endEffect), and a stance of that skill the player is still under is stopped
// (PlayerController.stopStance: SM_PLAYER_STANCE 0) - "still", because the end of a stance's effect stops its stance itself (Effect.java:721-725).
// The effects are real Effects carrying the effect lane's probes, set up as RemoveAlteredStatePacketTest does.

#include "../cm_ak/InWorldPacketRunSupport.h"
#include "../skills/P5-02b/EffectTestSupport.h"

#include <algorithm>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "aion/commons/utils/ByteBuffer.h"
#include "aion/gameserver/configs/main/LoggingConfig.h"
#include "aion/gameserver/configs/main/PunishmentConfig.h"
#include "aion/gameserver/controllers/PlayerController.h"
#include "aion/gameserver/controllers/effect/PlayerEffectController.h"
#include "aion/gameserver/instance/handlers/GeneralInstanceHandler.h"
#include "aion/gameserver/network/aion/clientpackets/CM_TOGGLE_SKILL_DEACTIVATE.h"
#include "aion/gameserver/network/aion/serverpackets/SM_PLAYER_STANCE.h"
#include "aion/gameserver/skillengine/model/Effect.h"
#include "aion/gameserver/world/WorldMap.h"
#include "aion/gameserver/world/WorldMap2DInstance.h"
#include "aion/gameserver/world/WorldPosition.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets::testing {
namespace {

namespace et = skillengine::effecttest;
using network::test::LogCapture;
using network::test::PacketWriter;

/** the decoded opcode of ClientPacketInfo.gen.inc:46 (Java AionClientPacketFactory: packets[34] = CM_TOGGLE_SKILL_DEACTIVATE, State.IN_GAME) */
constexpr int32_t OPCODE = 34;

const char* BASE_CLIENT_PACKET_LOGGER = "com.aionemu.commons.network.packet.BaseClientPacket";
const char* AUDIT_LOGGER = "AUDIT_LOG"; // AuditLogger.cpp:22

/** Java readImpl: UH skill id, H, H */
std::vector<uint8_t> body(int32_t skillId) {
	return PacketWriter().H(skillId).H(0).H(0).data;
}

/** a toggle, a stance and a skill that is neither (the audited arm); 0xFFFF has no template at all */
constexpr int32_t TOGGLE_SKILL = 9311;
constexpr int32_t STANCE_SKILL = 9312;
constexpr int32_t ACTIVE_SKILL = 9313;
constexpr int32_t UNKNOWN_SKILL = 0xFFFF;

std::string skillTemplateXml(int32_t skillId, std::string_view activation, std::string_view extra = {}) {
	return R"(<skill_template skill_id=")" + std::to_string(skillId) + R"(" name="probe )" + std::to_string(skillId) + R"(" nameId="1" stack="TSD)" +
		std::to_string(skillId) + R"(" lvl="1" skilltype="MAGICAL" skillsubtype="BUFF" tslot="BUFF" activation=")" + std::string(activation) +
		R"(" duration="0" )" + std::string(extra) + "/>";
}

std::string auditLine(int32_t skillId) {
	return "tried to remove non-toggle skill effect (" + std::to_string(skillId) + ") through CM_TOGGLE_SKILL_DEACTIVATE";
}

TEST(ToggleSkillDeactivateReadTest, ASkillIdAndTwoShorts) {
	{
		LogCapture capture({BASE_CLIENT_PACKET_LOGGER});
		std::vector<uint8_t> data = body(TOGGLE_SKILL);
		auto packet = std::make_unique<CM_TOGGLE_SKILL_DEACTIVATE>(OPCODE, StateSet{AionConnection_State::IN_GAME});
		packet->setBuffer(commons::utils::ByteBuffer::wrap(data));
		ASSERT_TRUE(packet->read());
		EXPECT_EQ(packet->getRemainingBytes(), 0) << "2 + 2 + 2 bytes";
		EXPECT_FALSE(capture.contains("Missing")) << capture.dump();
	}
	{ // the last short missing: the second readH underflows
		LogCapture capture({BASE_CLIENT_PACKET_LOGGER});
		std::vector<uint8_t> data = PacketWriter().H(TOGGLE_SKILL).H(0).data;
		auto packet = std::make_unique<CM_TOGGLE_SKILL_DEACTIVATE>(OPCODE, StateSet{AionConnection_State::IN_GAME});
		packet->setBuffer(commons::utils::ByteBuffer::wrap(data));
		ASSERT_TRUE(packet->read());
		EXPECT_TRUE(capture.contains("Missing H")) << capture.dump();
	}
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

class ToggleSkillDeactivateRunTest : public InWorldPacketTest {
protected:
	void SetUp() override {
		InWorldPacketTest::SetUp();
		et::publishWorldStaticDataOnce();
		dataholders::DataManager::SKILL_DATA.resetForTests(); // the fixture published an empty holder
		dataholders::DataManager::SKILL_DATA.publish(xml::bindString<dataholders::SkillData>(skillContext,
			"<skill_data>" + skillTemplateXml(TOGGLE_SKILL, "TOGGLE") + skillTemplateXml(STANCE_SKILL, "ACTIVE", R"(stance="true")") +
				skillTemplateXml(ACTIVE_SKILL, "ACTIVE") + "</skill_data>"));
		et::injectProbes(skillTemplate(TOGGLE_SKILL), lastingProbe("toggle"));
		et::injectProbes(skillTemplate(STANCE_SKILL), lastingProbe("stance"));
		et::injectProbes(skillTemplate(ACTIVE_SKILL), lastingProbe("active"));

		map = world::WorldMap::create(dataholders::DataManager::WORLD_MAPS_DATA->getTemplate(et::POETA));
		mapInstance = world::WorldMap2DInstance::create(*map, 1, 0, 0, [](world::WorldMapInstance& instance) {
			return runtime::Ref<::aion::gameserver::instance::handlers::InstanceHandler>(
				::aion::gameserver::instance::handlers::GeneralInstanceHandler::create(instance));
		});
		actor = makePlayer(320011, 9411, "Toggler");
		actor.player->setPosition(world::WorldPosition::create(et::POETA, 500.0f, 500.0f, 100.0f, int8_t{0}, mapInstance->getRegion(500, 500, 100)));
		actor.player->getPosition()->setIsSpawned(true);
		client = std::make_unique<TestClient>();
		client->enterWorld(actor);
		(*client)->clearSent();
	}

	void TearDown() override {
		if (actor.player) {
			actor.player->getController().stopStance(); // the stance observer holds the player
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

	void deactivate(int32_t skillId) {
		Driver<CM_TOGGLE_SKILL_DEACTIVATE> packet(OPCODE);
		packet.readAndRun(body(skillId), client->get());
	}

	/** how many SM_PLAYER_STANCE(player, state) the player was sent */
	int64_t stancePackets(int32_t state) {
		const std::vector<uint8_t> expected = serialized(serverpackets::SM_PLAYER_STANCE(*actor.player, state), client->con());
		const std::vector<std::vector<uint8_t>> sent = (*client)->sentBytes();
		return std::count(sent.begin(), sent.end(), expected);
	}

	xml::LoadContext skillContext;
	et::Journal journal;
	runtime::Ref<world::WorldMap> map;
	runtime::Ref<world::WorldMapInstance> mapInstance;
	PlayerFixture actor;
	std::unique_ptr<TestClient> client;
};

TEST_F(ToggleSkillDeactivateRunTest, ASkillIdWithoutATemplateIsAudited) {
	ASSERT_EQ(skillTemplate(UNKNOWN_SKILL), nullptr);
	AuditScope auditOn;
	LogCapture audit({AUDIT_LOGGER});

	EXPECT_NO_THROW(deactivate(UNKNOWN_SKILL));

	// CM_TOGGLE_SKILL_DEACTIVATE.java:33-36; readUH: 0xFFFF is 65535
	EXPECT_EQ(audit.count(auditLine(65535)), 1) << audit.dump();
	EXPECT_TRUE((*client)->sentBytes().empty());
}

TEST_F(ToggleSkillDeactivateRunTest, ASkillThatIsNeitherToggleNorStanceIsAuditedAndItsEffectStays) {
	putOnThePlayer(ACTIVE_SKILL);
	AuditScope auditOn;
	LogCapture audit({AUDIT_LOGGER});

	EXPECT_NO_THROW(deactivate(ACTIVE_SKILL));

	EXPECT_EQ(audit.count(auditLine(ACTIVE_SKILL)), 1) << audit.dump();
	EXPECT_TRUE(journal.empty()) << "no endEffect";
	EXPECT_NE(actor.player->getEffectController()->findBySkillId(ACTIVE_SKILL), nullptr);
	EXPECT_TRUE((*client)->sentBytes().empty());
}

TEST_F(ToggleSkillDeactivateRunTest, AToggleIsSwitchedOff) {
	putOnThePlayer(TOGGLE_SKILL);
	AuditScope auditOn;
	LogCapture audit({AUDIT_LOGGER});

	EXPECT_NO_THROW(deactivate(TOGGLE_SKILL));

	// :37, removeEffect: findBySkillId, endEffect; the player is under no stance, so :39-40 do nothing
	EXPECT_EQ(journal, (et::Journal{"toggle.end"}));
	EXPECT_EQ(actor.player->getEffectController()->findBySkillId(TOGGLE_SKILL), nullptr);
	EXPECT_FALSE(audit.contains("tried to remove")) << audit.dump();
	EXPECT_EQ(stancePackets(0), 0);
}

TEST_F(ToggleSkillDeactivateRunTest, AToggleThePlayerDoesNotHaveDoesNothing) {
	putOnThePlayer(ACTIVE_SKILL); // another skill's effect, which the lookup must not find
	AuditScope auditOn;
	LogCapture audit({AUDIT_LOGGER});

	EXPECT_NO_THROW(deactivate(TOGGLE_SKILL));

	EXPECT_TRUE(journal.empty());
	EXPECT_FALSE(audit.contains("tried to remove")) << audit.dump();
	EXPECT_TRUE((*client)->sentBytes().empty());
	EXPECT_NE(actor.player->getEffectController()->findBySkillId(ACTIVE_SKILL), nullptr);
}

TEST_F(ToggleSkillDeactivateRunTest, AStanceIsSwitchedOffAndStopped) {
	putOnThePlayer(STANCE_SKILL);
	actor.player->getController().startStance(STANCE_SKILL);
	ASSERT_EQ(actor.player->getController().getStanceSkillId(), STANCE_SKILL);
	(*client)->clearSent();
	AuditScope auditOn;
	LogCapture audit({AUDIT_LOGGER});

	EXPECT_NO_THROW(deactivate(STANCE_SKILL));

	// :37 ends the effect, and Effect.endEffect stops the stance of its own skill (Effect.java:721-725): SM_PLAYER_STANCE 0, once; :39-40 then
	// find no stance. The packet's own stopStance is the next case's
	EXPECT_EQ(journal, (et::Journal{"stance.end"}));
	EXPECT_EQ(actor.player->getEffectController()->findBySkillId(STANCE_SKILL), nullptr);
	EXPECT_FALSE(actor.player->getController().isUnderStance());
	EXPECT_EQ(actor.player->getController().getStanceSkillId(), 0);
	EXPECT_EQ(stancePackets(0), 1);
	EXPECT_FALSE(audit.contains("tried to remove")) << audit.dump();
}

TEST_F(ToggleSkillDeactivateRunTest, AStanceWithoutItsEffectIsStoppedByThePacket) {
	actor.player->getController().startStance(STANCE_SKILL); // the stance's effect is gone (or was never there): no effect end stops it
	(*client)->clearSent();

	EXPECT_NO_THROW(deactivate(STANCE_SKILL));

	// :37 finds no effect; :39-40, the stance of that skill: stopStance, SM_PLAYER_STANCE 0
	EXPECT_TRUE(journal.empty());
	EXPECT_FALSE(actor.player->getController().isUnderStance());
	EXPECT_EQ(stancePackets(0), 1);
}

TEST_F(ToggleSkillDeactivateRunTest, AToggleLeavesTheStanceOfAnotherSkill) {
	putOnThePlayer(TOGGLE_SKILL);
	actor.player->getController().startStance(STANCE_SKILL);
	(*client)->clearSent();

	EXPECT_NO_THROW(deactivate(TOGGLE_SKILL));

	// :39, getStanceSkillId() == skillId: the stance is another skill's, so it stays
	EXPECT_EQ(journal, (et::Journal{"toggle.end"}));
	EXPECT_EQ(actor.player->getController().getStanceSkillId(), STANCE_SKILL);
	EXPECT_EQ(stancePackets(0), 0);
}

} // namespace
} // namespace aion::gameserver::network::aion::clientpackets::testing
