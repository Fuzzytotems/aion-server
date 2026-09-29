// CM_ATTACK and CM_HEADING_UPDATE (P5-15, m5b-plan.md C-02): the two A-K client packets a fighting client sends that the server did not have.
// CM_ATTACK is the packet the whole melee path hangs from; CM_HEADING_UPDATE is the spin packet the client sends beside it and that Java
// answers with nothing at all.
//
// Java: game-server/src/com/aionemu/gameserver/network/aion/clientpackets/CM_ATTACK.java:36-59 and CM_HEADING_UPDATE.java:17-23.
//
// The byte vectors below are laid out field by field from the Java readImpl, and read() must consume them exactly (the base class logs
// "Missing X" on underflow). The run tests drive runImpl against real Players, a real KnownList and a real AionConnection, as the
// CM_TARGET_SELECT tests of the last wave do.

#include "InWorldPacketRunSupport.h"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "aion/commons/utils/ByteBuffer.h"
#include "aion/commons/utils/TimeUtils.h"
#include "aion/gameserver/configs/main/GeoDataConfig.h"
#include "aion/gameserver/controllers/PlayerController.h"
#include "aion/gameserver/controllers/VisibleObjectController.h"
#include "aion/gameserver/controllers/attack/AggroList.h"
#include "aion/gameserver/model/animations/AttackHandAnimation.h"
#include "aion/gameserver/model/animations/AttackTypeAnimation.h"
#include "aion/gameserver/model/gameobjects/Item.h"
#include "aion/gameserver/model/gameobjects/Persistable_PersistentState.h"
#include "aion/gameserver/model/gameobjects/player/CustomPlayerState.h"
#include "aion/gameserver/model/gameobjects/player/Equipment.h"
#include "aion/gameserver/model/gameobjects/state/CreatureVisualState.h"
#include "aion/gameserver/model/items/ItemSlotInfo.h"
#include "aion/gameserver/model/skill/PlayerSkillEntry.h"
#include "aion/gameserver/model/skill/PlayerSkillList.h"
#include "aion/gameserver/model/stats/calc/Stat2.h"
#include "aion/gameserver/model/stats/container/PlayerGameStats.h"
#include "aion/gameserver/model/templates/item/ItemTemplate.bind.h"
#include "aion/gameserver/model/templates/item/ItemTemplate.h"
#include "aion/gameserver/network/aion/clientpackets/CM_ATTACK.h"
#include "aion/gameserver/network/aion/clientpackets/CM_HEADING_UPDATE.h"
#include "aion/gameserver/network/aion/serverpackets/SM_ATTACK.h"
#include "aion/gameserver/network/aion/serverpackets/SM_ATTACK_RESPONSE.h"
#include "aion/gameserver/network/aion/serverpackets/SM_PLAYER_STATE.h"
#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/world/WorldPosition.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets::testing {
namespace {

using model::gameobjects::state::CreatureVisualState;
using network::test::LogCapture;
using network::test::PacketWriter;
using serverpackets::SM_ATTACK_RESPONSE;
using serverpackets::SM_PLAYER_STATE;

/** the decoded opcodes of ClientPacketInfo.gen.inc:44 and :130 */
constexpr int32_t ATTACK_OPCODE = 32;
constexpr int32_t HEADING_UPDATE_OPCODE = 147;

const char* BASE_CLIENT_PACKET_LOGGER = "com.aionemu.commons.network.packet.BaseClientPacket";
const char* CM_ATTACK_LOGGER = "com.aionemu.gameserver.network.aion.clientpackets.CM_ATTACK";

/** VisibleObjectController with a public constructor (the base one is protected), as tests/world's RecordingController has it */
class PlainController final : public controllers::VisibleObjectController {
public:
	PlainController() = default;
};

/** A known object that is not a Creature, so CM_ATTACK's `obj instanceof Creature` is false and the warn branch is taken */
class PlainObject final : public model::gameobjects::VisibleObject {
	AION_MAKE_REF_FRIEND
public:
	static runtime::Ref<PlainObject> create(int32_t objectId) { return VisibleObject::create<PlainObject>(objectId); }

	PlainObject(CreateKey key, int32_t objectId)
		: VisibleObject(key, objectId, std::make_unique<PlainController>(), nullptr, nullptr, world::WorldPosition::create(210010000), false) {}

	std::string getName() override { return "PlainObject" + std::to_string(getObjectId()); }

protected:
	~PlainObject() override = default;

	void postConstruct() override {
		VisibleObject::postConstruct();
		getController().setOwner(*this);
		setKnownlist(std::make_unique<world::knownlist::KnownList>(*this));
	}
};

/** Java CM_ATTACK.readImpl: D target object id, UC attackno, UH time, UC type */
std::vector<uint8_t> attackBody(int32_t targetObjectId, int32_t attackno, int32_t time, int32_t type) {
	return PacketWriter().D(targetObjectId).C(attackno).H(time).C(type).data;
}

/** Reads `data` with a fresh packet of type P (no connection) and returns the unread byte count, -1 if read() threw.
 *  An underflowing field does NOT fail read(): ClientPacketBase logs "Missing <type>" and the reader returns a default (BaseClientPacket.cpp:36-61). */
template <class P>
int32_t unreadBytesAfterRead(const std::vector<uint8_t>& data, int32_t opcode) {
	std::vector<uint8_t> copy = data;
	auto packet = std::make_unique<P>(opcode, StateSet{AionConnection_State::IN_GAME});
	packet->setBuffer(commons::utils::ByteBuffer::wrap(copy));
	if (!packet->read())
		return -1;
	return packet->getRemainingBytes();
}

/** Asserts that P consumes `data` exactly and logs no missing field */
template <class P>
void expectExactRead(const std::vector<uint8_t>& data, int32_t opcode) {
	LogCapture capture({BASE_CLIENT_PACKET_LOGGER});
	EXPECT_EQ(unreadBytesAfterRead<P>(data, opcode), 0) << typeid(P).name();
	EXPECT_FALSE(capture.contains("Missing")) << capture.dump();
}

TEST(AttackPacketReadTest, AttackAndHeadingUpdateBodies) {
	// CM_ATTACK: 4 + 1 + 2 + 1 = 8 bytes, never more and never less
	expectExactRead<CM_ATTACK>(attackBody(0x12345678, 3, 1500, 1), ATTACK_OPCODE);
	expectExactRead<CM_ATTACK>(attackBody(0, 0, 0, 0), ATTACK_OPCODE);
	expectExactRead<CM_ATTACK>(attackBody(-1, 0xFF, 0xFFFF, 0xFF), ATTACK_OPCODE);
	{ // one byte short: readUC of `type` underflows, which ClientPacketBase reports as "Missing C" (BaseClientPacket.cpp:60-61)
		LogCapture capture({BASE_CLIENT_PACKET_LOGGER});
		EXPECT_EQ(unreadBytesAfterRead<CM_ATTACK>(PacketWriter().D(7).C(0).H(0).data, ATTACK_OPCODE), 0);
		EXPECT_TRUE(capture.contains("Missing C")) << capture.dump();
	}
	std::vector<uint8_t> oneSpare = attackBody(7, 0, 0, 0);
	oneSpare.push_back(0);
	EXPECT_EQ(unreadBytesAfterRead<CM_ATTACK>(oneSpare, ATTACK_OPCODE), 1) << "readImpl reads four fields and no fifth";

	// CM_HEADING_UPDATE: a single C, and nothing else
	expectExactRead<CM_HEADING_UPDATE>(PacketWriter().C(0x5A).data, HEADING_UPDATE_OPCODE);
	{ // an empty body must underflow on the single readC
		LogCapture capture({BASE_CLIENT_PACKET_LOGGER});
		EXPECT_EQ(unreadBytesAfterRead<CM_HEADING_UPDATE>({}, HEADING_UPDATE_OPCODE), 0);
		EXPECT_TRUE(capture.contains("Missing C")) << capture.dump();
	}
}

class AttackRunTest : public InWorldPacketTest {
protected:
	void SetUp() override {
		InWorldPacketTest::SetUp();
		// the unit tests never load geo data; with gameserver.geodata.cansee.enable off GeoService::canSee answers true at its first statement
		// (GeoService.cpp:117-119), which PlayerController::attackTarget asks before it hits (PlayerController.java:411)
		canSeeEnabled = configs::main::GeoDataConfig::CANSEE_ENABLE.exchange(false);
		actor = makePlayer(300001, 9201, "Attacker");
		victim = makePlayer(300002, 9202, "Victim");
		actor.player->getPosition()->setIsSpawned(true);
		victim.player->getPosition()->setIsSpawned(true);
		client = std::make_unique<TestClient>();
		client->enterWorld(actor);
	}

	void TearDown() override {
		if (actor.player) {
			actor.player->setTarget(nullptr);
			actor.player->setClientConnection(nullptr);
		}
		client.reset();
		weapons.clear();
		actor = {};
		victim = {};
		configs::main::GeoDataConfig::CANSEE_ENABLE.store(canSeeEnabled);
		InWorldPacketTest::TearDown();
	}

	/** Reads and runs one CM_ATTACK, after dropping everything the setup queued */
	void attack(int32_t targetObjectId, int32_t time = 0) {
		(*client)->clearSent();
		Driver<CM_ATTACK> packet(ATTACK_OPCODE);
		packet.readAndRun(attackBody(targetObjectId, 1, time, 0), client->get());
	}

	/**
	 * Flags the victim as an enemy of all players, the one lever that makes `Player.isEnemyFrom(Player)` answer true without a PvP zone, a duel
	 * or a second race (Player.java:684-686), so PlayerRestrictions.canAttack reaches its `return player.isEnemy(target)` with true.
	 */
	void makeEnemy() {
		victim.player->setCustomState(model::gameobjects::player::CustomPlayerState::ENEMY_OF_ALL_PLAYERS);
		ASSERT_TRUE(actor.player->isEnemy(*victim.player));
	}

	/** The opcodes of everything sendPacket queued on the attacker's connection, in order */
	std::vector<int32_t> sentOpcodes() {
		std::vector<int32_t> opcodes;
		for (const SerializedBody& body : (*client)->sent())
			opcodes.push_back(body.opCode);
		return opcodes;
	}

	/** SM_ATTACK's opcode, read off a packet object rather than hardcoded (the opcode tables are generated) */
	int32_t attackOpcode() const {
		return serverpackets::SM_ATTACK(*actor.player, *victim.player, 0, 0, model::animations::AttackTypeAnimation::MELEE,
			model::animations::AttackHandAnimation::MAIN_HAND, {})
			.getOpCode();
	}

	int32_t attackResponseOpcode() const { return SM_ATTACK_RESPONSE::STOP_WITHOUT_MESSAGE(0).getOpCode(); }

	/** The attacker and the victim know each other, and the victim is an enemy in attack range (the setup of the in-range case below) */
	void standInRangeOfAnEnemy() {
		makeEnemy();
		ASSERT_TRUE(actor.knownList().addForTest(*victim.player));
		ASSERT_TRUE(victim.knownList().addForTest(*actor.player)); // so AggroList.isAware answers for the hit that lands
	}

	/** The one-handed Training Sword (item_templates.xml:375: attack speed 1400), bound once for the process as the DataManager holders keep it */
	static const model::templates::item::ItemTemplate* trainingSword() {
		static const model::templates::item::ItemTemplate* const sword = [] {
			xml::LoadContext context;
			return xml::bindString<model::templates::item::ItemTemplate>(context,
				R"(<item_template id="100000094" name="Training Sword" level="1" cName="sword_n_c_01a" mask="138366" item_group="SWORD")"
				R"( quality="COMMON" price="5" desc="700775" attack_type="PHYSICAL" max_enchant="10" m_slots="1"><weapon_stats hit_count="2")"
				R"( attack_range="1500" parry="173" physical_accuracy="52" critical="50" attack_speed="1400" max_damage="20" min_damage="16"/>)"
				R"(</item_template>)")
				.release();
		}();
		return sword;
	}

	/**
	 * A sword in each hand, loaded the way PlayerService.loadPlayer loads equipped items (Equipment.onLoadHandler). Its two checks pass: the sword
	 * skill 37 (ItemGroup SWORD, Equipment.checkAvailableEquipSkills) and, for the left hand, WeaponDualEffect.hasDualWieldEffect, which asks a
	 * spawned player's skill efficiency (what the dual-wield passive of a Scout sets). The attack speed becomes 1400 + 1400 / 4 = 1750
	 * (PlayerGameStats.getBaseAttackSpeed), the dual-wield window of the play-session report.
	 */
	void equipTwoSwords() {
		actor.player->setSkillList(model::skill::PlayerSkillList::create(
			{model::skill::PlayerSkillEntry::create(37, 1, 0, model::gameobjects::Persistable_PersistentState::UPDATED)}));
		actor.player->getGameStats()->setSkillEfficiency(1.0f);
		runtime::Ref<model::gameobjects::Item> mainHand =
			model::gameobjects::Item::create(300101, trainingSword(), 1, true, model::items::getSlotIdMask(model::items::ItemSlot::MAIN_HAND));
		runtime::Ref<model::gameobjects::Item> offHand =
			model::gameobjects::Item::create(300102, trainingSword(), 1, true, model::items::getSlotIdMask(model::items::ItemSlot::SUB_HAND));
		weapons = {mainHand, offHand};
		actor.player->getEquipment().onLoadHandler(*mainHand);
		actor.player->getEquipment().onLoadHandler(*offHand);
		ASSERT_EQ(actor.player->getEquipment().getMainHandWeapon().get(), mainHand.get());
		ASSERT_EQ(actor.player->getEquipment().getOffHandWeapon().get(), offHand.get()) << "the left-hand sword was put back into the cube";
		ASSERT_EQ(actor.player->getGameStats()->getAttackSpeed()->getCurrent(), 1750);
	}

	PlayerFixture actor, victim;
	std::vector<runtime::Ref<model::gameobjects::Item>> weapons;
	std::unique_ptr<TestClient> client;
	bool canSeeEnabled = false;
};

TEST_F(AttackRunTest, ADeadAttackerIsIgnoredBeforeAnythingElse) {
	actor.player->setLifeStats(std::make_unique<DeadLifeStats>(*actor.player));
	ASSERT_TRUE(actor.player->isDead());
	actor.player->setVisualState(CreatureVisualState::BLINKING); // spawn protection still on
	ASSERT_TRUE(actor.player->isProtectionActive());
	ASSERT_TRUE(actor.knownList().addForTest(*victim.player));

	attack(victim.player->getObjectId());

	// Java returns before the protection task and before the known list lookup, so nothing happened at all
	EXPECT_TRUE(actor.player->isProtectionActive()) << "CM_ATTACK.java:44-46: the dead branch returns first";
	EXPECT_TRUE((*client)->sentBytes().empty());
}

TEST_F(AttackRunTest, AnAttackEndsTheSpawnProtection) {
	actor.player->setVisualState(CreatureVisualState::BLINKING);
	ASSERT_TRUE(actor.player->isProtectionActive());

	// object id 0 is in nobody's known list, so runImpl stops after the protection task: that is what isolates this assertion
	attack(0);

	EXPECT_FALSE(actor.player->isProtectionActive()) << "CM_ATTACK.java:47-48 -> PlayerController.stopProtectionActiveTask";
	EXPECT_EQ((*client)->sentBytes(), exactly({serialized(SM_PLAYER_STATE(*actor.player), client->con())}))
		<< "stopProtectionActiveTask broadcasts SM_PLAYER_STATE to the sighted players and to the player himself";
}

TEST_F(AttackRunTest, AnUnknownTargetIsSilentlyIgnored) {
	LogCapture log({CM_ATTACK_LOGGER});

	attack(victim.player->getObjectId()); // never added to the known list

	EXPECT_TRUE((*client)->sentBytes().empty());
	EXPECT_FALSE(log.contains("unsupported target")) << "a null object is neither a Creature nor the warn branch: " << log.dump();
}

TEST_F(AttackRunTest, ATargetThatIsNoCreatureIsOnlyLogged) {
	runtime::Ref<PlainObject> object = PlainObject::create(300003);
	ASSERT_TRUE(actor.knownList().addForTest(*object));
	LogCapture log({CM_ATTACK_LOGGER});

	attack(object->getObjectId());

	// Java: `else if (obj != null) log.warn(player + " attacking unsupported target " + obj)` - a warning and nothing else
	EXPECT_TRUE(log.contains("attacking unsupported target")) << log.dump();
	EXPECT_TRUE(log.contains(object->toString())) << log.dump();
	EXPECT_TRUE((*client)->sentBytes().empty());
}

// ------------------------------------------------------------------------------- the creature branch, in process (m5b-plan.md A1a / A1b)
//
// Until m5b-1 E-01b these three cases were one, and it asserted the creature branch by catching the UnportedException of
// standins::playerRestrictionsCanAttack. That stand-in is gone: PlayerController::attackTarget calls restrictions::PlayerRestrictions::canAttack
// (C-01) and CreatureController::attackTarget calls attack::AttackUtil::calculatePhysAttackResult (B-01), so the packets the gate's A1a and A1b
// assert against a real client can be asserted here against a real AionConnection.

TEST_F(AttackRunTest, ACreatureTargetTheRestrictionsRefuseIsAnsweredWithNothingAtAll) {
	ASSERT_TRUE(actor.knownList().addForTest(*victim.player));
	LogCapture log({CM_ATTACK_LOGGER});
	// both characters are ELYOS, neither is flagged, so PlayerRestrictions.canAttack ends at `return player.isEnemy(target)` with false
	// (PlayerRestrictions.java:238) - the one arm of canAttack that sends no packet of its own
	ASSERT_FALSE(actor.player->isEnemy(*victim.player));

	attack(victim.player->getObjectId());

	// `obj instanceof Creature` was true (no warn), attackTarget ran and canAttack refused before the range check could answer anything
	EXPECT_FALSE(log.contains("unsupported target")) << log.dump();
	EXPECT_TRUE((*client)->sentBytes().empty()) << "canAttack's no returns silently: no SM_ATTACK_RESPONSE and no SM_ATTACK";
	EXPECT_EQ(actor.player->getGameStats()->getAttackCounter(), 0) << "CreatureController.attackTarget was never reached";
}

TEST_F(AttackRunTest, AnEnemyOutOfAttackRangeIsAnsweredWithTargetTooFarAway) {
	makeEnemy();
	ASSERT_TRUE(actor.knownList().addForTest(*victim.player));
	// 50 m apart: far beyond `1 + attackRange/1000` plus the first-hit tolerance calculateMaxCoveredDistance(player, 100)
	victim.player->getPosition()->setXYZH(150.0f, 100.0f, 50.0f, int8_t{0});
	ASSERT_FALSE(victim.player->getAggroList().isHating(*actor.player)) << "the first hit, so the tolerance branch is the one taken";

	attack(victim.player->getObjectId());

	// Java PlayerController.java:406-409
	EXPECT_EQ((*client)->sentBytes(),
		exactly({serialized(SM_ATTACK_RESPONSE::TARGET_TOO_FAR_AWAY(actor.player->getGameStats()->getAttackCounter()), client->con())}))
		<< "SM_ATTACK_RESPONSE.TARGET_TOO_FAR_AWAY and nothing else";
	EXPECT_EQ(actor.player->getGameStats()->getAttackCounter(), 0) << "the counter is only increased by a hit that happened";
	EXPECT_EQ(victim.player->getLifeStats()->getCurrentHp(), victim.player->getLifeStats()->getMaxHp());
}

TEST_F(AttackRunTest, AnEnemyInAttackRangeIsAnsweredWithSmAttackAndTakesDamage) {
	makeEnemy();
	ASSERT_TRUE(actor.knownList().addForTest(*victim.player));
	ASSERT_TRUE(victim.knownList().addForTest(*actor.player)); // so AggroList.isAware answers for the hit that lands
	const int32_t hpBefore = victim.player->getLifeStats()->getCurrentHp();

	attack(victim.player->getObjectId());

	// The whole chain: CM_ATTACK -> PlayerRestrictions.canAttack -> range -> GeoService.canSee -> CreatureController.attackTarget ->
	// AttackUtil.calculatePhysAttackResult -> SM_ATTACK. The packet is identified by its opcode, not by a byte comparison: its body carries the
	// random AttackResult list, which cannot be reconstructed here.
	const std::vector<int32_t> opcodes = sentOpcodes();
	ASSERT_FALSE(opcodes.empty()) << "attackTarget queued nothing at all";
	EXPECT_EQ(opcodes.front(), attackOpcode()) << "the first packet the attacker receives is SM_ATTACK (CreatureController.java:346-348)";
	EXPECT_EQ(std::count(opcodes.begin(), opcodes.end(), attackResponseOpcode()), 0)
		<< "no SM_ATTACK_RESPONSE: nothing on the way to the damage refused";
	EXPECT_EQ(actor.player->getGameStats()->getAttackCounter(), 1) << "increaseAttackCounter (CreatureController.java:351)";
	EXPECT_LT(victim.player->getLifeStats()->getCurrentHp(), hpBefore) << "onAttack reached reduceHp with the summed AttackUtil damage";
	EXPECT_EQ(victim.player->getAttackedCount(), 1);
}

// ---------------------------------------------------------------------- play-session fixes 2026-09-28 (docs/deviations/P4-11b.md)
//
// Deviation: the swing throttle of PlayerController.attackTarget reads its own timestamp. Java compares against lastAttackMillis
// (PlayerController.java:424), which enterCombat(true) also writes at the end of every hostile skill since #175 (Skill.java:650-651), so the
// client's first CM_ATTACK after an ability got STOP_WITHOUT_MESSAGE and auto-attack stopped - most often with two weapons, whose attack speed
// widens the refusal window. No clock is needed: the swings below follow each other within a few milliseconds.

TEST_F(AttackRunTest, TheFirstSwingAfterAHostileSkillIsAnsweredWithSmAttackAndDamage) {
	standInRangeOfAnEnemy();
	const int32_t hpBefore = victim.player->getLifeStats()->getCurrentHp();
	actor.player->getController().enterCombat(true); // what a hostile skill's Skill.endCast does
	ASSERT_TRUE(actor.player->getController().isInCombat()) << "#175's meaning is kept: a hostile skill counts as combat";

	attack(victim.player->getObjectId());

	const std::vector<int32_t> opcodes = sentOpcodes();
	ASSERT_FALSE(opcodes.empty()) << "attackTarget queued nothing at all";
	EXPECT_EQ(opcodes.front(), attackOpcode());
	EXPECT_EQ(std::count(opcodes.begin(), opcodes.end(), attackResponseOpcode()), 0) << "no STOP_WITHOUT_MESSAGE: the swing was not throttled";
	EXPECT_EQ(actor.player->getGameStats()->getAttackCounter(), 1);
	EXPECT_LT(victim.player->getLifeStats()->getCurrentHp(), hpBefore);
}

TEST_F(AttackRunTest, TwoSwingsBackToBackStillStopTheSecond) {
	standInRangeOfAnEnemy();
	attack(victim.player->getObjectId());
	ASSERT_EQ(actor.player->getGameStats()->getAttackCounter(), 1);
	const int32_t hpAfterTheFirst = victim.player->getLifeStats()->getCurrentHp();

	attack(victim.player->getObjectId());

	// Java's anti-speed-hack check (PlayerController.java:420-428): a swing within attackSpeed - 300 ms of the last one is refused
	EXPECT_EQ((*client)->sentBytes(),
		exactly({serialized(SM_ATTACK_RESPONSE::STOP_WITHOUT_MESSAGE(actor.player->getGameStats()->getAttackCounter()), client->con())}));
	EXPECT_EQ(actor.player->getGameStats()->getAttackCounter(), 1);
	EXPECT_EQ(victim.player->getLifeStats()->getCurrentHp(), hpAfterTheFirst);
}

TEST_F(AttackRunTest, WithTwoSwordsTheFirstSwingAfterAHostileSkillIsNotRefusedAndTheNextIs) {
	equipTwoSwords();
	standInRangeOfAnEnemy();
	const int32_t hpBefore = victim.player->getLifeStats()->getCurrentHp();
	actor.player->getController().enterCombat(true);

	attack(victim.player->getObjectId());

	const std::vector<int32_t> opcodes = sentOpcodes();
	ASSERT_FALSE(opcodes.empty()) << "attackTarget queued nothing at all";
	EXPECT_EQ(opcodes.front(), attackOpcode());
	EXPECT_EQ(std::count(opcodes.begin(), opcodes.end(), attackResponseOpcode()), 0) << "no STOP_WITHOUT_MESSAGE: the swing was not throttled";
	EXPECT_EQ(actor.player->getGameStats()->getAttackCounter(), 1);
	EXPECT_LT(victim.player->getLifeStats()->getCurrentHp(), hpBefore);

	attack(victim.player->getObjectId());

	EXPECT_EQ((*client)->sentBytes(),
		exactly({serialized(SM_ATTACK_RESPONSE::STOP_WITHOUT_MESSAGE(actor.player->getGameStats()->getAttackCounter()), client->con())}))
		<< "the swing throttle still refuses a second swing within 1750 - 300 ms";
}

// A swing is combat of its own: attackTarget calls enterCombat(true) right after the throttle (PlayerController.java:428), beside the throttle's
// own timestamp; the skill cases above only see the combat a skill entered
TEST_F(AttackRunTest, ASwingEntersCombat) {
	standInRangeOfAnEnemy();
	ASSERT_FALSE(actor.player->getController().isInCombat()) << "no fight yet";
	const int64_t beforeTheSwing = commons::utils::currentTimeMillis();

	attack(victim.player->getObjectId());

	ASSERT_EQ(actor.player->getGameStats()->getAttackCounter(), 1) << "the swing was let through";
	EXPECT_TRUE(actor.player->getController().isInCombat());
	EXPECT_GE(actor.player->getController().getLastCombatTime(), beforeTheSwing);
}

// Only a swing the throttle lets through moves its timestamp: a refused swing does not, so the next one is measured from the last swing that
// landed, as Java's lastAttackMillis was before #175. currentTimeMillis has no test clock, so this case waits for real (attack speed - 300 ms):
// the refused swing comes a third of the window after the first, the last one just after the first swing's window closed and well inside the
// window a refused swing would have opened.
TEST_F(AttackRunTest, ARefusedSwingDoesNotPostponeTheNextOne) {
	standInRangeOfAnEnemy();
	const int64_t window = actor.player->getGameStats()->getAttackSpeed()->getCurrent() - 300;
	ASSERT_GE(window, 900) << "the margins below assume the unarmed attack speed";

	attack(victim.player->getObjectId());
	const int64_t firstSwing = commons::utils::currentTimeMillis(); // no earlier than the time attackTarget read
	ASSERT_EQ(actor.player->getGameStats()->getAttackCounter(), 1);

	std::this_thread::sleep_for(std::chrono::milliseconds(window / 3));
	attack(victim.player->getObjectId());
	ASSERT_EQ((*client)->sentBytes(),
		exactly({serialized(SM_ATTACK_RESPONSE::STOP_WITHOUT_MESSAGE(actor.player->getGameStats()->getAttackCounter()), client->con())}))
		<< "the second swing is inside the first one's window (" << commons::utils::currentTimeMillis() - firstSwing << " ms after it)";

	while (commons::utils::currentTimeMillis() < firstSwing + window + 20)
		std::this_thread::sleep_for(std::chrono::milliseconds(5));
	attack(victim.player->getObjectId());

	const std::vector<int32_t> opcodes = sentOpcodes();
	ASSERT_FALSE(opcodes.empty()) << "attackTarget queued nothing at all";
	EXPECT_EQ(opcodes.front(), attackOpcode());
	EXPECT_EQ(std::count(opcodes.begin(), opcodes.end(), attackResponseOpcode()), 0)
		<< "the refused swing moved the throttle's timestamp (" << commons::utils::currentTimeMillis() - firstSwing << " ms after the first)";
	EXPECT_EQ(actor.player->getGameStats()->getAttackCounter(), 2);
}

TEST(HeadingUpdateRunTest, RunImplDoesNothing) {
	// Java CM_HEADING_UPDATE.runImpl is empty; it does not even read the connection, so it runs without an active player
	Driver<CM_HEADING_UPDATE> packet(HEADING_UPDATE_OPCODE);
	EXPECT_NO_THROW(packet.runNow());
}

} // namespace
} // namespace aion::gameserver::network::aion::clientpackets::testing
