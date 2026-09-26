// CM_CASTSPELL (P5-15, m5b2-plan.md P-02/P-05): C_USE_SKILL, the only packet with which a player starts a skill cast, and with skill id 0 the one
// with which he cancels it. It did not exist before M5b-2, so a real client's skill bar went to the factory's "not ported yet" warning.
//
// Java: game-server/src/com/aionemu/gameserver/network/aion/clientpackets/CM_CASTSPELL.java:36-109.
//
// readImpl: the byte vectors are laid out field by field from the Java readImpl, one per targetType arm (0/3/4: an object id, 1: a point,
// 2: a point and eight unknown floats, anything else: nothing), and every decoded field is compared, not only the byte count - the arms 0 and 1
// differ by eight bytes, but a swapped x/y or a hit time read as the level would consume exactly as many bytes as the right one.
//
// runImpl: every early return of the Java body is driven against a real Player and a real AionConnection, and since M5b-2 part 2 ported the
// cast engine (m5b2-plan.md S-01, S-02) so is the arm that goes on to PlayerController::useSkill (:108). The caster has learned ACTIVE_SKILL,
// a 2,000 ms self cast: SkillEngine::getSkillFor answers a Skill, PlayerRestrictions::canUseSkill lets it through (that body's own cases are
// tests/instance/PlayerRestrictionsTest.cpp) and Skill::useSkill starts the cast - the caster is casting and SM_CASTSPELL is sent now, the end
// task waits on the fixture's manual clock, which no case here advances (the whole cast is tests/skills/P5-02a's). A skill the caster has not
// learned makes getSkillFor answer null, and nothing follows the protection and item-use statements. Spell id 0 cancels the cast in progress
// with SM_SKILL_CANCEL and STR_SKILL_CANCELED (PlayerController.cancelCurrentSkill).
//
// NOT COVERED: the `!player.getSummon().isPet()` half of the pet-order guard (a Summon needs an npc template and a spawn this fixture does
// not build; only the `getSummon() == null` half is driven), and useSkill's transform arm, the only reader of the packet's level (the FORM1
// panel lookup, PlayerController.java:460-466) - the level of a normal cast is the skill list's, which the start case asserts.

#include "InWorldPacketRunSupport.h"

#include <chrono>
#include <cstdint>
#include <memory>
#include <optional>
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
#include "aion/gameserver/model/gameobjects/Persistable.h"
#include "aion/gameserver/model/gameobjects/state/CreatureState.h"
#include "aion/gameserver/model/gameobjects/state/CreatureVisualState.h"
#include "aion/gameserver/model/skill/PlayerSkillEntry.h"
#include "aion/gameserver/model/skill/PlayerSkillList.h"
#include "aion/gameserver/network/aion/clientpackets/CM_CASTSPELL.h"
#include "aion/gameserver/network/aion/serverpackets/SM_CASTSPELL.h"
#include "aion/gameserver/network/aion/serverpackets/SM_PLAYER_STATE.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SKILL_CANCEL.h"
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
using model::gameobjects::state::CreatureState;
using model::gameobjects::state::CreatureVisualState;
using network::test::LogCapture;
using network::test::PacketWriter;
using serverpackets::SM_CASTSPELL;
using serverpackets::SM_PLAYER_STATE;
using serverpackets::SM_SKILL_CANCEL;
using serverpackets::SM_SYSTEM_MESSAGE;
using skillengine::model::Skill;

/** the decoded opcode of ClientPacketInfo.gen.inc:45 (Java AionClientPacketFactory: packets[33] = CM_CASTSPELL, State.IN_GAME) */
constexpr int32_t OPCODE = 33;

const char* BASE_CLIENT_PACKET_LOGGER = "com.aionemu.commons.network.packet.BaseClientPacket";
const char* AUDIT_LOGGER = "AUDIT_LOG"; // AuditLogger.cpp:22

constexpr int32_t ACTIVE_SKILL = 1282;  // the id of Flame Bolt, the Mage's first cast; its template below is a 2,000 ms self cast (no target to find)
constexpr int32_t PASSIVE_SKILL = 40;   // a passive of every starting class
constexpr int32_t PET_ORDER_SKILL = 3835; // pet_skills.xml: the order skill of a pet
constexpr int32_t PREVIOUS_SKILL = 2864;  // the skill of the "previous skill" in the too-early audit line; an ACTIVE skill the caster has not learned
constexpr int32_t LEARNED_LEVEL = 2;      // ACTIVE_SKILL's level in the caster's skill list; every packet below says level 1
constexpr int32_t CAST_DURATION = 2000;   // ACTIVE_SKILL's duration, the SM_CASTSPELL cast time without a cast speed modifier

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

/** targetType 1: F x, F y, F z, then UH hit time and D unk */
std::vector<uint8_t> pointBody(int32_t spellId, int32_t level, float x, float y, float z, int32_t hitTime) {
	return head(spellId, level, 1).F(x).F(y).F(z).H(hitTime).D(0x0BADF00D).data;
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

/**
 * One of Java's anonymous `new ItemUseObserver() { ... }` of a delayed item action, which PlayerController::cancelUseItem aborts. It also
 * records whether its owner's spawn protection was still on when it was aborted: a real one sends nothing a test could order against
 * stopProtectionActiveTask's SM_PLAYER_STATE, so this is how the order of CM_CASTSPELL.java:95-97 is seen.
 */
class RecordingItemUseObserver final : public controllers::observer::ItemUseObserver {
	AION_MAKE_REF_FRIEND
public:
	int32_t aborted = 0;
	/** the owner's isProtectionActive() at the last abort(); empty while it was never aborted */
	std::optional<bool> protectionActiveAtAbort;

	static runtime::Ref<RecordingItemUseObserver> create(model::gameobjects::player::Player& owner) {
		return runtime::makeRef<RecordingItemUseObserver>(owner);
	}

	void abort() override {
		aborted++;
		protectionActiveAtAbort = owner.isProtectionActive();
	}

protected:
	explicit RecordingItemUseObserver(model::gameobjects::player::Player& ownerValue) : owner(ownerValue) {}
	~RecordingItemUseObserver() override = default;

private:
	model::gameobjects::player::Player& owner; // attached to the owner's ObserveController, which the owner outlives
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

std::string skillTemplateXml(int32_t skillId, std::string_view activation, int32_t duration = 0, std::string_view children = {}) {
	return "<skill_template skill_id=\"" + std::to_string(skillId) + "\" name=\"s" + std::to_string(skillId) +
		R"(" nameId="1" skilltype="MAGICAL" skillsubtype="ATTACK" activation=")" + std::string(activation) + R"(" duration=")" +
		std::to_string(duration) + R"(" stack="S)" + std::to_string(skillId) + "\">" + std::string(children) + "</skill_template>";
}

class CastSpellRunTest : public InWorldPacketTest {
protected:
	void SetUp() override {
		InWorldPacketTest::SetUp();
		xml::LoadContext context;
		dataholders::DataManager::SKILL_DATA.resetForTests(); // the fixture published an empty holder
		// ACTIVE_SKILL: first_target ME, so FirstTargetProperty makes the caster the target and the cast needs no other creature
		// (FirstTargetProperty.java:22-25); the templates without <properties> have an empty effected list, which the object arm refuses
		dataholders::DataManager::SKILL_DATA.publish(xml::bindString<dataholders::SkillData>(context,
			"<skill_data>" +
				skillTemplateXml(ACTIVE_SKILL, "ACTIVE", CAST_DURATION, R"(<properties first_target="ME" target_type="ONLYONE"/>)") +
				skillTemplateXml(PASSIVE_SKILL, "PASSIVE") + skillTemplateXml(PET_ORDER_SKILL, "ACTIVE") +
				skillTemplateXml(PREVIOUS_SKILL, "ACTIVE") + "</skill_data>"));
		dataholders::DataManager::PET_SKILL_DATA.publish(xml::bindString<dataholders::PetSkillData>(context,
			R"(<pet_skill_templates><pet_skill skill_id="22107" pet_id="833288" order_skill=")" + std::to_string(PET_ORDER_SKILL) +
				R"("/></pet_skill_templates>)"));
		actor = makePlayer(310001, 9301, "Caster");
		actor.player->getPosition()->setIsSpawned(true);
		// the caster's one learned skill (Java PlayerSkillListDAO rows): SkillEngine.getSkillFor answers null for every other ACTIVE skill
		learned = model::skill::PlayerSkillEntry::create(ACTIVE_SKILL, LEARNED_LEVEL, 0, model::gameobjects::Persistable_PersistentState::NOACTION);
		actor.player->setSkillList(
			model::skill::PlayerSkillList::create({runtime::Ptr<model::skill::PlayerSkillEntry>(learned)}));
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
		learned.reset();
		dataholders::DataManager::PET_SKILL_DATA.resetForTests();
		InWorldPacketTest::TearDown();
	}

	runtime::Ref<Skill> skill(int32_t skillId) {
		return Skill::create(dataholders::DataManager::SKILL_DATA->getSkillTemplate(skillId), *actor.player, nullptr, 1);
	}

	/** Reads and runs one CM_CASTSPELL, after dropping everything the arrangement queued */
	void cast(const std::vector<uint8_t>& body) {
		(*client)->clearSent();
		Driver<CM_CASTSPELL> packet(OPCODE);
		packet.readAndRun(body, client->get());
	}

	void cast(int32_t spellId) { cast(objectBody(spellId, 1, 0, 0)); }

	/**
	 * true if the packet went on to PlayerController::useSkill and started the cast of ACTIVE_SKILL: Skill.useSkill has made it the caster's
	 * casting skill (Skill.java:289), and its end task waits on the manual clock
	 */
	bool castStarted() {
		runtime::Ptr<Skill> casting = actor.player->getCastingSkill();
		return casting && casting->getSkillId() == ACTIVE_SKILL;
	}

	/**
	 * The SM_CASTSPELL startCast broadcasts to the caster for the cast in progress of an object-arm packet (Skill.java:495-503): the learned
	 * level, the caster himself as the target of a first_target ME skill, and the template's duration. The two animation fields are the
	 * started Skill's own; nothing here sets a cast speed.
	 */
	std::vector<uint8_t> castSpellOfTheCastInProgress() {
		runtime::Ptr<Skill> casting = actor.player->getCastingSkill();
		if (!casting)
			return {};
		return serialized(SM_CASTSPELL(*actor.player, ACTIVE_SKILL, LEARNED_LEVEL, 0, actor.player->getObjectId(), CAST_DURATION,
							  casting->getCastSpeedForAnimationBoostAndChargeSkills(), casting->allowAnimationBoostByCastSpeed()),
			client->con());
	}

	/** Sets the last skill the way Java does: Player.setCasting remembers the template of the cast it replaces */
	void rememberLastSkill(int32_t skillId) {
		actor.player->setCasting(skill(skillId));
		actor.player->setCasting(nullptr);
		ASSERT_NE(actor.player->getLastSkill(), nullptr);
	}

	std::vector<uint8_t> message(SM_SYSTEM_MESSAGE&& packet) { return serialized(std::move(packet), client->con()); }

	std::vector<uint8_t> playerState() { return serialized(SM_PLAYER_STATE(*actor.player), client->con()); }

	/** A pending delayed item use of the caster, as ItemUseObserver: what PlayerController::cancelUseItem aborts (CM_CASTSPELL.java:97) */
	runtime::Ref<RecordingItemUseObserver> pendingItemUse() {
		runtime::Ref<RecordingItemUseObserver> itemUse = RecordingItemUseObserver::create(*actor.player);
		actor.player->getObserveController()->attach(*itemUse);
		return itemUse;
	}

	PlayerFixture actor;
	std::unique_ptr<TestClient> client;
	runtime::Ref<model::skill::PlayerSkillEntry> learned;
};

TEST_F(CastSpellRunTest, ADeadCasterIsToldSoAndNothingElseHappens) {
	actor.player->setLifeStats(std::make_unique<DeadPlayerLifeStats>(*actor.player));
	ASSERT_TRUE(actor.player->isDead());
	actor.player->setVisualState(CreatureVisualState::BLINKING);
	runtime::Ref<Skill> inProgress = skill(PREVIOUS_SKILL);
	actor.player->setCasting(inProgress); // spell id 0 would cancel it
	runtime::Ref<RecordingItemUseObserver> itemUse = pendingItemUse();
	const std::vector<uint8_t> dead = message(SM_SYSTEM_MESSAGE::STR_SKILL_CANT_CAST(utils::ChatUtil::l10n(getL10nId(model::ActionState::DEAD))));

	EXPECT_NO_THROW(cast(0)) << "CM_CASTSPELL.java:77-80: the dead branch returns before the cancel";
	EXPECT_EQ((*client)->sentBytes(), exactly({dead})) << "no SM_SKILL_CANCEL";
	EXPECT_EQ(actor.player->getCastingSkill(), inProgress) << "the cast in progress is still the caster's";

	EXPECT_NO_THROW(cast(ACTIVE_SKILL)) << "and before the engine";
	EXPECT_EQ((*client)->sentBytes(), exactly({dead}));
	EXPECT_TRUE(actor.player->isProtectionActive()) << "and before the protection task";
	EXPECT_EQ(itemUse->aborted, 0) << "and before the item-use cancel";
	EXPECT_EQ(actor.player->getCastingSkill(), inProgress) << "and no cast of ACTIVE_SKILL started";
}

TEST_F(CastSpellRunTest, SpellIdZeroCancelsTheCastInProgress) {
	cast(ACTIVE_SKILL);
	ASSERT_TRUE(castStarted()) << "the 2,000 ms cast of AnActiveSkillEndsTheProtectionCancelsTheItemUseAndReachesTheEngine, waiting for its end";

	EXPECT_NO_THROW(cast(0));

	// CM_CASTSPELL.java:82-85: player.getController().cancelCurrentSkill(null) -> castingSkill.cancelCast(), player.setCasting(null), and for a
	// CAST skill SM_SKILL_CANCEL to the caster and everyone who sees him, then the message (PlayerController.java:514-541)
	EXPECT_FALSE(actor.player->isCasting());
	EXPECT_EQ((*client)->sentBytes(),
		exactly({serialized(SM_SKILL_CANCEL(*actor.player, ACTIVE_SKILL), client->con()), message(SM_SYSTEM_MESSAGE::STR_SKILL_CANCELED())}));
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
	runtime::Ref<RecordingItemUseObserver> itemUse = pendingItemUse();

	EXPECT_NO_THROW(cast(PET_ORDER_SKILL));
	EXPECT_EQ((*client)->sentBytes(), exactly({message(SM_SYSTEM_MESSAGE::STR_SKILL_NOT_NEED_PET())})) << "CM_CASTSPELL.java:86-89";
	EXPECT_TRUE(actor.player->isProtectionActive());
	EXPECT_EQ(itemUse->aborted, 0) << "the pet guard returns before the item-use cancel";
	EXPECT_FALSE(actor.player->isCasting());
}

TEST_F(CastSpellRunTest, AnUnknownOrPassiveSkillIsIgnored) {
	actor.player->setVisualState(CreatureVisualState::BLINKING);
	ASSERT_EQ(dataholders::DataManager::SKILL_DATA->getSkillTemplate(9999), nullptr);
	ASSERT_TRUE(dataholders::DataManager::SKILL_DATA->getSkillTemplate(PASSIVE_SKILL)->isPassive());
	runtime::Ref<RecordingItemUseObserver> itemUse = pendingItemUse();

	for (int32_t spellId : {9999, PASSIVE_SKILL}) {
		SCOPED_TRACE("skill " + std::to_string(spellId));
		EXPECT_NO_THROW(cast(spellId));
		EXPECT_TRUE((*client)->sentBytes().empty()) << "CM_CASTSPELL.java:91-93 returns without a word";
		EXPECT_TRUE(actor.player->isProtectionActive()) << "and before the protection task";
		EXPECT_EQ(itemUse->aborted, 0) << "and before the item-use cancel (:97)";
	}
	EXPECT_FALSE(actor.player->isCasting());
}

TEST_F(CastSpellRunTest, AnActiveSkillEndsTheProtectionCancelsTheItemUseAndReachesTheEngine) {
	actor.player->setVisualState(CreatureVisualState::BLINKING);
	runtime::Ref<RecordingItemUseObserver> itemUse = pendingItemUse();

	// the point arm, so the SM_CASTSPELL below carries the x/y/z this packet brought
	EXPECT_NO_THROW(cast(pointBody(ACTIVE_SKILL, 1, 1.5f, -2.25f, 100.125f, 700)));

	// CM_CASTSPELL.java:95-97: the protection ends, and only then is the item use cancelled
	EXPECT_FALSE(actor.player->isProtectionActive()) << "stopProtectionActiveTask";
	EXPECT_EQ(itemUse->aborted, 1) << "cancelUseItem aborted the pending item use";
	EXPECT_EQ(itemUse->protectionActiveAtAbort, std::optional<bool>(false)) << "stopProtectionActiveTask runs before cancelUseItem";
	// :108 -> PlayerController.useSkill (PlayerController.java:458-481): getSkillFor answers the learned skill, canUseSkill lets it through,
	// setTargetType/setClientHitTime hand it this packet's target and hit time, and Skill.useSkill starts the cast (Skill.java:274-319)
	ASSERT_TRUE(castStarted());
	runtime::Ptr<Skill> started = actor.player->getCastingSkill();
	EXPECT_EQ(started->getSkillLevel(), LEARNED_LEVEL) << "new Skill(template, player, target) takes the skill list's level, not the packet's 1";
	EXPECT_EQ(started->getHitTime(), 700) << "the client's hit time: gameserver.security.check_animations is off here (Skill.java:416-418)";
	EXPECT_EQ((*client)->sentBytes(),
		exactly({playerState(), serialized(SM_CASTSPELL(*actor.player, ACTIVE_SKILL, LEARNED_LEVEL, 1, 1.5f, -2.25f, 100.125f, CAST_DURATION,
										 started->getCastSpeedForAnimationBoostAndChargeSkills(), started->allowAnimationBoostByCastSpeed()),
									client->con())}))
		<< "stopProtectionActiveTask's SM_PLAYER_STATE, then startCast's SM_CASTSPELL of the point arm (Skill.java:515-518)";
	EXPECT_EQ(runtime::unportedHitCount(), 0u) << "starting a cast reaches no unported body";
}

TEST_F(CastSpellRunTest, ASkillTheCasterHasNotLearnedEndsInTheEngine) {
	actor.player->setVisualState(CreatureVisualState::BLINKING);
	ASSERT_FALSE(actor.player->getSkillList()->isSkillPresent(PREVIOUS_SKILL));
	runtime::Ref<RecordingItemUseObserver> itemUse = pendingItemUse();

	EXPECT_NO_THROW(cast(PREVIOUS_SKILL));

	// the template exists and is active, so :91-93 lets it through and :95-97 run; then SkillEngine.getSkillFor answers null for a skill that is
	// not PROVOKED and not in the skill list (SkillEngine.java:56-61), the caster is not transformed, and useSkill ends without a word
	// (PlayerController.java:460-468)
	EXPECT_FALSE(actor.player->isProtectionActive());
	EXPECT_EQ(itemUse->aborted, 1);
	EXPECT_EQ((*client)->sentBytes(), exactly({playerState()})) << "no SM_CASTSPELL and no refusal";
	EXPECT_FALSE(actor.player->isCasting());
}

TEST_F(CastSpellRunTest, ALearnedSkillIsStillAskedToPlayerRestrictions) {
	actor.player->setState(CreatureState::RESTING); // Creature.canAttack is false while resting; Skill.canUseSkill does not look at it
	ASSERT_FALSE(actor.player->canAttack());

	EXPECT_NO_THROW(cast(ACTIVE_SKILL));

	// PlayerController.java:473-474: `if (!PlayerRestrictions.canUseSkill(player, skill)) return;` between getSkillFor and useSkill - the
	// refusal is PlayerRestrictions.java:72-75's, and no cast starts
	EXPECT_EQ((*client)->sentBytes(), exactly({message(SM_SYSTEM_MESSAGE::STR_SKILL_CAN_NOT_ATTACK_WHILE_IN_ABNORMAL_STATE())}));
	EXPECT_FALSE(actor.player->isCasting());
}

TEST_F(CastSpellRunTest, ACastBeforeTheNextSkillUseIsAuditedAndRefused) {
	rememberLastSkill(PREVIOUS_SKILL);
	actor.player->setVisualState(CreatureVisualState::BLINKING);
	runtime::Ref<RecordingItemUseObserver> itemUse = pendingItemUse();
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
	EXPECT_EQ(itemUse->aborted, 1) << "a refused cast has still cancelled the pending item use: cancelUseItem runs before the too-early check";
	EXPECT_FALSE(actor.player->isCasting()) << "a refused cast never reaches the controller";
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
	(*client)->clearSent();

	EXPECT_NO_THROW(packet.readAndRun(objectBody(ACTIVE_SKILL, 1, 0, 0), client->get()));

	EXPECT_TRUE(audit.contains("tried to use skill " + std::to_string(ACTIVE_SKILL) + " 1 ms too early")) << audit.dump();
	ASSERT_TRUE(castStarted()) << "CM_CASTSPELL.java:102 is false, so :108 runs and the cast starts";
	EXPECT_EQ((*client)->sentBytes(), exactly({castSpellOfTheCastInProgress()})) << "SM_CASTSPELL and no STR_SKILL_NOT_READY";
}

TEST_F(CastSpellRunTest, ACastAtTheNextSkillUseIsNotEarly) {
	AuditScope auditOn;
	LogCapture audit({AUDIT_LOGGER});
	Driver<CM_CASTSPELL> packet(OPCODE);
	actor.player->setNextSkillUse(Access::receiveTime(packet)); // `getNextSkillUse() > receiveTime` is false for equal times
	ASSERT_EQ(actor.player->getLastSkill(), nullptr) << "no last skill: the audit branch would throw NullPointerException";

	EXPECT_NO_THROW(packet.readAndRun(objectBody(ACTIVE_SKILL, 1, 0, 0), client->get()));

	EXPECT_FALSE(audit.contains("too early")) << audit.dump();
	ASSERT_TRUE(castStarted()) << ":99 is false, so :108 runs and the cast starts";
	EXPECT_EQ((*client)->sentBytes(), exactly({castSpellOfTheCastInProgress()}));
}

TEST_F(CastSpellRunTest, AnEarlyCastWithoutALastSkillThrowsLikeJava) {
	// "lastSkill cannot be null, as nextSkillUse is zero on the first cast" (CM_CASTSPELL.java:100): Java dereferences it anyway
	ASSERT_EQ(actor.player->getLastSkill(), nullptr);
	actor.player->setNextSkillUse(commons::utils::currentTimeMillis() + 60000);

	EXPECT_THROW(cast(ACTIVE_SKILL), runtime::NullPointerException);
	EXPECT_FALSE(actor.player->isCasting());
}

} // namespace
} // namespace testing
} // namespace aion::gameserver::network::aion::clientpackets
