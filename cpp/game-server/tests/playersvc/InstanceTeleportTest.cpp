// The instance side of TeleportService and PlayerReviveService on the ascension route (P5-08, m5f-plan.md §15, items T-02's subset and T-06):
// the WorldMapInstance overloads the quest handlers enter Karamatis B / Ataxiar B with, the (worldId, instanceId, x, y, z, h) overload,
// moveToInstanceExit, and instanceRevive.
//
// Java: TeleportService.java:265-279 (the overloads), :394-403 (moveToInstanceExit); PlayerReviveService.java:157-187 (instanceRevive).
// The fixture (tests/instance/AscensionTestSupport.h) is a World of the real map rows with the real spawn, cooltime and exit rows.
//
// Not covered here, said in place:
// - the (worldId, instanceId, x, y, z) overload (TeleportService.cpp:455) stays AION_UNPORTED in this lane: DialogServiceTest.cpp:1109-1110 pins
//   it as throwing, and that file is leased by M5c stage 2's craft lane (m5f-plan.md §15.4).
// - moveToInstanceExit's `instanceExists(exitWorld, 1)` term: every exit world of instance_exit.xml is an open map with an instance 1, so no
//   shipped row reaches the false arm (a map World does not know is Java's NullPointerException in instanceExists, not that arm).
// - instanceRevive's EVENT_MODE `return`: the arm ends in TeleportService::teleportToEvent, still AION_UNPORTED, which throws first.
// - instanceRevive's `map == null` arm: a spawned player's map always exists.
// - a logout of a player dead inside an instance (PlayerLeaveWorldService.cpp:119-120) runs the whole leave-world path with its DAO saves; the
//   two arms it reaches are the startPos and bind arms below.

#include "../instance/AscensionTestSupport.h"

#include <algorithm>
#include <cstdint>
#include <memory>
#include <string>
#include <typeinfo>
#include <unordered_map>
#include <vector>

#include "aion/gameserver/configs/main/MembershipConfig.h"
#include "aion/gameserver/controllers/PlayerController.h"
#include "aion/gameserver/instance/handlers/GeneralInstanceHandler.h"
#include "aion/gameserver/model/TaskId.h"
#include "aion/gameserver/model/animations/ArrivalAnimationInfo.h"
#include "aion/gameserver/model/animations/TeleportAnimation.h"
#include "aion/gameserver/model/gameobjects/player/CustomPlayerState.h"
#include "aion/gameserver/model/gameobjects/player/PlayerCommonData.h"
#include "aion/gameserver/model/gameobjects/player/motion/Motion.h"
#include "aion/gameserver/model/stats/container/PlayerLifeStats.h"
#include "aion/gameserver/network/aion/serverpackets/SM_CHANNEL_INFO.h"
#include "aion/gameserver/network/aion/serverpackets/SM_MOTION.h"
#include "aion/gameserver/network/aion/serverpackets/SM_PLAYER_INFO.h"
#include "aion/gameserver/network/aion/serverpackets/SM_PLAYER_SPAWN.h"
#include "aion/gameserver/network/aion/serverpackets/SM_STATS_INFO.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/network/aion/serverpackets/SM_TELEPORT_LOC.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/services/instance/InstanceService.h"
#include "aion/gameserver/services/player/PlayerReviveService.h"
#include "aion/gameserver/services/teleport/TeleportService.h"
#include "aion/gameserver/world/World.h"
#include "aion/gameserver/world/WorldMapInstance.h"
#include "aion/gameserver/world/WorldPosition.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::instance::test {
namespace {

using model::animations::TeleportAnimation;
using network::aion::serverpackets::SM_SYSTEM_MESSAGE;
using services::instance::InstanceService;
using services::player::PlayerReviveService;
using services::teleport::TeleportService;

/** An instance script that takes over the revive (GeneralInstanceHandler.onReviveEvent answers false) */
class RevivingInstanceHandler final : public handlers::GeneralInstanceHandler {
	AION_MAKE_REF_FRIEND
public:
	explicit RevivingInstanceHandler(world::WorldMapInstance& instance) : GeneralInstanceHandler(instance) {}

	static runtime::Ref<RevivingInstanceHandler> create(world::WorldMapInstance& instance) {
		return runtime::makeRef<RevivingInstanceHandler>(instance);
	}

	bool onReviveEvent(model::gameobjects::player::Player&) override {
		++asked;
		return true;
	}

	int asked = 0;

protected:
	~RevivingInstanceHandler() override = default;
};

class InstanceTeleportTest : public AscensionWorldTest {
protected:
	void SetUp() override {
		AscensionWorldTest::SetUp();
		if (IsSkipped())
			return;
		// 1006's beam spot on Poeta (_1006Ascension.java:95)
		spawnActor(POETA, 1, 657.0f, 1071.0f, 99.375f, int8_t{72});
	}

	void expectAt(int32_t mapId, int32_t instanceId, float x, float y, float z) {
		EXPECT_EQ(actor.player->getWorldId(), mapId);
		EXPECT_EQ(actor.player->getInstanceId(), instanceId);
		EXPECT_FLOAT_EQ(actor.player->getX(), x);
		EXPECT_FLOAT_EQ(actor.player->getY(), y);
		EXPECT_FLOAT_EQ(actor.player->getZ(), z);
	}

	/** The character inside the instance and spawned (the cross-map arm waits for CM_LEVEL_READY, which the unit test stands in for) */
	void enter(world::WorldMapInstance& instance, float x, float y, float z) {
		TeleportService::teleportTo(*actor.player, instance, x, y, z, int8_t{0});
		world::World::getInstance().spawn(runtime::Ptr<model::gameobjects::VisibleObject>(*actor.player));
		(*client)->clearSent();
	}

	void die() { actor.player->setLifeStats(std::make_unique<cptest::DeadPlayerLifeStats>(*actor.player)); }

	int countClass(const std::vector<uint8_t>& packet) {
		int n = 0;
		for (const std::vector<uint8_t>& bytes : sent())
			if (headerOf(bytes) == headerOf(packet))
				++n;
		return n;
	}

	/** The index of the first sent packet equal to `packet`, -1 if none was sent */
	int indexOf(const std::vector<uint8_t>& packet) {
		std::vector<std::vector<uint8_t>> bytes = sent();
		auto it = std::find(bytes.begin(), bytes.end(), packet);
		return it == bytes.end() ? -1 : static_cast<int>(it - bytes.begin());
	}

	/** The index of the first sent packet of `packet`'s class after `after`, -1 if none */
	int indexOfClass(const std::vector<uint8_t>& packet, int after) {
		std::vector<std::vector<uint8_t>> bytes = sent();
		for (int i = after + 1; i < static_cast<int>(bytes.size()); ++i)
			if (headerOf(bytes[i]) == headerOf(packet))
				return i;
		return -1;
	}
};

// ---- T-02 --------------------------------------------------------------------------------------------------------------------------------

TEST_F(InstanceTeleportTest, The1006EntryLandsInTheNewKaramatisBInstance) {
	runtime::Ptr<world::WorldMapInstance> instance = InstanceService::getNextAvailableInstance(KARAMATIS_B, *actor.player);

	// _1006Ascension.java:98-99, verbatim
	TeleportService::teleportTo(*actor.player, *instance, 52, 174, 229, int8_t{10}, TeleportAnimation::NONE);

	expectAt(KARAMATIS_B, instance->getInstanceId(), 52.0f, 174.0f, 229.0f);
	EXPECT_EQ(actor.player->getHeading(), 10);
	// SpawnTask.run's map-reloading arm (TeleportService.java:525-531); neither map is personal, so the dungeon message follows
	EXPECT_TRUE(wasSent(cptest::serialized(network::aion::serverpackets::SM_CHANNEL_INFO(actor.player->getPosition()), client->con())));
	EXPECT_TRUE(wasSentClass(cptest::serialized(network::aion::serverpackets::SM_PLAYER_SPAWN(*actor.player), client->con())));
	EXPECT_TRUE(wasSent(cptest::serialized(SM_SYSTEM_MESSAGE::STR_MSG_INSTANCE_DUNGEON_OPENED_FOR_SELF(KARAMATIS_B), client->con())));
}

TEST_F(InstanceTeleportTest, The2008EntryKeepsThePlayersHeading) {
	world::World::getInstance().despawn(*actor.player);
	world::World::getInstance().removeObject(*actor.player);
	actor.player->setClientConnection(nullptr);
	client.reset();
	spawnActor(ISHALGEN, 1, 100.0f, 100.0f, 200.0f, int8_t{33}, model::Race::ASMODIANS);
	runtime::Ptr<world::WorldMapInstance> instance = InstanceService::getNextAvailableInstance(ATAXIAR_B, *actor.player);

	// _2008Ascension.java:131-132, verbatim: the 5-argument overload, TeleportService.java:269-271
	TeleportService::teleportTo(*actor.player, *instance, 457.65f, 426.8f, 230.4f);

	expectAt(ATAXIAR_B, instance->getInstanceId(), 457.65f, 426.8f, 230.4f);
	EXPECT_EQ(actor.player->getHeading(), 33) << "player.getHeading()";
}

TEST_F(InstanceTeleportTest, TheSixArgumentInstanceOverloadTakesTheHeading) {
	runtime::Ptr<world::WorldMapInstance> instance = InstanceService::getNextAvailableInstance(KARAMATIS_B, *actor.player);

	TeleportService::teleportTo(*actor.player, *instance, 60.0f, 180.0f, 229.0f, int8_t{99}); // TeleportService.java:273-275

	expectAt(KARAMATIS_B, instance->getInstanceId(), 60.0f, 180.0f, 229.0f);
	EXPECT_EQ(actor.player->getHeading(), 99);
}

TEST_F(InstanceTeleportTest, TheInstanceIdOverloadLandsInThatInstance) {
	runtime::Ptr<world::WorldMapInstance> instance = InstanceService::getNextAvailableInstance(KARAMATIS_B, *actor.player);

	// TeleportService.java:265-267, (worldId, instanceId, x, y, z, h) with TeleportAnimation.NONE: the move is done at once
	TeleportService::teleportTo(*actor.player, KARAMATIS_B, instance->getInstanceId(), 70.0f, 190.0f, 229.0f, int8_t{7});

	expectAt(KARAMATIS_B, instance->getInstanceId(), 70.0f, 190.0f, 229.0f);
	EXPECT_EQ(actor.player->getHeading(), 7);
	EXPECT_FALSE(actor.player->getController().hasTask(model::TaskId::TELEPORT)) << "NONE: no animation to wait for";
}

TEST_F(InstanceTeleportTest, TheFiveArgumentInstanceIdOverloadKeepsTheHeadingAndMovesAtOnce) {
	world::World::getInstance().despawn(*actor.player);
	world::World::getInstance().removeObject(*actor.player);
	actor.player->setClientConnection(nullptr);
	client.reset();
	spawnActor(POETA, 1, 100.0f, 100.0f, 200.0f, int8_t{44}, model::Race::ELYOS);
	runtime::Ptr<world::WorldMapInstance> instance = InstanceService::getNextAvailableInstance(KARAMATIS_B, *actor.player);

	// TeleportService.java:261-263, (worldId, instanceId, x, y, z): player.getHeading() and TeleportAnimation.NONE. Its callers are
	// DialogService's ENTER_PVP / LEAVE_PVP arms (DialogService.java:161-186), which the owner's client reached at Sanctum's arena npc Epeios.
	TeleportService::teleportTo(*actor.player, KARAMATIS_B, instance->getInstanceId(), 71.0f, 191.0f, 229.0f);

	expectAt(KARAMATIS_B, instance->getInstanceId(), 71.0f, 191.0f, 229.0f);
	EXPECT_EQ(actor.player->getHeading(), 44) << "player.getHeading()";
	EXPECT_FALSE(actor.player->getController().hasTask(model::TaskId::TELEPORT)) << "NONE: no animation to wait for";
}

TEST_F(InstanceTeleportTest, TheWorldPositionOverloadOnTheSameMapMovesAtOnceWithItsHeading) {
	// TeleportService.java:221-226: same map - abortPlayerActions, the pet and the player set to the position, spawnOnSameMap
	const float z = actor.player->getZ();
	runtime::Ref<world::WorldPosition> pos = world::WorldPosition::create(POETA, 120.0f, 130.0f, z, int8_t{21});

	TeleportService::teleportTo(*actor.player, *pos);

	expectAt(POETA, 1, 120.0f, 130.0f, z);
	EXPECT_EQ(actor.player->getHeading(), 21) << "pos.getHeading()";
	EXPECT_TRUE(actor.player->isSpawned()) << "spawnOnSameMap";
}

TEST_F(InstanceTeleportTest, TheWorldPositionOverloadToAnotherMapTeleportsWithoutAnimation) {
	runtime::Ptr<world::WorldMapInstance> instance = InstanceService::getNextAvailableInstance(KARAMATIS_B, *actor.player);
	runtime::Ref<world::WorldPosition> pos =
		world::WorldPosition::create(KARAMATIS_B, 60.0f, 170.0f, 229.0f, int8_t{12}, instance->getRegion(60.0f, 170.0f, 229.0f));

	TeleportService::teleportTo(*actor.player, *pos); // TeleportService.java:229-230: the 8-argument overload with NONE

	expectAt(KARAMATIS_B, instance->getInstanceId(), 60.0f, 170.0f, 229.0f);
	EXPECT_EQ(actor.player->getHeading(), 12);
	EXPECT_FALSE(actor.player->getController().hasTask(model::TaskId::TELEPORT)) << "NONE: no animation to wait for";
	EXPECT_FALSE(actor.player->isDead());
}

TEST_F(InstanceTeleportTest, ADeadPlayerMovedToAnotherMapStaysDeadAndLands) {
	runtime::Ptr<world::WorldMapInstance> instance = InstanceService::getNextAvailableInstance(KARAMATIS_B, *actor.player);
	runtime::Ref<world::WorldPosition> pos =
		world::WorldPosition::create(KARAMATIS_B, 62.0f, 172.0f, 229.0f, int8_t{13}, instance->getRegion(62.0f, 172.0f, 229.0f));
	die();

	// TeleportService.java:227-228 -> teleportDeadTo (:234-247): no revive (the other overloads revive a dead player first, :282-283)
	TeleportService::teleportTo(*actor.player, *pos);

	expectAt(KARAMATIS_B, instance->getInstanceId(), 62.0f, 172.0f, 229.0f);
	EXPECT_TRUE(actor.player->isDead()) << "teleportDeadTo does not revive";
	EXPECT_TRUE(wasSent(cptest::serialized(network::aion::serverpackets::SM_CHANNEL_INFO(actor.player->getPosition()), client->con())));
	EXPECT_TRUE(wasSentClass(cptest::serialized(network::aion::serverpackets::SM_PLAYER_SPAWN(*actor.player), client->con())));
	EXPECT_TRUE(wasSentClass(cptest::serialized(network::aion::serverpackets::SM_PLAYER_INFO(*actor.player), client->con())));
	EXPECT_EQ(actor.player->getPortAnimationId(), model::animations::getId(model::animations::ArrivalAnimation::LANDING));
}

TEST_F(InstanceTeleportTest, TheAnimatedInstanceOverloadWaitsForTheClient) {
	runtime::Ptr<world::WorldMapInstance> instance = InstanceService::getNextAvailableInstance(KARAMATIS_B, *actor.player);

	TeleportService::teleportTo(*actor.player, *instance, 52, 174, 229, int8_t{10}, TeleportAnimation::FADE_OUT_BEAM);

	// sendLoc (TeleportService.java:179-193): despawned, SM_TELEPORT_LOC, and the SpawnTask waits for CM_TELEPORT_ANIMATION_DONE
	EXPECT_FALSE(actor.player->isSpawned());
	EXPECT_EQ(actor.player->getWorldId(), POETA) << "not moved yet";
	EXPECT_TRUE(wasSent(cptest::serialized(network::aion::serverpackets::SM_TELEPORT_LOC(KARAMATIS_B, instance->getInstanceId(), 52, 174, 229,
													 int8_t{10}, TeleportAnimation::FADE_OUT_BEAM),
		client->con())));
	EXPECT_TRUE(actor.player->getController().hasTask(model::TaskId::TELEPORT));
}

TEST_F(InstanceTeleportTest, HaramelsExitDependsOnTheRace) {
	TeleportService::moveToInstanceExit(*actor.player, HARAMEL, model::Race::ELYOS);
	expectAt(VERTERON, 1, 2533.8564f, 835.055f, 103.967476f); // instance_exit.xml:16
	EXPECT_EQ(actor.player->getHeading(), 59);

	TeleportService::moveToInstanceExit(*actor.player, HARAMEL, model::Race::ASMODIANS);
	expectAt(ALTGARD, 1, 2907.624f, 1464.1887f, 252.59264f); // instance_exit.xml:17
	EXPECT_EQ(actor.player->getHeading(), 36);
}

TEST_F(InstanceTeleportTest, KaramatisBHasNoExitRowSoItWarnsAndUsesTheBindLocation) {
	network::test::LogCapture capture({"com.aionemu.gameserver.services.teleport.TeleportService"}, spdlog::level::info);

	TeleportService::moveToInstanceExit(*actor.player, KARAMATIS_B, model::Race::ELYOS);

	EXPECT_TRUE(capture.contains("No instance exit found for race: ELYOS 310020000")) << "TeleportService.java:400";
	// no bind point: player_initial_data.xml:4, the Elyos spawn location
	expectAt(POETA, 1, ELYOS_SPAWN_X, ELYOS_SPAWN_Y, ELYOS_SPAWN_Z);
	EXPECT_EQ(actor.player->getHeading(), ELYOS_SPAWN_HEADING);
}

TEST_F(InstanceTeleportTest, ARelogAfterTheInstanceIsGoneMovesToTheExit) {
	runtime::Ptr<world::WorldMapInstance> instance = InstanceService::getNextAvailableInstance(KARAMATIS_B, *actor.player);
	enter(*instance, 52.0f, 174.0f, 229.0f);
	instance->getRegisteredObjects().remove(actor.player->getObjectId()); // as after the instance was destroyed: not registered anywhere

	// InstanceService.onPlayerLogin (InstanceService.java:144-153) -> moveToExitPoint -> moveToInstanceExit(player, 310020000, ELYOS)
	InstanceService::onPlayerLogin(*actor.player);

	expectAt(POETA, 1, ELYOS_SPAWN_X, ELYOS_SPAWN_Y, ELYOS_SPAWN_Z);
}

// ---- T-06 --------------------------------------------------------------------------------------------------------------------------------

TEST_F(InstanceTeleportTest, InstanceReviveMovesToTheStartPosition) {
	runtime::Ptr<world::WorldMapInstance> instance = InstanceService::getNextAvailableInstance(KARAMATIS_B, *actor.player);
	enter(*instance, 100.0f, 200.0f, 229.0f);
	instance->setStartPos(world::WorldPosition::create(KARAMATIS_B, 52.0f, 174.0f, 229.0f, int8_t{10}));
	die();
	actor.player->setResPosState(true);

	PlayerReviveService::instanceRevive(*actor.player);

	EXPECT_FALSE(actor.player->isDead());
	EXPECT_EQ(actor.player->getLifeStats()->getHpPercentage(), 25) << "revive(player, 25, 25, true, skillId)";
	EXPECT_EQ(actor.player->getLifeStats()->getCurrentMp(), actor.player->getLifeStats()->getMaxMp() * 25 / 100)
		<< "revive(player, 25, 25, true, skillId): setCurrentMpPercent(25)";
	const int rebirth = indexOf(cptest::serialized(SM_SYSTEM_MESSAGE::STR_REBIRTH_MASSAGE_ME(), client->con()));
	ASSERT_GE(rebirth, 0);
	// PlayerReviveService.java:177, updateStatsAndSpeedVisually: SM_STATS_INFO between the rebirth message and instanceRevive's SM_PLAYER_INFO
	const int stats = indexOfClass(cptest::serialized(network::aion::serverpackets::SM_STATS_INFO(*actor.player), client->con()), rebirth);
	const int playerInfo = indexOfClass(cptest::serialized(network::aion::serverpackets::SM_PLAYER_INFO(*actor.player), client->con()), rebirth);
	EXPECT_GT(stats, rebirth);
	EXPECT_LT(stats, playerInfo);
	// instanceRevive's own SM_PLAYER_INFO and SM_MOTION (PlayerReviveService.java:178-179), then spawnOnSameMap's pair of the startPos move
	EXPECT_GE(countClass(cptest::serialized(network::aion::serverpackets::SM_PLAYER_INFO(*actor.player), client->con())), 2);
	EXPECT_GE(countClass(cptest::serialized(network::aion::serverpackets::SM_MOTION(actor.player->getObjectId(),
		std::unordered_map<int32_t, runtime::Ptr<model::gameobjects::player::motion::Motion>>{}), client->con())), 2);
	// startPos on the same map: the 5-argument teleportTo keeps the instance and moves at once
	expectAt(KARAMATIS_B, instance->getInstanceId(), 52.0f, 174.0f, 229.0f);
	EXPECT_FALSE(actor.player->isInResPostState()) << "player.unsetResPosState()";
}

TEST_F(InstanceTeleportTest, WithoutAStartPositionInstanceReviveFallsBackToTheBindRevive) {
	runtime::Ptr<world::WorldMapInstance> instance = InstanceService::getNextAvailableInstance(KARAMATIS_B, *actor.player);
	enter(*instance, 100.0f, 200.0f, 229.0f);
	ASSERT_FALSE(instance->getStartPos()) << "the ascension case: getNextAvailableInstance never sets it (m5f-plan.md §15.3)";
	die();

	PlayerReviveService::instanceRevive(*actor.player);

	// revive, then bindRevive (which revives a second time, as Java does) and moveToBindLocation: player_initial_data's Elyos spawn on Poeta
	EXPECT_FALSE(actor.player->isDead());
	expectAt(POETA, 1, ELYOS_SPAWN_X, ELYOS_SPAWN_Y, ELYOS_SPAWN_Z);
}

TEST_F(InstanceTeleportTest, AnInstanceScriptMayTakeOverTheRevive) {
	runtime::Ptr<world::WorldMapInstance> instance = InstanceService::getNextAvailableInstance(KARAMATIS_B, 0, int8_t{0},
		[](world::WorldMapInstance& created) -> runtime::Ref<handlers::InstanceHandler> { return RevivingInstanceHandler::create(created); },
		1, false);
	enter(*instance, 100.0f, 200.0f, 229.0f);
	die();

	PlayerReviveService::instanceRevive(*actor.player);

	EXPECT_EQ(dynamic_cast<RevivingInstanceHandler&>(*instance->getInstanceHandler()).asked, 1);
	EXPECT_TRUE(actor.player->isDead()) << "PlayerReviveService.java:169-170: the handler answered true, return";
	EXPECT_TRUE(sent().empty());
}

TEST_F(InstanceTeleportTest, InEventModeInstanceReviveRevivesFullyAndGoesToTheEvent) {
	runtime::Ptr<world::WorldMapInstance> instance = InstanceService::getNextAvailableInstance(KARAMATIS_B, *actor.player);
	enter(*instance, 100.0f, 200.0f, 229.0f);
	die();
	actor.player->setCustomState(model::gameobjects::player::CustomPlayerState::EVENT_MODE);
	runtime::resetUnportedHitsForTests();

	EXPECT_THROW(PlayerReviveService::instanceRevive(*actor.player), runtime::UnportedException) << "TeleportService.teleportToEvent";

	EXPECT_EQ(actor.player->getLifeStats()->getHpPercentage(), 100) << "revive(player, 100, 100, false, skillId)";
	const int rebirth = indexOf(cptest::serialized(SM_SYSTEM_MESSAGE::STR_REBIRTH_MASSAGE_ME(), client->con()));
	EXPECT_GE(rebirth, 0);
	// PlayerReviveService.java:165, updateStatsAndSpeedVisually before the teleport to the event
	EXPECT_GT(indexOfClass(cptest::serialized(network::aion::serverpackets::SM_STATS_INFO(*actor.player), client->con()), rebirth), rebirth);
	actor.player->unsetCustomState(model::gameobjects::player::CustomPlayerState::EVENT_MODE);
}

TEST_F(InstanceTeleportTest, InstanceReviveMakesTheCharacterSoulSick) {
	// gameserver.soulsickness.disable is 10 in the shipped membership.properties (the membership that spares a character); the test process
	// loads no properties, so the field is 0 there and spares every account
	const int8_t disableSoulSickness = configs::main::MembershipConfig::DISABLE_SOULSICKNESS.load();
	configs::main::MembershipConfig::DISABLE_SOULSICKNESS.store(10);
	runtime::Ptr<world::WorldMapInstance> instance = InstanceService::getNextAvailableInstance(KARAMATIS_B, *actor.player);
	enter(*instance, 100.0f, 200.0f, 229.0f);
	instance->setStartPos(world::WorldPosition::create(KARAMATIS_B, 52.0f, 174.0f, 229.0f, int8_t{10}));
	die();
	ASSERT_EQ(actor.player->getCommonData()->getDeathCount(), 0);

	// revive(player, 25, 25, true, skillId) -> PlayerController.updateSoulSickness: the death is counted, then Soul Sickness (8291) is cast at
	// that level. This fixture has no skill data, so SkillEngine.getSkill answers null and the cast throws Java's NullPointerException
	// (PlayerController.java: getSkill(...).useSkill()), after the count
	EXPECT_THROW(PlayerReviveService::instanceRevive(*actor.player), runtime::NullPointerException);

	EXPECT_EQ(actor.player->getCommonData()->getDeathCount(), 1);
	configs::main::MembershipConfig::DISABLE_SOULSICKNESS.store(disableSoulSickness);
}

} // namespace
} // namespace aion::gameserver::instance::test
