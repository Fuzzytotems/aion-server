// CM_EMOTION (stage 3 wave B, m5a-plan.md §11 / m5a-client-session.md F-2): C_ACTION, the packet the real client sends for sitting, standing,
// walk/run, weapon in and out, jumping, chat emotes and powershards, which the server ignored because the class was not ported.
//
// readImpl is a switch over 22 emotion types with four different body layouts, and runImpl is a second switch over the same types. Both are
// driven here against a real Player and a real AionConnection: the readImpl tests assert that each layout is consumed exactly, the runImpl tests
// assert the CreatureState the emotion leaves behind and the exact SM_EMOTION or SM_SYSTEM_MESSAGE bytes it broadcasts.
//
// Java: game-server/src/com/aionemu/gameserver/network/aion/clientpackets/CM_EMOTION.java:53-234.

#include "InWorldPacketRunSupport.h"

#include <cstdint>
#include <utility>
#include <vector>

#include "aion/gameserver/configs/main/CustomConfig.h"
#include "aion/gameserver/controllers/ObserveController.h"
#include "aion/gameserver/controllers/PlayerController.h"
#include "aion/gameserver/controllers/effect/PlayerEffectController.h"
#include "aion/gameserver/controllers/observer/ItemUseObserver.h"
#include "aion/gameserver/model/EmotionType.h"
#include "aion/gameserver/model/EmotionTypeInfo.h"
#include "aion/gameserver/model/TaskId.h"
#include "aion/gameserver/model/actions/PlayerMode.h"
#include "aion/gameserver/model/gameobjects/player/Equipment.h"
#include "aion/gameserver/model/gameobjects/state/CreatureState.h"
#include "aion/gameserver/model/gameobjects/state/CreatureStateInfo.h"
#include "aion/gameserver/model/gameobjects/state/CreatureVisualState.h"
#include "aion/gameserver/model/gameobjects/state/FlyState.h"
#include "aion/gameserver/network/aion/clientpackets/CM_EMOTION.h"
#include "aion/gameserver/network/aion/serverpackets/SM_EMOTION.h"
#include "aion/gameserver/network/aion/serverpackets/SM_PLAYER_STATE.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/skillengine/effect/AbnormalState.h"
#include "aion/gameserver/world/WorldPosition.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets::testing {
namespace {

using model::EmotionType;
using model::gameobjects::state::CreatureState;
using model::gameobjects::state::CreatureVisualState;
using model::gameobjects::state::FlyState;
using network::test::LogCapture;
using network::test::PacketWriter;
using serverpackets::SM_EMOTION;
using serverpackets::SM_PLAYER_STATE;
using serverpackets::SM_SYSTEM_MESSAGE;
using skillengine::effect::AbnormalState;

/**
 * One of Java's anonymous `new ItemUseObserver() { ... }` of a delayed item action (ItemActionService, StigmaService, Equipment), which is what
 * `PlayerController::cancelUseItem` aborts: it asks the ObserveController for every attached ItemUseObserver, removes it and calls
 * onRemoved() + abort() on it (ObserveController.cpp:187-201). abort() is the only abstract method.
 */
class TestItemUseObserver final : public controllers::observer::ItemUseObserver {
	AION_MAKE_REF_FRIEND
public:
	/** true once ObserveController::abortItemUseObservers cancelled this item use */
	bool aborted = false;

	static runtime::Ref<TestItemUseObserver> create() { return runtime::makeRef<TestItemUseObserver>(); }

	void abort() override { aborted = true; }

protected:
	TestItemUseObserver() = default;
	~TestItemUseObserver() override = default;
};

/** the decoded opcode of CM_EMOTION (ClientPacketInfo.gen.inc:54) */
constexpr int32_t OPCODE = 43;

const char* CM_EMOTION_LOGGER = "com.aionemu.gameserver.network.aion.clientpackets.CM_EMOTION";
const char* BASE_CLIENT_PACKET_LOGGER = "com.aionemu.commons.network.packet.BaseClientPacket";

/** Java readImpl: UC emotion type, and for most types nothing else */
std::vector<uint8_t> plainBody(EmotionType type) {
	return PacketWriter().C(model::getTypeId(type)).data;
}

/** EmotionType.EMOTE: UC type, UH emotion, D target object id */
std::vector<uint8_t> emoteBody(int32_t emotion, int32_t targetObjectId) {
	return PacketWriter().C(model::getTypeId(EmotionType::EMOTE)).H(emotion).D(targetObjectId).data;
}

/** EmotionType.CHAIR_SIT / CHAIR_UP: UC type, F x, F y, F z, C heading */
std::vector<uint8_t> chairBody(EmotionType type, float x, float y, float z, int8_t heading) {
	return PacketWriter().C(model::getTypeId(type)).F(x).F(y).F(z).C(heading).data;
}

class EmotionRunTest : public InWorldPacketTest {
protected:
	void SetUp() override {
		InWorldPacketTest::SetUp();
		configs::main::CustomConfig::CANCEL_ITEM_USE_ON_TARGET_CHANGE.store(true); // gameserver.custom properties default
		actor = makePlayer(100001, 9001, "Actor");
		client = std::make_unique<TestClient>();
		client->enterWorld(actor);
		(*client)->clearSent();
	}

	void TearDown() override {
		if (actor.player)
			actor.player->setClientConnection(nullptr);
		client.reset();
		actor = {};
		InWorldPacketTest::TearDown();
	}

	/** Reads and runs one CM_EMOTION, after dropping whatever was queued before */
	void emote(const std::vector<uint8_t>& packetBody) {
		(*client)->clearSent();
		Driver<CM_EMOTION> packet(OPCODE);
		packet.readAndRun(packetBody, client->get());
	}

	void emote(EmotionType type) { emote(plainBody(type)); }

	/** The SM_EMOTION the broadcast is expected to carry (Java: new SM_EMOTION(player, type, emotion, x, y, z, heading, targetObjectId)) */
	std::vector<uint8_t> expectedEmotion(EmotionType type, int32_t emotion = 0, float x = 0, float y = 0, float z = 0, int8_t heading = 0,
		int32_t targetObjectId = 0) {
		return serialized(SM_EMOTION(*actor.player, type, emotion, x, y, z, heading, targetObjectId));
	}

	PlayerFixture actor;
	std::unique_ptr<TestClient> client;
};

// ------------------------------------------------------------------------------------------------------------------- readImpl

TEST_F(EmotionRunTest, ReadConsumesEveryBodyLayoutExactly) {
	// the 20 types that read nothing after the type byte (Java CM_EMOTION.java:58-78)
	for (EmotionType type : {EmotionType::SELECT_TARGET, EmotionType::JUMP, EmotionType::SIT, EmotionType::STAND, EmotionType::LAND_FLYTELEPORT,
			 EmotionType::FLY, EmotionType::LAND, EmotionType::DIE, EmotionType::EMOTE_END, EmotionType::WALK, EmotionType::RUN,
			 EmotionType::OPEN_DOOR, EmotionType::CLOSE_DOOR, EmotionType::POWERSHARD_ON, EmotionType::POWERSHARD_OFF,
			 EmotionType::ATTACKMODE_IN_MOVE, EmotionType::ATTACKMODE_IN_STANDING, EmotionType::NEUTRALMODE_IN_MOVE,
			 EmotionType::NEUTRALMODE_IN_STANDING, EmotionType::END_SPRINT}) {
		Driver<CM_EMOTION> packet(OPCODE);
		std::vector<uint8_t> data = plainBody(type);
		packet.setBuffer(commons::utils::ByteBuffer::wrap(data));
		LogCapture capture({BASE_CLIENT_PACKET_LOGGER, CM_EMOTION_LOGGER});
		ASSERT_TRUE(packet.read()) << static_cast<int32_t>(type);
		EXPECT_EQ(packet.getRemainingBytes(), 0) << static_cast<int32_t>(type);
		EXPECT_FALSE(capture.contains("Missing")) << capture.dump();
		EXPECT_FALSE(capture.contains("Unknown emotion type")) << capture.dump();
	}
	{ // WINDSTREAM_STRAFE: one extra byte
		Driver<CM_EMOTION> packet(OPCODE);
		std::vector<uint8_t> data = PacketWriter().C(model::getTypeId(EmotionType::WINDSTREAM_STRAFE)).C(2).data;
		packet.setBuffer(commons::utils::ByteBuffer::wrap(data));
		ASSERT_TRUE(packet.read());
		EXPECT_EQ(packet.getRemainingBytes(), 0);
	}
	{ // START_SPRINT: one extra int
		Driver<CM_EMOTION> packet(OPCODE);
		std::vector<uint8_t> data = PacketWriter().C(model::getTypeId(EmotionType::START_SPRINT)).D(1).data;
		packet.setBuffer(commons::utils::ByteBuffer::wrap(data));
		ASSERT_TRUE(packet.read());
		EXPECT_EQ(packet.getRemainingBytes(), 0);
	}
	{ // EMOTE: UH emotion, D target
		Driver<CM_EMOTION> packet(OPCODE);
		std::vector<uint8_t> data = emoteBody(0x1234, 0x0A0B0C0D);
		packet.setBuffer(commons::utils::ByteBuffer::wrap(data));
		ASSERT_EQ(data.size(), 7u) << "C type + H emotion + D target";
		ASSERT_TRUE(packet.read());
		EXPECT_EQ(packet.getRemainingBytes(), 0);
	}
	for (EmotionType type : {EmotionType::CHAIR_SIT, EmotionType::CHAIR_UP}) { // F x, F y, F z, C heading
		Driver<CM_EMOTION> packet(OPCODE);
		std::vector<uint8_t> data = chairBody(type, 1.5f, -2.25f, 3.0f, 60);
		packet.setBuffer(commons::utils::ByteBuffer::wrap(data));
		ASSERT_EQ(data.size(), 14u) << "C type + 3 * F + C heading";
		ASSERT_TRUE(packet.read());
		EXPECT_EQ(packet.getRemainingBytes(), 0);
	}
}

/**
 * Java: an id no EmotionType carries reaches the default arm and logs the hex id in upper case. (The type is read with readUC, but the highest
 * EmotionType id is END_SPRINT's 54, so a signed read would decode every real type the same way and only this arm could ever tell them apart.)
 */
TEST_F(EmotionRunTest, ReadLogsAnUnknownEmotionType) {
	Driver<CM_EMOTION> packet(OPCODE);
	std::vector<uint8_t> data = PacketWriter().C(0xFE).data; // no EmotionType has type id 254
	ASSERT_EQ(model::getEmotionTypeById(0xFE), EmotionType::NONE);
	packet.setBuffer(commons::utils::ByteBuffer::wrap(data));
	LogCapture capture({CM_EMOTION_LOGGER});

	ASSERT_TRUE(packet.read());

	EXPECT_TRUE(capture.contains("Unknown emotion type? 0xFE")) << capture.dump();
}

// ------------------------------------------------------------------------------------------------------------------- runImpl

TEST_F(EmotionRunTest, ADeadPlayerIsIgnored) {
	actor.player->setLifeStats(std::make_unique<DeadLifeStats>(*actor.player));
	ASSERT_TRUE(actor.player->isDead());

	emote(EmotionType::SIT);

	EXPECT_FALSE(actor.player->isInState(CreatureState::RESTING));
	EXPECT_TRUE((*client)->sentBytes().empty());
}

TEST_F(EmotionRunTest, SitStartsRestingAndStandEndsIt) {
	emote(EmotionType::SIT);

	EXPECT_TRUE(actor.player->isInState(CreatureState::RESTING));
	EXPECT_EQ((*client)->sentBytes(), exactly({expectedEmotion(EmotionType::SIT)}));

	emote(EmotionType::STAND);

	EXPECT_FALSE(actor.player->isInState(CreatureState::RESTING));
	EXPECT_EQ((*client)->sentBytes(), exactly({expectedEmotion(EmotionType::STAND)}));
}

TEST_F(EmotionRunTest, AttackModeAndNeutralModeToggleTheWeaponState) {
	emote(EmotionType::ATTACKMODE_IN_STANDING);

	EXPECT_TRUE(actor.player->isInState(CreatureState::WEAPON_EQUIPPED));
	EXPECT_TRUE(actor.player->isInAttackMode());
	EXPECT_EQ((*client)->sentBytes(), exactly({expectedEmotion(EmotionType::ATTACKMODE_IN_STANDING)}));

	emote(EmotionType::NEUTRALMODE_IN_MOVE);

	EXPECT_FALSE(actor.player->isInState(CreatureState::WEAPON_EQUIPPED));
	EXPECT_EQ((*client)->sentBytes(), exactly({expectedEmotion(EmotionType::NEUTRALMODE_IN_MOVE)}));
}

TEST_F(EmotionRunTest, WalkAndRunToggleTheWalkState) {
	emote(EmotionType::WALK);
	EXPECT_TRUE(actor.player->isInState(CreatureState::WALK_MODE));

	emote(EmotionType::RUN);
	EXPECT_FALSE(actor.player->isInState(CreatureState::WALK_MODE));
	EXPECT_EQ((*client)->sentBytes(), exactly({expectedEmotion(EmotionType::RUN)}));
}

/** Java: `isInState(PRIVATE_SHOP) || isInAttackMode() && (type == CHAIR_SIT || type == JUMP)` - && binds tighter, so only those two are blocked */
TEST_F(EmotionRunTest, AttackModeBlocksJumpAndChairSitButNotSitting) {
	actor.player->setState(CreatureState::WEAPON_EQUIPPED);
	ASSERT_TRUE(actor.player->isInAttackMode());

	emote(EmotionType::JUMP);
	EXPECT_TRUE((*client)->sentBytes().empty()) << "Java returns before the broadcast";

	emote(chairBody(EmotionType::CHAIR_SIT, 1.0f, 2.0f, 3.0f, 0));
	EXPECT_FALSE(actor.player->isInState(CreatureState::CHAIR));
	EXPECT_TRUE((*client)->sentBytes().empty());

	emote(EmotionType::SIT);
	EXPECT_TRUE(actor.player->isInState(CreatureState::RESTING)) << "sitting down with the weapon out is allowed";
	EXPECT_EQ((*client)->sentBytes(), exactly({expectedEmotion(EmotionType::SIT)}));
}

/** Java: the whole runImpl returns when the player is in a private shop, whatever the emotion is */
TEST_F(EmotionRunTest, APrivateShopBlocksEveryEmotion) {
	actor.player->setState(CreatureState::PRIVATE_SHOP);

	emote(EmotionType::ATTACKMODE_IN_STANDING);

	EXPECT_FALSE(actor.player->isInState(CreatureState::WEAPON_EQUIPPED));
	EXPECT_TRUE((*client)->sentBytes().empty());
}

TEST_F(EmotionRunTest, ChairSitTakesThePositionFromTheBodyAndChairUpLeavesIt) {
	emote(chairBody(EmotionType::CHAIR_SIT, 1.5f, -2.25f, 3.0f, 60));

	// CHAIR is set with replace, so the whole state word becomes FLYING + RESTING (CreatureStateInfo.h:38) and ACTIVE is gone
	EXPECT_TRUE(actor.player->isInState(CreatureState::CHAIR));
	EXPECT_FALSE(actor.player->isInState(CreatureState::ACTIVE));
	// Java broadcasts the client's x/y/z/heading unchanged; the packet writes them for CHAIR_SIT (SM_EMOTION.cpp case CHAIR_SIT)
	EXPECT_EQ((*client)->sentBytes(), exactly({expectedEmotion(EmotionType::CHAIR_SIT, 0, 1.5f, -2.25f, 3.0f, 60)}));

	emote(chairBody(EmotionType::CHAIR_UP, 1.5f, -2.25f, 3.0f, 60));

	EXPECT_FALSE(actor.player->isInState(CreatureState::CHAIR));
	EXPECT_EQ(actor.player->getState(), model::gameobjects::state::getId(CreatureState::ACTIVE)) << "setState(ACTIVE, true) replaces the word";
	EXPECT_EQ((*client)->sentBytes(), exactly({expectedEmotion(EmotionType::CHAIR_UP, 0, 1.5f, -2.25f, 3.0f, 60)}));
}

/**
 * Java: CHAIR_UP only changes the state when the player was on a chair, but it broadcasts either way. ACTIVE is the state a Creature starts in,
 * so the telling difference is the `replace` flag: setState(ACTIVE, true) would overwrite the whole word and drop WALK_MODE with it.
 */
TEST_F(EmotionRunTest, ChairUpWithoutAChairBroadcastsAndKeepsTheOtherStates) {
	actor.player->setState(CreatureState::WALK_MODE);
	const int32_t stateBefore = actor.player->getState();
	ASSERT_FALSE(actor.player->isInState(CreatureState::CHAIR));

	emote(chairBody(EmotionType::CHAIR_UP, 0, 0, 0, 0));

	EXPECT_EQ(actor.player->getState(), stateBefore) << "setState(ACTIVE, true) is inside the isInState(CHAIR) branch";
	EXPECT_TRUE(actor.player->isInState(CreatureState::WALK_MODE));
	EXPECT_EQ((*client)->sentBytes(), exactly({expectedEmotion(EmotionType::CHAIR_UP)}));
}

TEST_F(EmotionRunTest, PowershardOnWithoutABoosterIsRefused) {
	ASSERT_FALSE(actor.player->getEquipment().isPowerShardEquipped());

	emote(EmotionType::POWERSHARD_ON);

	EXPECT_FALSE(actor.player->isInState(CreatureState::POWERSHARD));
	EXPECT_EQ((*client)->sentBytes(), exactly({serialized(SM_SYSTEM_MESSAGE::STR_WEAPON_BOOST_NO_BOOSTER_EQUIPED())}))
		<< "Java returns after the message, so no SM_EMOTION follows";
}

TEST_F(EmotionRunTest, PowershardOffEndsBoostMode) {
	actor.player->setState(CreatureState::POWERSHARD);

	emote(EmotionType::POWERSHARD_OFF);

	EXPECT_FALSE(actor.player->isInState(CreatureState::POWERSHARD));
	// Java: the message first, then the broadcast at the end of runImpl
	EXPECT_EQ((*client)->sentBytes(),
		exactly({serialized(SM_SYSTEM_MESSAGE::STR_WEAPON_BOOST_BOOST_MODE_ENDED()), expectedEmotion(EmotionType::POWERSHARD_OFF)}));
}

/** Java getTargetObjectId: the player's current target wins over the object id in the packet, and the id is used only without a target */
TEST_F(EmotionRunTest, AnEmoteCarriesTheCurrentTargetAndFallsBackToThePacketsId) {
	emote(emoteBody(31, 0x00C0FFEE));

	EXPECT_EQ((*client)->sentBytes(), exactly({expectedEmotion(EmotionType::EMOTE, 31, 0, 0, 0, 0, 0x00C0FFEE)}));

	actor.player->setTarget(runtime::Ptr<model::gameobjects::VisibleObject>(*actor.player));
	emote(emoteBody(31, 0x00C0FFEE));

	EXPECT_EQ((*client)->sentBytes(), exactly({expectedEmotion(EmotionType::EMOTE, 31, 0, 0, 0, 0, actor.player->getObjectId())}));
	actor.player->setTarget(nullptr);
}

/**
 * Java's abnormal-state guard (CM_EMOTION.java:109-115) covers every emotion except SELECT_TARGET and the four attack/neutral mode ones, so a
 * rooted player can still sheathe his weapon but not sit down. Its three operands - any CANT_MOVE_STATE bit, FEAR, CONFUSE - are driven one by
 * one through EffectController::setAbnormal (EffectController.java:706-718, ported in M5b-2 part 2): each makes SIT a silent no-op, and
 * unsetAbnormal (:720-733; no effect carries the state, so the bit is simply cleared) lets the same player sit again. ROOT is the CANT_MOVE_STATE
 * bit because it is not in AUTOMATICALLY_STANDUP, so setting it leaves the states alone; FEAR and CONFUSE are, but only for a RESTING player.
 */
TEST_F(EmotionRunTest, TheAbnormalStateGuardIsReadAndLetsAnUnaffectedPlayerThrough) {
	controllers::effect::PlayerEffectController& effects = *actor.player->getEffectController();
	for (const auto& [state, name] : {std::pair{AbnormalState::ROOT, "ROOT"}, std::pair{AbnormalState::FEAR, "FEAR"},
			 std::pair{AbnormalState::CONFUSE, "CONFUSE"}}) {
		SCOPED_TRACE(name);
		effects.setAbnormal(state);
		ASSERT_TRUE(effects.isInAnyAbnormalState(AbnormalState::CANT_MOVE_STATE) || effects.isUnderFear() || effects.isConfused());

		emote(EmotionType::SIT);

		EXPECT_FALSE(actor.player->isInState(CreatureState::RESTING)) << "the guard returns before the SIT arm";
		EXPECT_TRUE((*client)->sentBytes().empty()) << "and before the broadcast";
		effects.unsetAbnormal(state);
	}
	ASSERT_FALSE(effects.isInAnyAbnormalState(AbnormalState::CANT_MOVE_STATE));
	ASSERT_FALSE(effects.isUnderFear());
	ASSERT_FALSE(effects.isConfused());

	emote(EmotionType::SIT);

	EXPECT_TRUE(actor.player->isInState(CreatureState::RESTING));
	EXPECT_EQ((*client)->sentBytes(), exactly({expectedEmotion(EmotionType::SIT)}));
}

/** The five emotions the abnormal-state guard lets through (CM_EMOTION.java:109-111): a rooted player still switches to attack mode */
TEST_F(EmotionRunTest, TheAbnormalStateGuardLetsTheModeSwitchesThrough) {
	actor.player->getEffectController()->setAbnormal(AbnormalState::ROOT);
	ASSERT_TRUE(actor.player->getEffectController()->isInAnyAbnormalState(AbnormalState::CANT_MOVE_STATE));

	emote(EmotionType::ATTACKMODE_IN_STANDING);

	EXPECT_TRUE(actor.player->isInState(CreatureState::WEAPON_EQUIPPED));
	EXPECT_EQ((*client)->sentBytes(), exactly({expectedEmotion(EmotionType::ATTACKMODE_IN_STANDING)}));
}

/** Java: SELECT_TARGET returns right after the item-use cancel, before the stance check and the broadcast */
TEST_F(EmotionRunTest, SelectTargetAnswersNothing) {
	emote(EmotionType::SELECT_TARGET);

	EXPECT_TRUE((*client)->sentBytes().empty());
}

/**
 * Java CM_EMOTION.java:122-126, the body of the SELECT_TARGET arm: selecting a target cancels a pending item use, but only while
 * `CustomConfig.CANCEL_ITEM_USE_ON_TARGET_CHANGE` is on. The second half is what tells the arm apart from the rest of runImpl, which cancels the
 * item use unconditionally two lines later (CM_EMOTION.cpp:137): with the arm gone, the observer of the second block is aborted as well.
 */
TEST_F(EmotionRunTest, SelectTargetCancelsAPendingItemUseOnlyWhileTheConfigIsOn) {
	runtime::Ref<TestItemUseObserver> pendingUse = TestItemUseObserver::create();
	actor.player->getObserveController()->addObserver(*pendingUse);
	ASSERT_TRUE(configs::main::CustomConfig::CANCEL_ITEM_USE_ON_TARGET_CHANGE.load()) << "the gameserver.custom properties default";

	emote(EmotionType::SELECT_TARGET);

	EXPECT_TRUE(pendingUse->aborted) << "player.getController().cancelUseItem() aborts every attached ItemUseObserver";
	EXPECT_TRUE((*client)->sentBytes().empty());

	// the same packet with the config off: the arm does nothing at all
	configs::main::CustomConfig::CANCEL_ITEM_USE_ON_TARGET_CHANGE.store(false);
	runtime::Ref<TestItemUseObserver> keptUse = TestItemUseObserver::create();
	actor.player->getObserveController()->addObserver(*keptUse);

	emote(EmotionType::SELECT_TARGET);

	EXPECT_FALSE(keptUse->aborted) << "CANCEL_ITEM_USE_ON_TARGET_CHANGE off: the item use survives the target change";
	EXPECT_TRUE((*client)->sentBytes().empty());
	actor.player->getObserveController()->removeObserver(*keptUse);
	configs::main::CustomConfig::CANCEL_ITEM_USE_ON_TARGET_CHANGE.store(true);
}

/**
 * Java CM_EMOTION.java:127: the `return` of the SELECT_TARGET arm, i.e. everything it skips. Both markers below are states in which every other
 * emotion answers: a stance makes runImpl send STR_SKILL_CAN_NOT_CHANGE_MODE_WHILE_IN_CURRENT_STANCE (CM_EMOTION.cpp:141-150), and an active spawn
 * protection makes the tail cancel the PROTECTION_ACTIVE task (CM_EMOTION.cpp:238-239). A SELECT_TARGET is gone before either.
 */
TEST_F(EmotionRunTest, SelectTargetReturnsBeforeTheStanceCheckAndTheProtectionTail) {
	actor.player->getController().startProtectionActiveTask();
	ASSERT_TRUE(actor.player->isProtectionActive());
	ASSERT_TRUE(actor.player->getController().hasTask(model::TaskId::PROTECTION_ACTIVE));

	emote(EmotionType::SELECT_TARGET);

	EXPECT_TRUE(actor.player->getController().hasTask(model::TaskId::PROTECTION_ACTIVE)) << "the tail is behind the return";
	EXPECT_TRUE(actor.player->isProtectionActive());
	EXPECT_TRUE((*client)->sentBytes().empty());
	actor.player->getController().stopProtectionActiveTask();

	actor.player->getController().startStance(1); // Java: PlayerController.startStance(skillId), the only writer of stanceObserver
	ASSERT_TRUE(actor.player->getController().isUnderStance());

	emote(EmotionType::SELECT_TARGET);

	EXPECT_TRUE((*client)->sentBytes().empty()) << "the stance check is behind the return too";
	// PlayerController::stopStance() would remove the stance effect, and EffectController::removeEffect is AION_UNPORTED (P5-04), so the stance
	// is dropped the way the logout breaker does it
	actor.player->getController().breakStanceObserver();
}

/**
 * Java CM_EMOTION.java:184-187 (`case WALK: if (player.isFlying()) return;`): walk mode cannot be toggled in the air. Player.isFlying() is the
 * fly state word, not the creature state (Player.java:727-729), so this is the one fly guard of runImpl a test can drive without the flight
 * machinery: FLY, LAND and LAND_FLYTELEPORT need FlyController.startFly / endFly / onFlyTeleportEnd, which the wave B report lists as untestable
 * at M5a.
 */
TEST_F(EmotionRunTest, WalkIsRefusedWhileFlying) {
	actor.player->setFlyState(FlyState::GLIDING);
	ASSERT_TRUE(actor.player->isFlying());

	emote(EmotionType::WALK);

	EXPECT_FALSE(actor.player->isInState(CreatureState::WALK_MODE)) << "Java returns before setState(WALK_MODE)";
	EXPECT_TRUE((*client)->sentBytes().empty()) << "and before the broadcast";

	actor.player->unsetFlyState(FlyState::GLIDING);
	ASSERT_FALSE(actor.player->isFlying());

	emote(EmotionType::WALK);

	EXPECT_TRUE(actor.player->isInState(CreatureState::WALK_MODE)) << "on the ground the same packet toggles walk mode";
	EXPECT_EQ((*client)->sentBytes(), exactly({expectedEmotion(EmotionType::WALK)}));
}

/**
 * Java CM_EMOTION.java:228-229, the tail of runImpl: an emotion that got through ends the spawn protection. The whole body of
 * PlayerController.stopProtectionActiveTask runs here, because the position is marked spawned as World.spawn would
 * (PlayerController.cpp:734-742): the PROTECTION_ACTIVE task is cancelled, BLINKING is unset and SM_PLAYER_STATE follows the SM_EMOTION of the
 * broadcast, which the tail comes after.
 */
TEST_F(EmotionRunTest, AnEmotionEndsTheSpawnProtection) {
	actor.player->getPosition()->setIsSpawned(true); // Java World.spawn; stopProtectionActiveTask is a no-op for an unspawned player
	actor.player->getController().startProtectionActiveTask();
	ASSERT_TRUE(actor.player->isProtectionActive()) << "startProtectionActiveTask sets the BLINKING visual state";
	ASSERT_TRUE(actor.player->getController().hasTask(model::TaskId::PROTECTION_ACTIVE)) << "and schedules the 60 s task";

	emote(EmotionType::JUMP);

	EXPECT_FALSE(actor.player->getController().hasTask(model::TaskId::PROTECTION_ACTIVE)) << "cancelTask(PROTECTION_ACTIVE)";
	EXPECT_FALSE(actor.player->isProtectionActive()) << "unsetVisualState(BLINKING)";
	EXPECT_EQ((*client)->sentBytes(), exactly({expectedEmotion(EmotionType::JUMP), serialized(SM_PLAYER_STATE(*actor.player))}))
		<< "Java runs the tail after the broadcast";
	actor.player->getPosition()->setIsSpawned(false);
}

/**
 * The guard of the same tail (Java CM_EMOTION.java:228): only a player who is blinking has his protection task cancelled - the state the
 * unguarded call would also cancel is a PROTECTION_ACTIVE task that outlived the BLINKING state, so that is what this arranges.
 */
TEST_F(EmotionRunTest, AnEmotionLeavesTheProtectionTaskOfAPlayerWhoIsNotBlinking) {
	actor.player->getController().startProtectionActiveTask();
	actor.player->unsetVisualState(CreatureVisualState::BLINKING);
	ASSERT_FALSE(actor.player->isProtectionActive());
	ASSERT_TRUE(actor.player->getController().hasTask(model::TaskId::PROTECTION_ACTIVE));

	emote(EmotionType::JUMP);

	EXPECT_TRUE(actor.player->getController().hasTask(model::TaskId::PROTECTION_ACTIVE)) << "the tail is guarded by isProtectionActive()";
	EXPECT_EQ((*client)->sentBytes(), exactly({expectedEmotion(EmotionType::JUMP)})) << "no SM_PLAYER_STATE follows";
	actor.player->getController().cancelTask(model::TaskId::PROTECTION_ACTIVE);
}

/** Java: both sprint emotions need PlayerMode.RIDE, which a player on foot is not in, so they return before touching the sprint flag */
TEST_F(EmotionRunTest, SprintWithoutARideIsIgnored) {
	ASSERT_FALSE(actor.player->isInPlayerMode(model::actions::PlayerMode::RIDE));

	emote(PacketWriter().C(model::getTypeId(EmotionType::START_SPRINT)).D(0).data);
	EXPECT_FALSE(actor.player->isInSprintMode());
	EXPECT_TRUE((*client)->sentBytes().empty());

	emote(EmotionType::END_SPRINT);
	EXPECT_FALSE(actor.player->isInSprintMode());
	EXPECT_TRUE((*client)->sentBytes().empty());
}

/** Java: OPEN_DOOR and CLOSE_DOOR have an empty case arm, so they fall through to the broadcast unchanged */
TEST_F(EmotionRunTest, DoorEmotionsOnlyBroadcast) {
	emote(EmotionType::OPEN_DOOR);
	EXPECT_EQ((*client)->sentBytes(), exactly({expectedEmotion(EmotionType::OPEN_DOOR)}));

	emote(EmotionType::CLOSE_DOOR);
	EXPECT_EQ((*client)->sentBytes(), exactly({expectedEmotion(EmotionType::CLOSE_DOOR)}));
}

/** Java: a type with no case arm at all (JUMP, DIE, EMOTE_END) skips the switch and broadcasts */
TEST_F(EmotionRunTest, JumpOnlyBroadcasts) {
	emote(EmotionType::JUMP);

	EXPECT_EQ((*client)->sentBytes(), exactly({expectedEmotion(EmotionType::JUMP)}));
}

} // namespace
} // namespace aion::gameserver::network::aion::clientpackets::testing
