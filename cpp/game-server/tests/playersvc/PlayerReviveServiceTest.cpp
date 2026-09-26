// P5-08 PlayerReviveService::revive / bindRevive and the TeleportService chain they need (m5b-plan.md C-03): what happens to a dead character
// when the client answers the death window. Before M5b-1 all twelve PlayerReviveService bodies and all of TeleportService's teleportTo family
// were AION_UNPORTED.
//
// Java: PlayerReviveService.java:100-133 (bindRevive) and :189-213 (revive); TeleportService.java:179-219, 249-288, 362-383, 500-536.
//
// What this file can and cannot reach, stated rather than left to the reader:
// - revive(..., setSoulSickness = false, ...) runs end to end here. That is the duelRevive/kiskRevive/EVENT_MODE shape and it covers every
//   statement of the body except the updateSoulSickness call.
// - revive(..., setSoulSickness = true, ...) - the shape bindRevive uses outside EVENT_MODE - reaches
//   PlayerController::updateSoulSickness, which reads the player's houses (HousingService, a database) and ends in
//   SkillEngine::getSkill, AION_UNPORTED until M5b-2. It is therefore only reachable on a real server, and only with
//   gameserver.soulsickness.disable low enough for the account's membership; see the wave report.
// - the teleport chain needs a real World (World::despawn and World::spawn), which TeleportOnSameMapTest builds from the world chunk's test
//   maps. It runs without a client connection, because PacketSendUtility::sendPacket returns early then and SM_PLAYER_INFO would otherwise
//   serialize the player's houses out of a database that a unit test has not got.

#include "../cm_ak/InWorldPacketRunSupport.h"
#include "../world/WorldTestSupport.h"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "aion/gameserver/controllers/PlayerController.h"
#include "aion/gameserver/dataholders/PlayerInitialData.bind.h"
#include "aion/gameserver/dataholders/PlayerInitialData.h"
#include "aion/gameserver/model/EmotionType.h"
#include "aion/gameserver/model/gameobjects/player/BindPointPosition.h"
#include "aion/gameserver/model/gameobjects/player/CustomPlayerState.h"
#include "aion/gameserver/model/gameobjects/player/PlayerCommonData.h"
#include "aion/gameserver/model/gameobjects/player/motion/MotionList.h"
#include "aion/gameserver/model/gameobjects/state/CreatureState.h"
#include "aion/gameserver/model/gameobjects/state/CreatureStateInfo.h"
#include "aion/gameserver/model/stats/container/PlayerGameStats.h"
#include "aion/gameserver/model/stats/container/PlayerLifeStats.h"
#include "aion/gameserver/network/aion/serverpackets/SM_EMOTION.h"
#include "aion/gameserver/network/aion/serverpackets/SM_STATS_INFO.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/services/panesterra/ahserion/PanesterraFaction.h"
#include "aion/gameserver/services/player/PlayerReviveService.h"
#include "aion/gameserver/services/teleport/TeleportService.h"
#include "aion/gameserver/world/World.h"
#include "aion/gameserver/world/WorldMapTypeInfo.h"
#include "aion/gameserver/world/WorldPosition.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets::testing {
namespace {

using model::gameobjects::player::CustomPlayerState;
using model::gameobjects::state::CreatureState;
using serverpackets::SM_EMOTION;
using serverpackets::SM_STATS_INFO;
using serverpackets::SM_SYSTEM_MESSAGE;
using services::player::PlayerReviveService;
using services::teleport::TeleportService;

/**
 * The elyos_spawn_location of this test. It is NOT the real (1212.94, 1044.85, 140.76) of player_initial_data.xml: the world chunk's test
 * Poeta is 1024 x 1024 and World::setPosition refuses coordinates outside the map, so the fixture uses a spot inside it. What the assertions
 * below check is that moveToBindLocation reads this holder at all and hands every one of its five values down the teleportTo chain.
 */
constexpr int32_t ELYOS_SPAWN_MAP = 210010000;
constexpr float ELYOS_SPAWN_X = 812.5f;
constexpr float ELYOS_SPAWN_Y = 644.25f;
constexpr float ELYOS_SPAWN_Z = 140.75568f;
constexpr int8_t ELYOS_SPAWN_HEADING = 32;

inline const char* const PLAYER_INITIAL_DATA_XML = R"(<player_initial_data>
	<asmodian_spawn_location map_id="220010000" heading="17" x="71.0388" y="87.3420" z="299.8750"/>
	<elyos_spawn_location map_id="210010000" heading="32" x="812.5" y="644.25" z="140.75568"/>
	<player_data class="WARRIOR"><items/></player_data>
	<player_data class="MAGE"><items/></player_data>
</player_initial_data>)";

class ReviveTest : public InWorldPacketTest {
protected:
	void SetUp() override {
		InWorldPacketTest::SetUp();
		xml::LoadContext context;
		dataholders::DataManager::PLAYER_INITIAL_DATA.publish(
			xml::bindString<dataholders::PlayerInitialData>(context, PLAYER_INITIAL_DATA_XML));
		actor = makePlayer(400001, 9301, "Reviver");
		watcher = makePlayer(400002, 9302, "Watcher");
		actor.player->getPosition()->setIsSpawned(true);
		watcher.player->getPosition()->setIsSpawned(true);
		client = std::make_unique<TestClient>();
		client->enterWorld(actor);
		(*client)->clearSent();
	}

	void TearDown() override {
		if (actor.player) {
			actor.player->setTarget(nullptr);
			actor.player->setClientConnection(nullptr);
		}
		if (watcher.player)
			watcher.player->setTarget(nullptr);
		client.reset();
		actor = {};
		watcher = {};
		dataholders::DataManager::PLAYER_INITIAL_DATA.resetForTests();
		InWorldPacketTest::TearDown();
	}

	int32_t maxHp() { return actor.player->getLifeStats()->getMaxHp(); }
	int32_t maxMp() { return actor.player->getLifeStats()->getMaxMp(); }

	PlayerFixture actor, watcher;
	std::unique_ptr<TestClient> client;
};

TEST_F(ReviveTest, ReviveSetsTheHpAndMpPercentagesItWasGiven) {
	actor.player->getLifeStats()->setCurrentHp(1);
	runtime::resetUnportedHitsForTests();

	PlayerReviveService::revive(*actor.player, 30, 30, false, 0);

	// Java: setCurrentHpPercent(hpPercent) / setCurrentMpPercent(mpPercent), i.e. maxHp * percent / 100 (CreatureLifeStats.cpp:285-287)
	EXPECT_EQ(actor.player->getLifeStats()->getCurrentHp(), static_cast<int32_t>(static_cast<int64_t>(maxHp()) * 30 / 100));
	EXPECT_EQ(actor.player->getLifeStats()->getCurrentMp(), static_cast<int32_t>(static_cast<int64_t>(maxMp()) * 30 / 100));
	EXPECT_FALSE(actor.player->isDead());
	// the isNoResurrectPenalty guard (PlayerReviveService.java:194), skipped at M5b-1 and ported in M5b-2 (docs/deviations/P5-08.md): the
	// effect controller answers through the ported hasAbnormalEffect(predicate), and a character without a NoResurrectPenaltyEffect keeps the
	// percentages it was given. The character with one is effects_mz OtherEffectsTest.AReviveUnderNoResurrectPenaltyIsFullAndBringsNoSoulSickness
	EXPECT_EQ(runtime::unportedHitCount(), 0u) << "the guard reaches no unported body";
	EXPECT_NE(actor.player->getLifeStats()->getCurrentHp(), maxHp()) << "no NoResurrectPenaltyEffect: the guard answers false, not true";
}

TEST_F(ReviveTest, ReviveClearsTheResurrectionSkillAndTheResFlag) {
	actor.player->setResurrectionSkill(1234);
	actor.player->setPlayerResActivate(true);

	PlayerReviveService::revive(*actor.player, 25, 25, false, 0);

	EXPECT_EQ(actor.player->getResurrectionSkill(), 0) << "PlayerReviveService.java:203";
	EXPECT_FALSE(actor.player->getResStatus()) << "setPlayerResActivate(false), PlayerReviveService.java:195";
	// Two statements of the body are deliberately NOT asserted here, because no assertion of this fixture could see them fail:
	// - `if (getDp() > 0 && !isNoResurrectPenalty) setDp(0)` (PlayerReviveService.java:198-199). A starting class can hold no DP at all:
	//   PlayerCommonData::setDp returns before writing for isStartingClass (PlayerCommonData.cpp:228-230), so getDp() is 0 for this
	//   WARRIOR whatever revive does. Only a second-class character could tell the two apart.
	// - `player.getAggroList().clear()` (:204). Filling an AggroList needs AggroList::addHate/addDamage, which are the attack-math lane's
	//   B-04 and not merged with this one. The gate's D1b covers the emptied list.
}

TEST_F(ReviveTest, ReviveMakesTheCharacterStopBeingACorpse) {
	// the shape a character is in when the client answers the death window: HP 0 and the DEAD creature state that
	// CreatureController::onDie set (CreatureController.cpp:214). DeadPlayerLifeStats is the fixture's 0 HP life stats whose onHpChanged is
	// overridden away, so writing the HP does not run the death path a second time (InWorldPacketRunSupport.h:106-117).
	actor.player->setLifeStats(std::make_unique<DeadPlayerLifeStats>(*actor.player));
	ASSERT_TRUE(actor.player->isDead());
	// `replace = true` writes the state instead of OR-ing it, exactly as CreatureController::onDie leaves it after an ordinary ACTIVE
	actor.player->setState(CreatureState::DEAD, true);
	ASSERT_TRUE(actor.player->isInState(CreatureState::DEAD));
	ASSERT_EQ(actor.player->getState(), model::gameobjects::state::getId(CreatureState::DEAD));

	PlayerReviveService::revive(*actor.player, 25, 25, false, 0);

	// Java: player.getController().onBeforeSpawn() (PlayerReviveService.java:205). The percentages two lines earlier already made isDead()
	// false, so PlayerController::onBeforeSpawn takes its `!isDead()` arm and swaps DEAD for ACTIVE (PlayerController.cpp:473-486). Without
	// that call the character is alive by HP and a corpse by state, and the client keeps him lying down.
	// The whole state word is asserted, not isInState(ACTIVE): CreatureState.DEAD is ACTIVE + FLYING + RESTING (CreatureStateInfo.h:42), so
	// isInState(ACTIVE) answers true for a corpse too and would pass with onBeforeSpawn deleted.
	EXPECT_FALSE(actor.player->isInState(CreatureState::DEAD)) << "PlayerReviveService.java:205";
	EXPECT_EQ(actor.player->getState(), model::gameobjects::state::getId(CreatureState::ACTIVE));
}

TEST_F(ReviveTest, ReviveDropsThePanesterraFactionOfACharacterOutsidePanesterra) {
	actor.player->setPanesterraFaction(services::panesterra::ahserion::PanesterraFaction::BELUS);
	ASSERT_TRUE(actor.player->getPanesterraFaction());
	ASSERT_FALSE(world::isPanesterraMap(actor.player->getWorldId())) << "the fixture stands on Poeta";

	PlayerReviveService::revive(*actor.player, 25, 25, false, 0);

	// the second half of PlayerController::onBeforeSpawn (PlayerController.cpp:484-485, Java PlayerController.java:392): a character who left
	// Panesterra loses the faction on his next spawn. It is asserted here as well as the state swap above because the two halves of
	// onBeforeSpawn are independent - a port that kept only the first would pass the corpse test.
	EXPECT_FALSE(actor.player->getPanesterraFaction());
}

TEST_F(ReviveTest, ReviveDropsTheRevivedPlayerAsEveryoneElsesTarget) {
	ASSERT_TRUE(actor.knownList().addForTest(*watcher.player));
	watcher.player->setTarget(runtime::Ptr<model::gameobjects::VisibleObject>(*actor.player));
	ASSERT_TRUE(watcher.player->getTarget());

	PlayerReviveService::revive(*actor.player, 25, 25, false, 0);

	// Java: knownList.forEachPlayer(p -> { if (player.equals(p.getTarget())) p.setTarget(null); })
	EXPECT_FALSE(watcher.player->getTarget()) << "PlayerReviveService.java:190-193";
}

TEST_F(ReviveTest, ReviveKeepsATargetThatIsSomebodyElse) {
	ASSERT_TRUE(actor.knownList().addForTest(*watcher.player));
	watcher.player->setTarget(runtime::Ptr<model::gameobjects::VisibleObject>(*watcher.player)); // targets himself, not the revived player

	PlayerReviveService::revive(*actor.player, 25, 25, false, 0);

	EXPECT_EQ(watcher.player->getTarget(), runtime::Ptr<model::gameobjects::VisibleObject>(*watcher.player))
		<< "only the revived player's own watchers lose their target";
}

TEST_F(ReviveTest, ReviveBroadcastsTheResurrectEmotionToHimself) {
	actor.player->getLifeStats()->setCurrentHp(1);
	(*client)->clearSent();

	PlayerReviveService::revive(*actor.player, 25, 25, false, 0);

	// Java: PacketSendUtility.broadcastPacket(player, new SM_EMOTION(player, EmotionType.RESURRECT), true) - the `true` is "to self"
	const std::vector<std::vector<uint8_t>> resurrect = {serialized(SM_EMOTION(*actor.player, model::EmotionType::RESURRECT), client->con())};
	EXPECT_EQ((*client)->sentBytes().back(), resurrect.back()) << "the last packet of revive is the resurrect emotion";
}

TEST_F(ReviveTest, BindReviveInEventModeRevivesToFullAndTeleportsToTheEvent) {
	actor.player->setCustomState(CustomPlayerState::EVENT_MODE);
	actor.player->getLifeStats()->setCurrentHp(1);
	runtime::resetUnportedHitsForTests();

	// Java bindRevive: EVENT_MODE takes revive(player, 100, 100, false, skillId) and then TeleportService.teleportToEvent(player), which is
	// still AION_UNPORTED (P5-08, no event location at M5b-1). The throw is what proves the branch, and every statement before it ran.
	try {
		PlayerReviveService::bindRevive(*actor.player, 0);
		FAIL() << "teleportToEvent is unported";
	} catch (const runtime::UnportedException& unported) {
		EXPECT_NE(std::string(unported.what()).find("teleportToEvent"), std::string::npos) << unported.what();
	}
	EXPECT_EQ(actor.player->getLifeStats()->getCurrentHp(), maxHp()) << "EVENT_MODE revives to 100 %, not to 25 %";
	EXPECT_EQ(actor.player->getLifeStats()->getCurrentMp(), maxMp());
}

TEST_F(ReviveTest, BindReviveWithASkillIdSendsTheRebirthMessage) {
	actor.player->setCustomState(CustomPlayerState::EVENT_MODE);
	actor.player->getLifeStats()->setCurrentHp(1);
	(*client)->clearSent();

	EXPECT_THROW(PlayerReviveService::bindRevive(*actor.player, 7), runtime::UnportedException);

	// Java: `if (skillId > 0) sendPacket(STR_REBIRTH_MASSAGE_ME())`, after revive and before the teleport branch
	const std::vector<uint8_t> rebirth = serialized(SM_SYSTEM_MESSAGE::STR_REBIRTH_MASSAGE_ME(), client->con());
	const std::vector<std::vector<uint8_t>> sent = (*client)->sentBytes();
	EXPECT_NE(std::find(sent.begin(), sent.end(), rebirth), sent.end()) << "no STR_REBIRTH_MASSAGE_ME was sent";
}

TEST_F(ReviveTest, BindReviveUpdatesTheStatsVisuallyBeforeItTeleports) {
	actor.player->setCustomState(CustomPlayerState::EVENT_MODE);
	actor.player->getLifeStats()->setCurrentHp(1);
	(*client)->clearSent();

	EXPECT_THROW(PlayerReviveService::bindRevive(*actor.player, 0), runtime::UnportedException);

	// Java: player.getGameStats().updateStatsAndSpeedVisually() (PlayerReviveService.java:112), between the revive and the teleport branch.
	// PlayerGameStats::updateStatsAndSpeedVisually -> onStatsChange(null) -> updateStatsVisually -> updateStatInfo ->
	// sendPacket(SM_STATS_INFO) (PlayerGameStats.cpp:80-92, 360-363). Nothing else on this path sends it: revive's own HP and MP writes send
	// SM_ATTACK_STATUS and SM_STATUPDATE_HP, and the EVENT_MODE arm throws in teleportToEvent before spawnOnSameMap could send one.
	const std::vector<uint8_t> statsInfo = serialized(SM_STATS_INFO(*actor.player), client->con());
	const std::vector<std::vector<uint8_t>> sent = (*client)->sentBytes();
	EXPECT_NE(std::find(sent.begin(), sent.end(), statsInfo), sent.end()) << "no SM_STATS_INFO was sent";
}

/*
 * bindRevive's last statement, `player.unsetResPosState()` (PlayerReviveService.java:132), has no test here and cannot have one at M5b-1.
 * It runs only when the whole if-chain above it finished, and every arm of that chain but one ends in an AION_UNPORTED body
 * (teleportToPrison, teleportToEvent, reviveInEventLocation). The one arm that can finish - an ordinary character on an ordinary map - is
 * entered with setSoulSickness = true, which reaches PlayerController::updateSoulSickness, whose FIRST statement is
 * `player.getActiveHouse()` (PlayerController.cpp:812, Java PlayerController.java:714). That loads the character's houses through
 * HousingService, whose constructor reads the houses and the used player ids out of the database (HousingService.cpp:66-72), so it throws in
 * a unit test whatever gameserver.soulsickness.disable is set to. The permission that skips the soul-sickness skill itself
 * (config/m5b.properties.example) is checked one statement LATER and cannot help here.
 * The statement is therefore the M5b-1 gate's P2 to assert: after CM_REVIVE(BIND_REVIVE) the character's res-position state must be off.
 */

TEST_F(ReviveTest, BindReviveWithoutASkillIdSendsNoRebirthMessage) {
	actor.player->setCustomState(CustomPlayerState::EVENT_MODE);
	actor.player->getLifeStats()->setCurrentHp(1);
	(*client)->clearSent();

	EXPECT_THROW(PlayerReviveService::bindRevive(*actor.player, 0), runtime::UnportedException);

	const std::vector<uint8_t> rebirth = serialized(SM_SYSTEM_MESSAGE::STR_REBIRTH_MASSAGE_ME(), client->con());
	const std::vector<std::vector<uint8_t>> sent = (*client)->sentBytes();
	EXPECT_EQ(std::find(sent.begin(), sent.end(), rebirth), sent.end()) << "skillId 0 must not send the rebirth message";
}

/**
 * The teleport half: a character really standing in a World map, moved by TeleportService::moveToBindLocation. No client connection, so every
 * PacketSendUtility::sendPacket of spawnOnSameMap returns early (see the file header).
 */
class TeleportOnSameMapTest : public InWorldPacketTest {
protected:
	void SetUp() override {
		InWorldPacketTest::SetUp();
		ASSERT_TRUE(world::test::publishTestStaticData());
		xml::LoadContext context;
		dataholders::DataManager::PLAYER_INITIAL_DATA.publish(
			xml::bindString<dataholders::PlayerInitialData>(context, PLAYER_INITIAL_DATA_XML));
		actor = makePlayer(410001, 9401, "Traveller");
		actor.player->setMotions(std::make_unique<model::gameobjects::player::motion::MotionList>(*actor.player));
		world::World& world = world::World::getInstance();
		world.storeObject(*actor.player);
		ASSERT_TRUE(world.setPosition(*actor.player, ELYOS_SPAWN_MAP, 300.0f, 300.0f, 10.0f, int8_t{0}));
		world.spawn(runtime::Ptr<model::gameobjects::VisibleObject>(*actor.player));
		ASSERT_TRUE(actor.player->isSpawned());
	}

	void TearDown() override {
		if (actor.player) {
			if (actor.player->isSpawned())
				world::World::getInstance().despawn(*actor.player);
			world::World::getInstance().removeObject(*actor.player);
			actor.player->setTarget(nullptr);
		}
		actor = {};
		dataholders::DataManager::PLAYER_INITIAL_DATA.resetForTests();
		InWorldPacketTest::TearDown();
	}

	PlayerFixture actor;
};

TEST_F(TeleportOnSameMapTest, MoveToBindLocationUsesTheRacesSpawnLocationWithoutABindPoint) {
	ASSERT_FALSE(actor.player->getBindPoint());
	actor.player->setTarget(runtime::Ptr<model::gameobjects::VisibleObject>(*actor.player));
	runtime::resetUnportedHitsForTests();

	TeleportService::moveToBindLocation(*actor.player);

	// Java moveToBindLocation: no bind point -> DataManager.PLAYER_INITIAL_DATA.getSpawnLocation(race), then teleportTo(worldId, x, y, z, h)
	EXPECT_EQ(actor.player->getWorldId(), ELYOS_SPAWN_MAP);
	EXPECT_FLOAT_EQ(actor.player->getX(), ELYOS_SPAWN_X);
	EXPECT_FLOAT_EQ(actor.player->getY(), ELYOS_SPAWN_Y);
	EXPECT_FLOAT_EQ(actor.player->getZ(), ELYOS_SPAWN_Z);
	EXPECT_EQ(actor.player->getHeading(), ELYOS_SPAWN_HEADING);
	// sendLoc -> abortPlayerActions clears the target before the despawn (TeleportService.java:195-206)
	EXPECT_FALSE(actor.player->getTarget());
	// the same map and the same instance, so SpawnTask takes the spawnOnSameMap arm and the character is in the world again
	EXPECT_TRUE(actor.player->isSpawned()) << "TeleportService.java:524-526 respawns instead of waiting for CM_LEVEL_READY";
	EXPECT_EQ(runtime::unportedHitCount(), 0u) << "the same-map teleport must reach no unported body";
}

TEST_F(TeleportOnSameMapTest, MoveToBindLocationPrefersTheBindPoint) {
	actor.player->setBindPoint(model::gameobjects::player::BindPointPosition::create(ELYOS_SPAWN_MAP, 500.0f, 400.0f, 12.5f, int8_t{7}));

	TeleportService::moveToBindLocation(*actor.player);

	EXPECT_FLOAT_EQ(actor.player->getX(), 500.0f);
	EXPECT_FLOAT_EQ(actor.player->getY(), 400.0f);
	EXPECT_FLOAT_EQ(actor.player->getZ(), 12.5f);
	EXPECT_EQ(actor.player->getHeading(), 7) << "the bind point's heading, not the spawn location's";
}

TEST_F(TeleportOnSameMapTest, TeleportToKeepsTheInstanceOfTheSameWorld) {
	// the second twin of the test Ishalgen (world_maps.xml twin_count="2"), so that "keep the player's instance" and "always instance 1" are
	// two different answers; on Poeta, which has one instance, the ternary of TeleportService.java:258 cannot be observed at all
	constexpr int32_t TWIN_MAP = 220010000;
	world::World& world = world::World::getInstance();
	world.despawn(*actor.player);
	ASSERT_TRUE(world.setPosition(runtime::Ptr<model::gameobjects::VisibleObject>(*actor.player), TWIN_MAP, 2, 100.0f, 100.0f, 10.0f, int8_t{0}));
	world.spawn(runtime::Ptr<model::gameobjects::VisibleObject>(*actor.player));
	ASSERT_EQ(actor.player->getInstanceId(), 2);

	// Java teleportTo(player, worldId, x, y, z, h, animation): `player.getWorldId() != worldId ? 1 : player.getInstanceId()`
	TeleportService::teleportTo(*actor.player, TWIN_MAP, 120.0f, 130.0f, 11.0f, int8_t{3});

	EXPECT_EQ(actor.player->getInstanceId(), 2) << "the same world keeps the player's own instance";
	EXPECT_FLOAT_EQ(actor.player->getX(), 120.0f);
	EXPECT_EQ(actor.player->getHeading(), 3);
}

TEST_F(TeleportOnSameMapTest, TeleportToWithoutAHeadingKeepsThePlayersOwn) {
	ASSERT_TRUE(world::World::getInstance().setPosition(*actor.player, ELYOS_SPAWN_MAP, 300.0f, 300.0f, 10.0f, int8_t{42}));
	ASSERT_EQ(actor.player->getHeading(), 42);

	// Java teleportTo(player, worldId, x, y, z): the 5-argument overload passes player.getHeading()
	TeleportService::teleportTo(*actor.player, ELYOS_SPAWN_MAP, 310.0f, 320.0f, 11.0f);

	EXPECT_EQ(actor.player->getHeading(), 42);
	EXPECT_FLOAT_EQ(actor.player->getY(), 320.0f);
}

} // namespace
} // namespace aion::gameserver::network::aion::clientpackets::testing
