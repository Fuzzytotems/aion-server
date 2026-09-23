// CM_CASTSPELL (P5-15, m5b2-plan.md P-02/P-05): C_USE_SKILL, the only packet with which a player starts a skill cast, and with skill id 0 the one
// with which he cancels it. It did not exist before M5b-2, so a real client's skill bar went to the factory's "not ported yet" warning.
//
// Java: game-server/src/com/aionemu/gameserver/network/aion/clientpackets/CM_CASTSPELL.java:36-109.
//
// readImpl: the byte vectors are laid out field by field from the Java readImpl, one per targetType arm (0/3/4: an object id, 1: a point,
// 2: a point and eight unknown floats, anything else: nothing), and every decoded field is compared, not only the byte count - the arms 0 and 1
// differ by eight bytes, but a swapped x/y or a hit time read as the level would consume exactly as many bytes as the right one.
//
// runImpl: every early return of the Java body is driven against a real Player and a real AionConnection. The body ends in
// PlayerController::useSkill, whose first statement is SkillEngine::getSkillFor - AION_UNPORTED until the cast lane ports it (m5b2-plan.md
// S-01). So, the way m5b-1's CM_ATTACK tests first proved the creature branch (AttackPacketTest.cpp), the arm that reaches the controller is
// asserted by the UnportedException of getSkillFor: that is "the packet reaches the engine", and it is all a unit test can see today.
// WHEN S-01 LANDS, SkillEngineReached below stops throwing and must become the real assertion: with the skill in the player's skill list
// (PlayerSkillList) getSkillFor answers a Skill, PlayerRestrictions::canUseSkill runs (tests/instance/PlayerRestrictionsTest.cpp) and the
// Skill carries targetType/x/y/z/clientHitTime from this packet; without it getSkillFor answers null and nothing happens.
//
// NOT COVERED: the `!player.getSummon().isPet()` half of the pet-order guard (a Summon needs an npc template and a spawn this fixture does
// not build; only the `getSummon() == null` half is driven), and the level, target arm and hit time as arguments of useSkill, which only
// S-01's getSkillFor can make observable - the read tests below assert that readImpl decoded them.

#include "InWorldPacketRunSupport.h"

#include <chrono>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

#include "aion/commons/utils/ByteBuffer.h"
#include "aion/commons/utils/TimeUtils.h"
#include "aion/gameserver/configs/main/LoggingConfig.h"
#include "aion/gameserver/configs/main/PunishmentConfig.h"
#include "aion/gameserver/controllers/ObserveController.h"
#include "aion/gameserver/controllers/PlayerController.h"
#include "aion/gameserver/controllers/observer/ItemUseObserver.h"
#include "aion/gameserver/dataholders/PetSkillData.bind.h"
#include "aion/gameserver/dataholders/PetSkillData.h"
#include "aion/gameserver/model/ActionState.h"
#include "aion/gameserver/model/ActionStateInfo.h"
#include "aion/gameserver/model/gameobjects/state/CreatureVisualState.h"
#include "aion/gameserver/model/skill/PlayerSkillList.h"
#include "aion/gameserver/network/aion/clientpackets/CM_CASTSPELL.h"
#include "aion/gameserver/network/aion/serverpackets/SM_PLAYER_STATE.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/skillengine/model/Skill.h"
#include "aion/gameserver/skillengine/model/SkillTemplate.h"
#include "aion/gameserver/utils/ChatUtil.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets {

/** The friend CM_CASTSPELL.h declares: read access to the fields readImpl decoded, which Java keeps private and never exposes */
struct CM_CASTSPELLTestAccess {
	static int64_t receiveTime(const CM_CASTSPELL& p) { return p.receiveTime; }
	static int32_t spellid(const CM_CASTSPELL& p) { return p.spellid; }
	static int32_t level(const CM_CASTSPELL& p) { return p.level; }
	static int32_t targetType(const CM_CASTSPELL& p) { return p.targetType; }
	static int32_t targetObjectId(const CM_CASTSPELL& p) { return p.targetObjectId; }
	static float x(const CM_CASTSPELL& p) { return p.x; }
	static float y(const CM_CASTSPELL& p) { return p.y; }
	static float z(const CM_CASTSPELL& p) { return p.z; }
	static int32_t hitTime(const CM_CASTSPELL& p) { return p.hitTime; }
	static int32_t unk(const CM_CASTSPELL& p) { return p.unk; }
};

namespace testing {
namespace {

using Access = CM_CASTSPELLTestAccess;
using model::gameobjects::state::CreatureVisualState;
using network::test::LogCapture;
using network::test::PacketWriter;
using serverpackets::SM_PLAYER_STATE;
using serverpackets::SM_SYSTEM_MESSAGE;

/** the decoded opcode of ClientPacketInfo.gen.inc:45 (Java AionClientPacketFactory: packets[33] = CM_CASTSPELL, State.IN_GAME) */
constexpr int32_t OPCODE = 33;

const char* BASE_CLIENT_PACKET_LOGGER = "com.aionemu.commons.network.packet.BaseClientPacket";
const char* AUDIT_LOGGER = "AUDIT_LOG"; // AuditLogger.cpp:22

constexpr int32_t ACTIVE_SKILL = 1282;  // Flame Bolt, the Mage's first cast
constexpr int32_t PASSIVE_SKILL = 40;   // a passive of every starting class
constexpr int32_t PET_ORDER_SKILL = 3835; // pet_skills.xml: the order skill of a pet
constexpr int32_t PREVIOUS_SKILL = 2864;  // the skill of the "previous skill" in the too-early audit line

/** readImpl up to the target arm: UH spell id, UC level, UC target type */
PacketWriter head(int32_t spellId, int32_t level, int32_t targetType) {
	PacketWriter w;
	w.H(spellId).C(level).C(targetType);
	return w;
}

/** targetType 0 (the same layout as 3 and 4): D target object id, then UH hit time and D unk */
std::vector<uint8_t> objectBody(int32_t spellId, int32_t level, int32_t targetObjectId, int32_t hitTime, int32_t targetType = 0) {
	return head(spellId, level, targetType).D(targetObjectId).H(hitTime).D(0x0BADF00D).data;
}

/** A fresh packet of type CM_CASTSPELL that has read `data`; nullptr if read() failed */
std::unique_ptr<CM_CASTSPELL> readPacket(const std::vector<uint8_t>& data, int32_t* unreadBytes = nullptr) {
	std::vector<uint8_t> copy = data;
	auto packet = std::make_unique<CM_CASTSPELL>(OPCODE, StateSet{AionConnection_State::IN_GAME});
	packet->setBuffer(commons::utils::ByteBuffer::wrap(copy));
	if (!packet->read())
		return nullptr;
	if (unreadBytes != nullptr)
		*unreadBytes = packet->getRemainingBytes();
	return packet;
}

/** Reads `data`, asserts that it was consumed exactly without a "Missing" log, and returns the packet */
std::unique_ptr<CM_CASTSPELL> readExactly(const std::vector<uint8_t>& data) {
	LogCapture capture({BASE_CLIENT_PACKET_LOGGER});
	int32_t unread = -1;
	std::unique_ptr<CM_CASTSPELL> packet = readPacket(data, &unread);
	EXPECT_NE(packet, nullptr);
	EXPECT_EQ(unread, 0);
	EXPECT_FALSE(capture.contains("Missing")) << capture.dump();
	return packet;
}

// -------------------------------------------------------------------------------------------------------------------------------- readImpl

TEST(CastSpellPacketReadTest, TheObjectArmsReadATargetObjectId) {
	for (int32_t targetType : {0, 3, 4}) {
		SCOPED_TRACE("targetType " + std::to_string(targetType));
		std::unique_ptr<CM_CASTSPELL> p = readExactly(objectBody(ACTIVE_SKILL, 3, 0x12345678, 1100, targetType));
		ASSERT_NE(p, nullptr);
		EXPECT_EQ(Access::spellid(*p), ACTIVE_SKILL);
		EXPECT_EQ(Access::level(*p), 3);
		EXPECT_EQ(Access::targetType(*p), targetType);
		EXPECT_EQ(Access::targetObjectId(*p), 0x12345678);
		EXPECT_EQ(Access::hitTime(*p), 1100);
		EXPECT_EQ(Access::unk(*p), 0x0BADF00D);
		EXPECT_EQ(Access::x(*p), 0.0f) << "no point in an object arm";
		EXPECT_EQ(Access::y(*p), 0.0f);
		EXPECT_EQ(Access::z(*p), 0.0f);
	}
}

TEST(CastSpellPacketReadTest, ThePointArmReadsThreeFloats) {
	std::unique_ptr<CM_CASTSPELL> p = readExactly(head(ACTIVE_SKILL, 1, 1).F(1.5f).F(-2.25f).F(100.125f).H(700).D(-1).data);
	ASSERT_NE(p, nullptr);
	EXPECT_EQ(Access::targetType(*p), 1);
	EXPECT_EQ(Access::x(*p), 1.5f);
	EXPECT_EQ(Access::y(*p), -2.25f);
	EXPECT_EQ(Access::z(*p), 100.125f);
	EXPECT_EQ(Access::targetObjectId(*p), 0) << "no object id in the point arm";
	EXPECT_EQ(Access::hitTime(*p), 700);
	EXPECT_EQ(Access::unk(*p), -1);
}

TEST(CastSpellPacketReadTest, TheSecondPointArmReadsAPointAndSkipsEightFloats) {
	PacketWriter w = head(ACTIVE_SKILL, 2, 2).F(10.0f).F(20.0f).F(30.0f);
	for (int i = 1; i <= 8; i++)
		w.F(1000.0f + static_cast<float>(i)); // unk1..unk8: read and dropped
	std::unique_ptr<CM_CASTSPELL> p = readExactly(w.H(2500).D(7).data);
	ASSERT_NE(p, nullptr);
	EXPECT_EQ(Access::targetType(*p), 2);
	EXPECT_EQ(Access::x(*p), 10.0f);
	EXPECT_EQ(Access::y(*p), 20.0f);
	EXPECT_EQ(Access::z(*p), 30.0f) << "the point is the first three floats, not the last three";
	EXPECT_EQ(Access::level(*p), 2);
	EXPECT_EQ(Access::hitTime(*p), 2500) << "the hit time follows the eighth unknown float";
	EXPECT_EQ(Access::unk(*p), 7);
}

TEST(CastSpellPacketReadTest, AnyOtherTargetTypeReadsNoTarget) {
	for (int32_t targetType : {5, 0x7F, 0xFF}) {
		SCOPED_TRACE("targetType " + std::to_string(targetType));
		std::unique_ptr<CM_CASTSPELL> p = readExactly(head(ACTIVE_SKILL, 1, targetType).H(900).D(11).data);
		ASSERT_NE(p, nullptr);
		EXPECT_EQ(Access::targetType(*p), targetType) << "readUC: unsigned";
		EXPECT_EQ(Access::targetObjectId(*p), 0);
		EXPECT_EQ(Access::hitTime(*p), 900) << "the switch has no default arm, the hit time comes right after the type";
		EXPECT_EQ(Access::unk(*p), 11);
	}
}

TEST(CastSpellPacketReadTest, TheUnsignedFieldsAreReadUnsigned) {
	std::unique_ptr<CM_CASTSPELL> p = readExactly(objectBody(0xFFFF, 0xFF, -2, 0xFFFF));
	ASSERT_NE(p, nullptr);
	EXPECT_EQ(Access::spellid(*p), 65535) << "readUH";
	EXPECT_EQ(Access::level(*p), 255) << "readUC";
	EXPECT_EQ(Access::targetObjectId(*p), -2) << "readD is signed";
	EXPECT_EQ(Access::hitTime(*p), 65535) << "readUH";
}

TEST(CastSpellPacketReadTest, AShortBodyUnderflowsAndASpareByteIsLeft) {
	{ // the second point arm one float short: the missing float shifts the hit time and the unk into underflow
		PacketWriter w = head(ACTIVE_SKILL, 1, 2);
		for (int i = 0; i < 10; i++)
			w.F(1.0f);
		LogCapture capture({BASE_CLIENT_PACKET_LOGGER});
		int32_t unread = -1;
		ASSERT_NE(readPacket(w.H(1).D(1).data, &unread), nullptr);
		EXPECT_EQ(unread, 0);
		EXPECT_TRUE(capture.contains("Missing")) << "11 floats are read in arm 2: " << capture.dump();
	}
	{ // the unk D is the last field: one byte more is left over
		std::vector<uint8_t> body = objectBody(ACTIVE_SKILL, 1, 7, 0);
		body.push_back(0);
		int32_t unread = -1;
		ASSERT_NE(readPacket(body, &unread), nullptr);
		EXPECT_EQ(unread, 1) << "readImpl reads nothing after unk";
	}
}

// --------------------------------------------------------------------------------------------------------------------------------- runImpl

/** One of Java's anonymous `new ItemUseObserver() { ... }` of a delayed item action, which PlayerController::cancelUseItem aborts */
class RecordingItemUseObserver final : public controllers::observer::ItemUseObserver {
	AION_MAKE_REF_FRIEND
public:
	int32_t aborted = 0;

	static runtime::Ref<RecordingItemUseObserver> create() { return runtime::makeRef<RecordingItemUseObserver>(); }

	void abort() override { aborted++; }

protected:
	RecordingItemUseObserver() = default;
	~RecordingItemUseObserver() override = default;
};

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

std::string skillTemplateXml(int32_t skillId, std::string_view activation) {
	return "<skill_template skill_id=\"" + std::to_string(skillId) + "\" name=\"s" + std::to_string(skillId) +
		R"(" nameId="1" skilltype="MAGICAL" skillsubtype="ATTACK" activation=")" + std::string(activation) + R"(" duration="0" stack="S)" +
		std::to_string(skillId) + "\"/>";
}

class CastSpellRunTest : public InWorldPacketTest {
protected:
	void SetUp() override {
		InWorldPacketTest::SetUp();
		xml::LoadContext context;
		dataholders::DataManager::SKILL_DATA.resetForTests(); // the fixture published an empty holder
		dataholders::DataManager::SKILL_DATA.publish(xml::bindString<dataholders::SkillData>(context,
			"<skill_data>" + skillTemplateXml(ACTIVE_SKILL, "ACTIVE") + skillTemplateXml(PASSIVE_SKILL, "PASSIVE") +
				skillTemplateXml(PET_ORDER_SKILL, "ACTIVE") + skillTemplateXml(PREVIOUS_SKILL, "ACTIVE") + "</skill_data>"));
		dataholders::DataManager::PET_SKILL_DATA.publish(xml::bindString<dataholders::PetSkillData>(context,
			R"(<pet_skill_templates><pet_skill skill_id="22107" pet_id="833288" order_skill=")" + std::to_string(PET_ORDER_SKILL) +
				R"("/></pet_skill_templates>)"));
		actor = makePlayer(310001, 9301, "Caster");
		actor.player->getPosition()->setIsSpawned(true);
		actor.player->setSkillList(model::skill::PlayerSkillList::create());
		client = std::make_unique<TestClient>();
		client->enterWorld(actor);
		(*client)->clearSent();
		runtime::resetUnportedHitsForTests();
	}

	void TearDown() override {
		if (actor.player) {
			actor.player->setCasting(nullptr); // a cast in progress holds the caster (Skill.effector): the cycle is cut here
			actor.player->setClientConnection(nullptr);
		}
		client.reset();
		actor = {};
		dataholders::DataManager::PET_SKILL_DATA.resetForTests();
		InWorldPacketTest::TearDown();
	}

	runtime::Ref<skillengine::model::Skill> skill(int32_t skillId) {
		return skillengine::model::Skill::create(dataholders::DataManager::SKILL_DATA->getSkillTemplate(skillId), *actor.player, nullptr, 1);
	}

	/** Reads and runs one CM_CASTSPELL, after dropping everything the arrangement queued */
	void cast(const std::vector<uint8_t>& body) {
		(*client)->clearSent();
		Driver<CM_CASTSPELL> packet(OPCODE);
		packet.readAndRun(body, client->get());
	}

	void cast(int32_t spellId) { cast(objectBody(spellId, 1, 0, 0)); }

	/** Runs `run` and returns the message of the UnportedException it must throw ("<function> is not ported yet (<file>:<line>)") */
	template <class F>
	std::string unportedMessage(F&& run) {
		try {
			run();
		} catch (const runtime::UnportedException& e) {
			return e.what();
		}
		ADD_FAILURE() << "no UnportedException";
		return {};
	}

	/** true if an AION_UNPORTED site in `file` whose function contains `function` was hit since SetUp */
	static bool unportedSiteHit(std::string_view file, std::string_view function) {
		for (const runtime::UnportedHit& hit : runtime::unportedHits())
			if (hit.hits > 0 && hit.file.find(file) != std::string::npos && hit.function.find(function) != std::string::npos)
				return true;
		return false;
	}

	static bool skillEngineReached() { return unportedSiteHit("skillengine/SkillEngine.cpp", "getSkillFor"); }

	/** Sets the last skill the way Java does: Player.setCasting remembers the template of the cast it replaces */
	void rememberLastSkill(int32_t skillId) {
		actor.player->setCasting(skill(skillId));
		actor.player->setCasting(nullptr);
		ASSERT_NE(actor.player->getLastSkill(), nullptr);
	}

	std::vector<uint8_t> message(SM_SYSTEM_MESSAGE&& packet) { return serialized(std::move(packet), client->con()); }

	std::vector<uint8_t> playerState() { return serialized(SM_PLAYER_STATE(*actor.player), client->con()); }

	PlayerFixture actor;
	std::unique_ptr<TestClient> client;
};

TEST_F(CastSpellRunTest, ADeadCasterIsToldSoAndNothingElseHappens) {
	actor.player->setLifeStats(std::make_unique<DeadPlayerLifeStats>(*actor.player));
	ASSERT_TRUE(actor.player->isDead());
	actor.player->setVisualState(CreatureVisualState::BLINKING);
	actor.player->setCasting(skill(ACTIVE_SKILL)); // spell id 0 would cancel it, which reaches the unported Skill::cancelCast

	EXPECT_NO_THROW(cast(0)) << "CM_CASTSPELL.java:77-80: the dead branch returns before the cancel";
	EXPECT_EQ((*client)->sentBytes(), exactly({message(SM_SYSTEM_MESSAGE::STR_SKILL_CANT_CAST(
										  utils::ChatUtil::l10n(getL10nId(model::ActionState::DEAD))))}));

	EXPECT_NO_THROW(cast(ACTIVE_SKILL)) << "and before the engine";
	EXPECT_TRUE(actor.player->isProtectionActive()) << "and before the protection task";
	EXPECT_FALSE(skillEngineReached());
}

TEST_F(CastSpellRunTest, SpellIdZeroCancelsTheCastInProgress) {
	actor.player->setCasting(skill(ACTIVE_SKILL));

	// Java: player.getController().cancelCurrentSkill(null), whose first step on a cast in progress is castingSkill.cancelCast() - a P5-02a body
	// (m5b2-plan.md S-02). Its UnportedException is the proof that the cancel was asked for; WHEN S-02 LANDS this case becomes the
	// SM_SKILL_CANCEL + STR_SKILL_CANCELED pair of PlayerController::cancelCurrentSkill.
	const std::string what = unportedMessage([&] { cast(0); });
	EXPECT_NE(what.find("Skill::cancelCast"), std::string::npos) << what;
	EXPECT_FALSE(skillEngineReached()) << "CM_CASTSPELL.java:82-85: spell id 0 returns after the cancel";
}

TEST_F(CastSpellRunTest, SpellIdZeroWithoutACastDoesNothing) {
	actor.player->setVisualState(CreatureVisualState::BLINKING);

	EXPECT_NO_THROW(cast(0));
	EXPECT_TRUE((*client)->sentBytes().empty()) << "cancelCurrentSkill returns at once without a cast (PlayerController.java:522-524)";
	EXPECT_TRUE(actor.player->isProtectionActive()) << "the spell id 0 branch returns before the protection task";
}

TEST_F(CastSpellRunTest, APetOrderSkillWithoutAPetIsRefused) {
	ASSERT_FALSE(actor.player->getSummon());
	ASSERT_NE(dataholders::DataManager::SKILL_DATA->getSkillTemplate(PET_ORDER_SKILL), nullptr)
		<< "the order skill has a template, so without the guard it would go on to the engine";
	actor.player->setVisualState(CreatureVisualState::BLINKING);

	EXPECT_NO_THROW(cast(PET_ORDER_SKILL));
	EXPECT_EQ((*client)->sentBytes(), exactly({message(SM_SYSTEM_MESSAGE::STR_SKILL_NOT_NEED_PET())})) << "CM_CASTSPELL.java:86-89";
	EXPECT_TRUE(actor.player->isProtectionActive());
	EXPECT_FALSE(skillEngineReached());
}

TEST_F(CastSpellRunTest, AnUnknownOrPassiveSkillIsIgnored) {
	actor.player->setVisualState(CreatureVisualState::BLINKING);
	ASSERT_EQ(dataholders::DataManager::SKILL_DATA->getSkillTemplate(9999), nullptr);
	ASSERT_TRUE(dataholders::DataManager::SKILL_DATA->getSkillTemplate(PASSIVE_SKILL)->isPassive());

	for (int32_t spellId : {9999, PASSIVE_SKILL}) {
		SCOPED_TRACE("skill " + std::to_string(spellId));
		EXPECT_NO_THROW(cast(spellId));
		EXPECT_TRUE((*client)->sentBytes().empty()) << "CM_CASTSPELL.java:91-93 returns without a word";
		EXPECT_TRUE(actor.player->isProtectionActive()) << "and before the protection task";
	}
	EXPECT_FALSE(skillEngineReached());
}

TEST_F(CastSpellRunTest, AnActiveSkillEndsTheProtectionCancelsTheItemUseAndReachesTheEngine) {
	actor.player->setVisualState(CreatureVisualState::BLINKING);
	runtime::Ref<RecordingItemUseObserver> itemUse = RecordingItemUseObserver::create();
	actor.player->getObserveController()->attach(*itemUse);

	const std::string what = unportedMessage([&] { cast(ACTIVE_SKILL); });

	// CM_CASTSPELL.java:95-97, then :108 -> PlayerController.useSkill -> SkillEngine.getSkillFor (m5b2-plan.md S-01, not ported yet)
	EXPECT_NE(what.find("SkillEngine::getSkillFor"), std::string::npos) << what;
	EXPECT_TRUE(skillEngineReached());
	EXPECT_FALSE(actor.player->isProtectionActive()) << "stopProtectionActiveTask";
	EXPECT_EQ((*client)->sentBytes(), exactly({playerState()})) << "stopProtectionActiveTask's SM_PLAYER_STATE and nothing else";
	EXPECT_EQ(itemUse->aborted, 1) << "cancelUseItem aborted the pending item use";
}

TEST_F(CastSpellRunTest, ACastBeforeTheNextSkillUseIsAuditedAndRefused) {
	rememberLastSkill(PREVIOUS_SKILL);
	actor.player->setVisualState(CreatureVisualState::BLINKING);
	AuditScope auditOn;
	LogCapture audit({AUDIT_LOGGER});
	Driver<CM_CASTSPELL> packet(OPCODE);
	const int64_t nextSkillUse = Access::receiveTime(packet) + 60000;
	actor.player->setNextSkillUse(nextSkillUse);
	(*client)->clearSent();

	EXPECT_NO_THROW(packet.readAndRun(objectBody(ACTIVE_SKILL, 1, 0, 0), client->get()));

	EXPECT_EQ(audit.count("tried to use skill " + std::to_string(ACTIVE_SKILL) + " 60000 ms too early. Previous skill: " +
				  std::to_string(PREVIOUS_SKILL)),
		1)
		<< audit.dump();
	// the protection task and the item-use cancel come first (CM_CASTSPELL.java:95-97), the refusal after them (:102-104)
	EXPECT_EQ((*client)->sentBytes(), exactly({playerState(), message(SM_SYSTEM_MESSAGE::STR_SKILL_NOT_READY())}));
	EXPECT_FALSE(skillEngineReached()) << "a refused cast never reaches the controller";
}

TEST_F(CastSpellRunTest, AnEarlyCastThatIsDueByNowIsAuditedAndGoesOn) {
	rememberLastSkill(PREVIOUS_SKILL);
	AuditScope auditOn;
	LogCapture audit({AUDIT_LOGGER});
	Driver<CM_CASTSPELL> packet(OPCODE);
	// the packet arrived 1 ms before the next skill use, and by the time it runs that moment has passed: Java logs the attempt and casts anyway
	const int64_t receiveTime = Access::receiveTime(packet);
	actor.player->setNextSkillUse(receiveTime + 1);
	while (commons::utils::currentTimeMillis() <= receiveTime + 1)
		std::this_thread::sleep_for(std::chrono::milliseconds(1));

	const std::string what = unportedMessage([&] { packet.readAndRun(objectBody(ACTIVE_SKILL, 1, 0, 0), client->get()); });

	EXPECT_TRUE(audit.contains("tried to use skill " + std::to_string(ACTIVE_SKILL) + " 1 ms too early")) << audit.dump();
	EXPECT_NE(what.find("SkillEngine::getSkillFor"), std::string::npos) << "CM_CASTSPELL.java:102 is false, so :108 runs: " << what;
	EXPECT_TRUE((*client)->sentBytes().empty()) << "no STR_SKILL_NOT_READY";
}

TEST_F(CastSpellRunTest, ACastAtTheNextSkillUseIsNotEarly) {
	AuditScope auditOn;
	LogCapture audit({AUDIT_LOGGER});
	Driver<CM_CASTSPELL> packet(OPCODE);
	actor.player->setNextSkillUse(Access::receiveTime(packet)); // `getNextSkillUse() > receiveTime` is false for equal times
	ASSERT_EQ(actor.player->getLastSkill(), nullptr) << "no last skill: the audit branch would throw NullPointerException";

	const std::string what = unportedMessage([&] { packet.readAndRun(objectBody(ACTIVE_SKILL, 1, 0, 0), client->get()); });

	EXPECT_NE(what.find("SkillEngine::getSkillFor"), std::string::npos) << what;
	EXPECT_FALSE(audit.contains("too early")) << audit.dump();
}

TEST_F(CastSpellRunTest, AnEarlyCastWithoutALastSkillThrowsLikeJava) {
	// "lastSkill cannot be null, as nextSkillUse is zero on the first cast" (CM_CASTSPELL.java:100): Java dereferences it anyway
	ASSERT_EQ(actor.player->getLastSkill(), nullptr);
	actor.player->setNextSkillUse(commons::utils::currentTimeMillis() + 60000);

	EXPECT_THROW(cast(ACTIVE_SKILL), runtime::NullPointerException);
	EXPECT_FALSE(skillEngineReached());
}

} // namespace
} // namespace testing
} // namespace aion::gameserver::network::aion::clientpackets
