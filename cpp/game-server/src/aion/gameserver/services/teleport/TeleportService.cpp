#include "aion/gameserver/services/teleport/TeleportService.h"

#include <optional>
#include <string>
#include <unordered_map>

#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/gameserver/runtime/sched/Future.h"
#include "aion/gameserver/runtime/sched/Pin.h"
#include "aion/commons/logging/LoggerFactory.h"
#include "aion/gameserver/controllers/FlyController.h"
#include "aion/gameserver/controllers/PlayerController.h"
#include "aion/gameserver/controllers/effect/PlayerEffectController.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/PlayerInitialData.h"
#include "aion/gameserver/dataholders/WorldMapsData.h"
#include "aion/gameserver/model/TaskId.h"
#include "aion/gameserver/model/actions/PlayerMode.h"
#include "aion/gameserver/model/animations/ArrivalAnimation.h"
#include "aion/gameserver/model/animations/TeleportAnimation.h"
#include "aion/gameserver/model/animations/TeleportAnimationInfo.h"
#include "aion/gameserver/model/gameobjects/Kisk.h"
#include "aion/gameserver/model/gameobjects/Pet.h"
#include "aion/gameserver/model/gameobjects/player/BindPointPosition.h"
#include "aion/gameserver/model/gameobjects/player/motion/Motion.h"
#include "aion/gameserver/model/gameobjects/player/motion/MotionList.h"
#include "aion/gameserver/model/team/legion/LegionMember.h"
#include "aion/gameserver/model/templates/world/WorldMapTemplate.h"
#include "aion/gameserver/network/aion/serverpackets/SM_BIND_POINT_INFO.h"
#include "aion/gameserver/network/aion/serverpackets/SM_CHANNEL_INFO.h"
#include "aion/gameserver/network/aion/serverpackets/SM_MOTION.h"
#include "aion/gameserver/network/aion/serverpackets/SM_PLAYER_INFO.h"
#include "aion/gameserver/network/aion/serverpackets/SM_PLAYER_SPAWN.h"
#include "aion/gameserver/network/aion/serverpackets/SM_STATS_INFO.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/network/aion/serverpackets/SM_TELEPORT_LOC.h"
#include "aion/gameserver/services/DuelService.h"
#include "aion/gameserver/services/LegionService.h"
#include "aion/gameserver/services/PrivateStoreService.h"
#include "aion/gameserver/services/RecallService.h"
#include "aion/gameserver/services/conquerorAndProtectorSystem/ConquerorAndProtectorService.h"
#include "aion/gameserver/services/instance/InstanceService.h"
#include "aion/gameserver/services/player/PlayerReviveService.h"
#include "aion/gameserver/utils/PacketSendUtility.h"
#include "aion/gameserver/world/World.h"
#include "aion/gameserver/world/WorldMapTypeInfo.h"
#include "aion/gameserver/world/WorldPosition.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::services::teleport {

// Anonymous classes and stored lambdas of the Java class (hub-headers.md §7.3): the bodies that create them define the structs that
// `python tools/gen/fieldmap.py --class <key>` prints.
// anonymous RequestResponseHandler at TeleportService.java:467 (com.aionemu.gameserver.services.teleport.TeleportService$1); local handler; storage:
// stored in ResponseRequester

static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.services.teleport.TeleportService");

// Java implements Runnable. teleportTo wraps it in a FutureTask kept as the player's TELEPORT task, and CM_TELEPORT_ANIMATION_DONE runs it on
// a later packet, so it is K4 (fieldmap.toml [kinds]): RefCounted, created with create(), retaining the player. The cycle player controller
// tasks -> Future -> task -> player is cut when the task runs or CreatureController.cancelAllTasks cancels it (design §5.1 Tasks).
class TeleportService::SpawnTask final : public runtime::RefCounted {
	AION_MAKE_REF_FRIEND
private:
	const runtime::Ref<model::gameobjects::player::Player> player;
	const int32_t worldId;
	const int32_t instanceId;
	const float x;
	const float y;
	const float z;
	const int8_t h;
	const model::animations::TeleportAnimation animation;

protected:
	SpawnTask(model::gameobjects::player::Player& player, int32_t worldId, int32_t instanceId, float x, float y, float z, int8_t h,
		model::animations::TeleportAnimation animation);
	~SpawnTask() override;

public:
	/** Java: new SpawnTask(player, worldId, instanceId, x, y, z, h, animation) */
	static runtime::Ref<SpawnTask> create(model::gameobjects::player::Player& player, int32_t worldId, int32_t instanceId, float x, float y,
		float z, int8_t h, model::animations::TeleportAnimation animation);
	void run();
};

TeleportService::SpawnTask::SpawnTask(model::gameobjects::player::Player& value, int32_t worldIdValue, int32_t instanceIdValue, float xValue,
	float yValue, float zValue, int8_t hValue, model::animations::TeleportAnimation animationValue)
	: player(value), worldId(worldIdValue), instanceId(instanceIdValue), x(xValue), y(yValue), z(zValue), h(hValue), animation(animationValue) {
}

TeleportService::SpawnTask::~SpawnTask() = default;

runtime::Ref<TeleportService::SpawnTask> TeleportService::SpawnTask::create(model::gameobjects::player::Player& value, int32_t worldIdValue,
	int32_t instanceIdValue, float xValue, float yValue, float zValue, int8_t hValue, model::animations::TeleportAnimation animationValue) {
	return runtime::makeRef<SpawnTask>(value, worldIdValue, instanceIdValue, xValue, yValue, zValue, hValue, animationValue);
}

// Java TeleportService.java:500-536
void TeleportService::SpawnTask::run() {
	if (player->isSpawned())
		return;

	if (animation != model::animations::TeleportAnimation::NONE) { // this is a delayed teleport (triggered after animation end)
		// instance might be destroyed after animation end if unlucky
		if (player->isDead() || !instance::InstanceService::instanceExists(worldId, instanceId)) {
			utils::PacketSendUtility::sendPacket(*player, network::aion::serverpackets::SM_PLAYER_INFO(*player));
			world::World::getInstance().spawn(runtime::Ptr<model::gameobjects::VisibleObject>(*player));
			return;
		}
		abortPlayerActions(*player);
	}

	int32_t currentWorldId = player->getWorldId();
	int32_t currentInstance = player->getInstanceId();
	if (currentWorldId != worldId || currentInstance != instanceId) {
		conquerorAndProtectorSystem::ConquerorAndProtectorService::getInstance().onLeaveMap(*player);
		instance::InstanceService::onLeaveInstance(*player);
	}
	world::World::getInstance().setPosition(runtime::Ptr<model::gameobjects::VisibleObject>(*player), worldId, instanceId, x, y, z, h);
	world::World::getInstance().setPosition(runtime::Ptr<model::gameobjects::VisibleObject>(player->getPet()), worldId, instanceId, x, y, z, h);

	player->setPortAnimation(model::animations::getDefaultArrivalAnimation(animation));
	if (currentWorldId == worldId && currentInstance == instanceId) {
		// instant teleport when map is the same
		spawnOnSameMap(*player);
	} else {
		// teleport with full map reloading, player will spawn via CM_LEVEL_READY
		utils::PacketSendUtility::sendPacket(*player, network::aion::serverpackets::SM_CHANNEL_INFO(player->getPosition()));
		utils::PacketSendUtility::sendPacket(*player, network::aion::serverpackets::SM_PLAYER_SPAWN(*player));
		// Java: `... .isInstance() && !WorldMapType.getWorld(worldId).isPersonal()`, the && short-circuit kept as a nested if because the
		// second operand throws NullPointerException for a map id without a WorldMapType constant (InstanceService.cpp:121-125)
		if (dataholders::DataManager::WORLD_MAPS_DATA->getTemplate(worldId)->isInstance()) {
			std::optional<world::WorldMapType> worldMapType = world::getWorldMapType(worldId);
			if (!worldMapType)
				throw runtime::NullPointerException("WorldMapType.getWorld(" + std::to_string(worldId) + ")");
			if (!world::isPersonal(*worldMapType))
				utils::PacketSendUtility::sendPacket(*player,
					network::aion::serverpackets::SM_SYSTEM_MESSAGE::STR_MSG_INSTANCE_DUNGEON_OPENED_FOR_SELF(worldId));
		}
	}
	if (player->isLegionMember() && player->getLegionMember()->getWorldId() != worldId)
		services::LegionService::getInstance().updateMemberInfo(*player);
}

void TeleportService::teleportToFirstTeleportLocation(model::gameobjects::player::Player& player, model::gameobjects::Npc& teleporter,
	model::animations::TeleportAnimation animation) {
	AION_UNPORTED();
}

void TeleportService::teleport(model::gameobjects::player::Player& player, const model::templates::teleport::TeleportLocation* location,
	model::animations::TeleportAnimation animation) {
	AION_UNPORTED();
}

const model::templates::teleport::TeleporterTemplate* TeleportService::validateTeleporterAndGetTemplate(model::gameobjects::player::Player& player,
	model::gameobjects::Npc& teleporter) {
	AION_UNPORTED();
}

bool TeleportService::checkKinahForTransportation(const model::templates::teleport::TeleportLocation* location,
	model::gameobjects::player::Player& player) {
	AION_UNPORTED();
}

// Java TeleportService.java:179-193
void TeleportService::sendLoc(model::gameobjects::player::Player& player, int32_t worldId, int32_t instanceId, float x, float y, float z, int8_t h,
	model::animations::TeleportAnimation animation) {
	abortPlayerActions(player);
	// despawn from world and send animation to others (also ends flying)
	world::World::getInstance().despawn(player, model::animations::getDefaultObjectDeleteAnimation(animation));

	runtime::Ref<SpawnTask> spawnTask = SpawnTask::create(player, worldId, instanceId, x, y, z, h, animation);
	if (animation == model::animations::TeleportAnimation::NONE) { // instant teleport (don't wait for player fade-out)
		spawnTask->run();
	} else {
		// send teleport animation to player and trigger CM_TELEPORT_ANIMATION_DONE when the animation ended
		utils::PacketSendUtility::sendPacket(player, network::aion::serverpackets::SM_TELEPORT_LOC(worldId, instanceId, x, y, z, h, animation));
		// task will be triggered from CM_TELEPORT_ANIMATION_DONE. Java: new FutureTask<Void>(spawnTask, null), a task bound to no executor;
		// Future::deferred is its C++ form (Future.h:118-127) and the Pin keeps the SpawnTask - and with it the player - alive until it runs
		// or the controller cancels it.
		player.getController().addTask(model::TaskId::TELEPORT,
			runtime::Future::deferred(runtime::Pin(spawnTask), [task = spawnTask.get()] { task->run(); }));
	}
}

// Java TeleportService.java:195-206
void TeleportService::abortPlayerActions(model::gameobjects::player::Player& player) {
	if (player.hasStore())
		PrivateStoreService::closePrivateStore(player);
	RecallService::getInstance().cancel(player, RecallService::CancelReason::CANCELLED);
	player.getController().cancelCurrentSkill(nullptr);
	player.setTarget(nullptr);
	player.unsetPlayerMode(model::actions::PlayerMode::RIDE);
	if (player.isUsingFlightTransporterOrWindstream()) {
		player.setFlightPath(nullptr);
		player.getFlyController().endFly(false);
	}
}

// Java TeleportService.java:208-219
void TeleportService::spawnOnSameMap(model::gameobjects::player::Player& player) {
	utils::PacketSendUtility::sendPacket(player, network::aion::serverpackets::SM_CHANNEL_INFO(player.getPosition()));
	utils::PacketSendUtility::sendPacket(player, network::aion::serverpackets::SM_PLAYER_INFO(player));
	utils::PacketSendUtility::sendPacket(player, network::aion::serverpackets::SM_STATS_INFO(player));
	// Java `new SM_MOTION(player.getObjectId(), player.getMotions().getActiveMotions())`: the packet's C++ constructor takes the map by value
	// type, so the RcLinkedHashMap is copied into it exactly as PlayerController::sendPlayerInfoPackets does (PlayerController.cpp:209-214).
	std::unordered_map<int32_t, runtime::Ptr<model::gameobjects::player::motion::Motion>> activeMotions;
	if (runtime::Ptr<runtime::RcLinkedHashMap<int32_t, runtime::Ref<model::gameobjects::player::motion::Motion>>> motions =
			player.getMotions().getActiveMotions()) {
		for (const auto& entry : motions->entrySet())
			activeMotions.emplace(entry.getKey(), entry.getValue());
	}
	utils::PacketSendUtility::sendPacket(player, network::aion::serverpackets::SM_MOTION(player.getObjectId(), activeMotions));
	world::World::getInstance().spawn(runtime::Ptr<model::gameobjects::VisibleObject>(player));
	world::World::getInstance().spawn(runtime::Ptr<model::gameobjects::VisibleObject>(player.getPet()));
	player.getController().startProtectionActiveTask();
	player.getEffectController()->updatePlayerEffectIcons(nullptr);
	player.getController().updateZone();
	player.setPortAnimation(model::animations::ArrivalAnimation::NONE);
}

void TeleportService::teleportTo(model::gameobjects::player::Player& player, world::WorldPosition& pos) {
	AION_UNPORTED();
}

void TeleportService::teleportDeadTo(model::gameobjects::player::Player& player, int32_t worldId, int32_t instanceId, float x, float y, float z,
	int8_t heading) {
	AION_UNPORTED();
}

// Java TeleportService.java:249-251
void TeleportService::teleportTo(model::gameobjects::player::Player& player, int32_t worldId, float x, float y, float z) {
	teleportTo(player, worldId, x, y, z, player.getHeading(), model::animations::TeleportAnimation::NONE);
}

// Java TeleportService.java:253-255
void TeleportService::teleportTo(model::gameobjects::player::Player& player, int32_t worldId, float x, float y, float z, int8_t h) {
	teleportTo(player, worldId, x, y, z, h, model::animations::TeleportAnimation::NONE);
}

// Java TeleportService.java:257-259
void TeleportService::teleportTo(model::gameobjects::player::Player& player, int32_t worldId, float x, float y, float z, int8_t h,
	model::animations::TeleportAnimation animation) {
	teleportTo(player, worldId, player.getWorldId() != worldId ? 1 : player.getInstanceId(), x, y, z, h, animation);
}

void TeleportService::teleportTo(model::gameobjects::player::Player& player, int32_t worldId, int32_t instanceId, float x, float y, float z) {
	AION_UNPORTED();
}

void TeleportService::teleportTo(model::gameobjects::player::Player& player, int32_t worldId, int32_t instanceId, float x, float y, float z,
	int8_t h) {
	AION_UNPORTED();
}

void TeleportService::teleportTo(model::gameobjects::player::Player& player, world::WorldMapInstance& instance, float x, float y, float z) {
	AION_UNPORTED();
}

void TeleportService::teleportTo(model::gameobjects::player::Player& player, world::WorldMapInstance& instance, float x, float y, float z, int8_t h) {
	AION_UNPORTED();
}

void TeleportService::teleportTo(model::gameobjects::player::Player& player, world::WorldMapInstance& instance, float x, float y, float z, int8_t h,
	model::animations::TeleportAnimation animation) {
	AION_UNPORTED();
}

// Java TeleportService.java:281-288
void TeleportService::teleportTo(model::gameobjects::player::Player& player, int32_t worldId, int32_t instanceId, float x, float y, float z,
	int8_t heading, model::animations::TeleportAnimation animation) {
	if (player.isDead()) {
		player::PlayerReviveService::revive(player, 20, 20, true, 0);
	} else if (DuelService::getInstance().isDueling(player)) {
		DuelService::getInstance().loseDuel(player);
	}
	sendLoc(player, worldId, instanceId, x, y, z, heading, animation);
}

void TeleportService::showMap(model::gameobjects::player::Player& player, model::gameobjects::Npc& npc) {
	AION_UNPORTED();
}

void TeleportService::teleportToPrison(model::gameobjects::player::Player& player) {
	AION_UNPORTED();
}

void TeleportService::teleportToNpc(model::gameobjects::player::Player& player, int32_t npcId) {
	AION_UNPORTED();
}

void TeleportService::sendObeliskBindPoint(model::gameobjects::player::Player& player) {
	int32_t worldId;
	float x, y, z;
	if (runtime::Ptr<model::gameobjects::player::BindPointPosition> bplist = player.getBindPoint()) {
		worldId = bplist->getMapId();
		x = bplist->getX();
		y = bplist->getY();
		z = bplist->getZ();
	} else {
		const dataholders::PlayerInitialData::LocationData& locationData = dataholders::DataManager::PLAYER_INITIAL_DATA->getSpawnLocation(player.getRace());
		worldId = locationData.getMapId();
		x = locationData.getX();
		y = locationData.getY();
		z = locationData.getZ();
	}
	utils::PacketSendUtility::sendPacket(player, network::aion::serverpackets::SM_BIND_POINT_INFO(worldId, x, y, z));
}

void TeleportService::sendKiskBindPoint(model::gameobjects::player::Player& player) {
	if (player.getKisk())
		utils::PacketSendUtility::sendPacket(player, network::aion::serverpackets::SM_BIND_POINT_INFO(player.getKisk()));
}

// Java TeleportService.java:362-383
void TeleportService::moveToBindLocation(model::gameobjects::player::Player& player) {
	float x, y, z;
	int32_t worldId;
	int8_t h;

	if (runtime::Ptr<model::gameobjects::player::BindPointPosition> bplist = player.getBindPoint()) {
		worldId = bplist->getMapId();
		x = bplist->getX();
		y = bplist->getY();
		z = bplist->getZ();
		h = bplist->getHeading();
	} else {
		const dataholders::PlayerInitialData::LocationData& locationData =
			dataholders::DataManager::PLAYER_INITIAL_DATA->getSpawnLocation(player.getRace());
		worldId = locationData.getMapId();
		x = locationData.getX();
		y = locationData.getY();
		z = locationData.getZ();
		h = locationData.getHeading();
	}
	teleportTo(player, worldId, x, y, z, h);
}

void TeleportService::moveToTargetWithDistance(model::gameobjects::VisibleObject& object, model::gameobjects::player::Player& player,
	int32_t direction, int32_t distance) {
	AION_UNPORTED();
}

void TeleportService::moveToInstanceExit(model::gameobjects::player::Player& player, int32_t worldId, model::Race race) {
	AION_UNPORTED();
}

void TeleportService::useTeleportScroll(model::gameobjects::player::Player& player, std::string_view portalName, int32_t worldId) {
	AION_UNPORTED();
}

void TeleportService::changeChannel(model::gameobjects::player::Player& player, int32_t channel) {
	AION_UNPORTED();
}

void TeleportService::setEventPos(world::WorldPosition& pos, model::Race race) {
	AION_UNPORTED();
}

void TeleportService::teleportToEvent(model::gameobjects::player::Player& player) {
	AION_UNPORTED();
}

bool TeleportService::sendTeleportRequest(model::gameobjects::player::Player& player, int32_t npcId) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::services::teleport
