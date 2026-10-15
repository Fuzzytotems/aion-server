#include "aion/gameserver/services/worldraid/WorldRaid.h"

#include "aion/commons/utils/Rnd.h"
#include "aion/gameserver/configs/main/EventsConfig.h"
#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/controllers/ObserveController.h"
#include "aion/gameserver/controllers/observer/DeathObserver.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/templates/spawns/SpawnTemplate.h"
#include "aion/gameserver/model/templates/worldraid/MarkerSpot.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/services/WorldRaidService.h"
#include "aion/gameserver/spawnengine/SpawnEngine.h"
#include "aion/gameserver/utils/PacketSendUtility.h"
#include "aion/gameserver/utils/ThreadPoolManager.h"
#include "aion/gameserver/world/World.h"
#include "aion/gameserver/world/WorldMap.h"
#include "aion/gameserver/world/WorldMapInstance.h"
#include "aion/commons/logging/LoggerFactory.h"
#include "aion/gameserver/model/gameobjects/Npc.h"
#include "aion/gameserver/model/templates/worldraid/WorldRaidLocation.h"
#include "aion/gameserver/model/templates/worldraid/WorldRaidNpc.h"

namespace aion::gameserver::services::worldraid {

using network::aion::serverpackets::SM_SYSTEM_MESSAGE;

static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.services.worldraid.WorldRaid");

WorldRaid::WorldRaid(const model::templates::worldraid::WorldRaidLocation* value, bool useSpecialSpawnMsgValue, bool sendMessagesValue)
	: raidLocation(value), useSpecialSpawnMsg(useSpecialSpawnMsgValue), sendMessages(sendMessagesValue) {
}

runtime::Ref<WorldRaid> WorldRaid::create(const model::templates::worldraid::WorldRaidLocation* value, bool useSpecialSpawnMsgValue,
	bool sendMessagesValue) {
	return runtime::makeRef<WorldRaid>(value, useSpecialSpawnMsgValue, sendMessagesValue);
}

// Java WorldRaid.java:50-53
void WorldRaid::startWorldRaid() {
	if (isStarted.compareAndSet(false, true))
		onWorldRaidStart();
}

// Java WorldRaid.java:55-58
void WorldRaid::stopWorldRaid() {
	if (isFinished_.compareAndSet(false, true))
		onWorldRaidFinish();
}

/** Java: the anonymous Runnable of onWorldRaidStart (WorldRaid.java:63-100) - its `progress` counts the minutes; one fixed-rate task runs it */
struct WorldRaid_Preparation final : runtime::RefCounted {
	AION_MAKE_REF_FRIEND
	runtime::Field<int32_t> progress{0}; // Java: private int progress = 0

	static runtime::Ref<WorldRaid_Preparation> create() { return runtime::makeRef<WorldRaid_Preparation>(); }

protected:
	WorldRaid_Preparation() = default;
	~WorldRaid_Preparation() override = default;
};

// Java WorldRaid.java:60-102
void WorldRaid::onWorldRaidStart() {
	if (runtime::Ptr<runtime::Future> task = preparationTask.get())
		task->cancel(false);
	runtime::Ref<WorldRaid_Preparation> preparation = WorldRaid_Preparation::create();
	WorldRaid_Preparation& state = *preparation;
	preparationTask.set(utils::ThreadPoolManager::getInstance().scheduleAtFixedRate({this, &state}, [this, &state] {
		const int32_t progress = state.progress.get();
		state.progress.set(progress + 1); // Java: switch (progress++)
		switch (progress) {
			case 0: {
				spawnAndInitMapFlag();
				SM_SYSTEM_MESSAGE msg = SM_SYSTEM_MESSAGE::STR_MSG_WORLDRAID_MESSAGE_01();
				broadcastMessage(msg);
				break;
			}
			case 10: { // 10 minutes
				spawnAndInitVortex();
				SM_SYSTEM_MESSAGE msg = SM_SYSTEM_MESSAGE::STR_MSG_WORLDRAID_MESSAGE_02();
				broadcastMessage(msg);
				break;
			}
			case 25: { // 25 minutes
				spawnAndInitMarkerSpots();
				SM_SYSTEM_MESSAGE msg = SM_SYSTEM_MESSAGE::STR_MSG_WORLDRAID_MESSAGE_03();
				broadcastMessage(msg);
				break;
			}
			case 29: { // 29 minutes
				if (!configs::main::EventsConfig::WORLDRAID_ENABLE_SPAWNMSG.load())
					break;
				SM_SYSTEM_MESSAGE msg = useSpecialSpawnMsg ? SM_SYSTEM_MESSAGE::STR_MSG_WORLDRAID_INVADE_VRITRA_SPECIAL()
					: SM_SYSTEM_MESSAGE::STR_MSG_WORLDRAID_INVADE_VRITRA();
				broadcastMessage(msg);
				break;
			}
			case 30: { // 30 minutes
				preparationTask.get()->cancel(false);
				preparationTask.set(nullptr);
				despawnNpcs({vortex.get()});
				spawnAndInitRandomBoss();
				SM_SYSTEM_MESSAGE msg = SM_SYSTEM_MESSAGE::STR_MSG_WORLDRAID_MESSAGE_04();
				broadcastMessage(msg);
				scheduleBossDespawn();
				break;
			}
		}
	}, 0, 60000));
}

// Java WorldRaid.java:104-107
void WorldRaid::onWorldRaidFinish() {
	despawnNpcs({flag.get(), vortex.get(), boss.get()});
	despawnNpcs(locationMarkers.snapshot());
}

// Java WorldRaid.java:109-111
void WorldRaid::scheduleBossDespawn() {
	const int32_t locationId = getLocationId();
	stopRaidTask.set(utils::ThreadPoolManager::getInstance().schedule({&WorldRaidService::getInstance()},
		[locationId] { WorldRaidService::getInstance().stopRaid(locationId); }, 1, runtime::TimeUnit::HOURS));
}

// Java WorldRaid.java:113-118
void WorldRaid::cancelStopRaidTask() {
	runtime::Ptr<runtime::Future> task = stopRaidTask.get();
	if (task && !task->isCancelled()) {
		task->cancel(false);
		stopRaidTask.set(nullptr);
	}
}

// Java WorldRaid.java:120-123 (a null npc is Java's NullPointerException)
void WorldRaid::despawnNpcs(std::initializer_list<runtime::Ptr<model::gameobjects::Npc>> npcs) {
	for (const runtime::Ptr<model::gameobjects::Npc>& npc : npcs)
		npc->getController().deleteIfAliveOrCancelRespawn();
}

// Java WorldRaid.java:125-128
void WorldRaid::despawnNpcs(const std::vector<runtime::Ptr<model::gameobjects::Npc>>& npcs) {
	for (const runtime::Ptr<model::gameobjects::Npc>& npc : npcs)
		npc->getController().deleteIfAliveOrCancelRespawn();
}

// Java WorldRaid.java:130-141
void WorldRaid::spawnAndInitRandomBoss() {
	if (!raidLocation->getNpcPool())
		throw runtime::NullPointerException("world raid location without npc pool"); // Java: Rnd.get(null)
	const model::templates::worldraid::WorldRaidNpc* bossNpcTemplate = commons::utils::Rnd::get(*raidLocation->getNpcPool());
	randomBossTemplate.set(bossNpcTemplate);
	runtime::Ref<model::templates::spawns::SpawnTemplate> bossTemplate = spawnengine::SpawnEngine::newSingleTimeSpawn(raidLocation->getMapId(),
		bossNpcTemplate->getNpcId(), raidLocation->getX(), raidLocation->getY(), raidLocation->getZ(), raidLocation->getH(), nullptr,
		"world_raid_aggressive");
	runtime::Ptr<model::gameobjects::Npc> bossNpc = runtime::cast<model::gameobjects::Npc>(spawnengine::SpawnEngine::spawnObject(*bossTemplate, 1));
	if (!bossNpc) {
		log.warn("Cannot initialize world raid boss with ID " + std::to_string(bossNpcTemplate->getNpcId()) + ". No boss was spawned.");
		return;
	}
	boss.set(bossNpc);
	registerDeathObserver(*bossNpc);
}

// Java WorldRaid.java:143-152. The DeathObserver's lambda pins this raid.
void WorldRaid::registerDeathObserver(model::gameobjects::Npc& npc) {
	npc.getObserveController()->attach(*controllers::observer::DeathObserver::create(
		runtime::PinnedCallback<void(model::gameobjects::Creature&)>({this}, [this](model::gameobjects::Creature&) {
			if (isFinished())
				return;
			// Java: getDeathMsgId() != null - an Integer field initialized to 0, never null
			SM_SYSTEM_MESSAGE msg(randomBossTemplate.get()->getDeathMsgId(), std::vector<std::string>{}); // STR_MSG_WORLDRAID_MESSAGE_DIE_01-06
			broadcastMessage(msg, true);
			cancelStopRaidTask();
			WorldRaidService::getInstance().stopRaid(getLocationId());
		})));
}

// Java WorldRaid.java:154-158
void WorldRaid::spawnAndInitMapFlag() {
	runtime::Ref<model::templates::spawns::SpawnTemplate> flagTemplate = spawnengine::SpawnEngine::newSingleTimeSpawn(raidLocation->getMapId(), 832819,
		raidLocation->getX(), raidLocation->getY(), raidLocation->getZ(), int8_t{0});
	flag.set(runtime::cast<model::gameobjects::Npc>(spawnengine::SpawnEngine::spawnObject(*flagTemplate, 1)));
}

// Java WorldRaid.java:160-164
void WorldRaid::spawnAndInitVortex() {
	runtime::Ref<model::templates::spawns::SpawnTemplate> vortexTemplate = spawnengine::SpawnEngine::newSingleTimeSpawn(raidLocation->getMapId(), 702550,
		raidLocation->getX(), raidLocation->getY(), raidLocation->getZ() + 40.0f, int8_t{0});
	vortex.set(runtime::cast<model::gameobjects::Npc>(spawnengine::SpawnEngine::spawnObject(*vortexTemplate, 1)));
}

// Java WorldRaid.java:166-172
void WorldRaid::spawnAndInitMarkerSpots() {
	if (!raidLocation->getLocationMarkers())
		throw runtime::NullPointerException("world raid location without location markers"); // Java: for (... : null)
	for (const model::templates::worldraid::MarkerSpot& locationMarker : *raidLocation->getLocationMarkers()) {
		runtime::Ref<model::templates::spawns::SpawnTemplate> markerTemplate = spawnengine::SpawnEngine::newSingleTimeSpawn(raidLocation->getMapId(),
			702548, locationMarker.getX(), locationMarker.getY(), locationMarker.getZ(), locationMarker.getH());
		locationMarkers.add(runtime::Ref<model::gameobjects::Npc>(runtime::cast<model::gameobjects::Npc>(spawnengine::SpawnEngine::spawnObject(*markerTemplate, 1))));
	}
}

// Java WorldRaid.java:174-176
void WorldRaid::broadcastMessage(SM_SYSTEM_MESSAGE& msg) {
	broadcastMessage(msg, false);
}

// Java WorldRaid.java:178-181
void WorldRaid::broadcastMessage(SM_SYSTEM_MESSAGE& msg, bool forceMsg) {
	if (sendMessages || forceMsg)
		world::World::getInstance().getWorldMap(raidLocation->getMapId())->getMainWorldMapInstance()->forEachPlayer(
			[&msg](model::gameobjects::player::Player& p) { utils::PacketSendUtility::sendPacket(p, msg); });
}

// Java WorldRaid.java:183-185
int32_t WorldRaid::getLocationId() {
	return raidLocation->getLocationId();
}

// Java WorldRaid.java:187-189
bool WorldRaid::isFinished() {
	return isFinished_.get();
}

WorldRaid::~WorldRaid() = default;

} // namespace aion::gameserver::services::worldraid
