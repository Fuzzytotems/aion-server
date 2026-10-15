#include "aion/gameserver/services/VortexService.h"

#include "aion/gameserver/configs/main/CustomConfig.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/VortexData.h"
#include "aion/gameserver/model/vortex/VortexLocation.h"
#include "aion/gameserver/model/vortex/VortexStateType.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include <string>
#include <vector>
#include "aion/gameserver/controllers/VisibleObjectController.h"
#include "aion/gameserver/dataholders/SpawnsData.h"
#include "aion/gameserver/model/gameobjects/VisibleObject.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/templates/spawns/SpawnGroup.h"
#include "aion/gameserver/model/templates/spawns/SpawnTemplate.h"
#include "aion/gameserver/model/templates/spawns/vortexspawns/VortexSpawnTemplate.h"
#include "aion/gameserver/runtime/sync/Monitor.h"
#include "aion/gameserver/services/rift/RiftInformer.h"
#include "aion/gameserver/services/rift/RiftManager.h"
#include "aion/gameserver/spawnengine/SpawnEngine.h"
#include "aion/gameserver/services/cron/CronExpression.h"
#include "aion/gameserver/services/cron/CronService.h"
#include "aion/gameserver/services/vortex/DimensionalVortex.h"
#include "aion/gameserver/services/vortex/Invasion.h"
#include "aion/gameserver/utils/ThreadPoolManager.h"
#include "aion/gameserver/world/WorldMapType.h"
#include "aion/gameserver/world/WorldMapTypeInfo.h"

namespace aion::gameserver::services {

// Anonymous classes and stored lambdas of the Java class (hub-headers.md §7.3): the bodies that create them define the structs that
// `python tools/gen/fieldmap.py --class <key>` prints.
//   com.aionemu.gameserver.services.VortexService@L38:39
//   com.aionemu.gameserver.services.VortexService@L39:39
//   com.aionemu.gameserver.services.VortexService@L57:44

void VortexService::initVortexLocations() {
	if (configs::main::CustomConfig::VORTEX_ENABLED.load()) {
		for (const auto& [id, loc] : dataholders::DataManager::VORTEX_DATA->getVortexLocations())
			spawn(*loc, model::vortex::VortexStateType::PEACE);

		const auto require = [](const cron::CronExpression* expression) -> const cron::CronExpression& {
			if (expression == nullptr)
				throw runtime::NullPointerException("cronExpression"); // Java: CronService.schedule with a null expression
			return *expression;
		};
		cron::CronService::getInstance().schedule([] { getInstance().startInvasion(0); },
			require(configs::main::CustomConfig::VORTEX_THEOBOMOS_SCHEDULE.load()));
		cron::CronService::getInstance().schedule([] { getInstance().startInvasion(1); },
			require(configs::main::CustomConfig::VORTEX_BRUSTHONIN_SCHEDULE.load()));
	}
}

// Java VortexService.java:43-58; lambda at VortexService.java:57 (fieldmap key VortexService@L57:44): the scheduled end
void VortexService::startInvasion(int32_t id) {
	runtime::Ref<vortex::DimensionalVortex> invasion;

	SYNCHRONIZED(*this) {
		if (activeInvasions.containsKey(id)) {
			return;
		}
		const runtime::Ref<model::vortex::VortexLocation>* loc = dataholders::DataManager::VORTEX_DATA->getVortexLocations().get(id);
		if (loc == nullptr) // Java: new Invasion(null) - its super constructor keeps null, and start() dereferences it
			throw runtime::NullPointerException("VortexLocation " + std::to_string(id));
		invasion = vortex::Invasion::create(**loc);
		activeInvasions.put(id, invasion);
	}

	invasion->start();

	// schedule invasion end
	utils::ThreadPoolManager::getInstance().schedule(this, [this, id] { stopInvasion(id); }, static_cast<int64_t>(getDuration()) * 3600 * 1000);
}

// Java VortexService.java:60-75
void VortexService::stopInvasion(int32_t id) {
	if (!isInvasionInProgress(id)) {
		return;
	}

	runtime::Ptr<vortex::DimensionalVortex> invasion;
	SYNCHRONIZED(*this) {
		invasion = activeInvasions.remove(id);
	}

	if (!invasion || invasion->isFinished()) {
		return;
	}

	invasion->stop();
}

// Java VortexService.java:77-94
void VortexService::spawn(model::vortex::VortexLocation& loc, model::vortex::VortexStateType state) {
	// Spawn Dimensional Vortex
	if (state == model::vortex::VortexStateType::INVASION) {
		rift::RiftManager::getInstance().spawnVortex(loc);
		rift::RiftInformer::sendRiftsInfo(loc.getHomeWorldId());
	}

	// Spawn NPC
	const std::vector<runtime::Ref<model::templates::spawns::SpawnGroup>>* locSpawns = dataholders::DataManager::SPAWNS_DATA->getVortexSpawnsByLocId(loc.getId());
	if (locSpawns == nullptr) // Java: the for-each over null
		throw runtime::NullPointerException("vortex spawns of location " + std::to_string(loc.getId()));
	for (const runtime::Ref<model::templates::spawns::SpawnGroup>& group : *locSpawns) {
		for (const runtime::Ptr<model::templates::spawns::SpawnTemplate>& st : group->getSpawnTemplates().snapshot()) {
			runtime::Ptr<model::templates::spawns::vortexspawns::VortexSpawnTemplate> vortextemplate =
				runtime::cast<model::templates::spawns::vortexspawns::VortexSpawnTemplate>(st);
			if (vortextemplate->getStateType() == state) {
				loc.getSpawned().add(runtime::Ref<model::gameobjects::VisibleObject>(spawnengine::SpawnEngine::spawnObject(*vortextemplate, 1)));
			}
		}
	}
}

// Java VortexService.java:96-106
void VortexService::despawn(model::vortex::VortexLocation& loc) {
	// Unset Vortex controller
	loc.setVortexController(nullptr);

	// Despawn all NPC
	for (const runtime::Ptr<model::gameobjects::VisibleObject>& npc : loc.getSpawned().snapshot()) {
		npc->getController().deleteIfAliveOrCancelRespawn();
	}

	loc.getSpawned().clear();
}

// Java VortexService.java:108-110
bool VortexService::isInvasionInProgress(int32_t id) {
	return activeInvasions.containsKey(id);
}

// Java VortexService.java:116-118
int32_t VortexService::getDuration() {
	return configs::main::CustomConfig::VORTEX_DURATION.load();
}

// Java VortexService.java:120-127
void VortexService::removeDefenderPlayer(model::gameobjects::player::Player& player) {
	for (const runtime::Ptr<vortex::DimensionalVortex>& invasion : activeInvasions.values()) {
		if (invasion->getDefenders().contains(player.getObjectId())) {
			invasion->kickPlayer(player, false);
			return;
		}
	}
}

// Java VortexService.java:129-136
void VortexService::removeInvaderPlayer(model::gameobjects::player::Player& player) {
	for (const runtime::Ptr<vortex::DimensionalVortex>& invasion : activeInvasions.values()) {
		if (invasion->getInvaders().contains(player.getObjectId())) {
			invasion->kickPlayer(player, true);
			return;
		}
	}
}

// Java VortexService.java:138-146
bool VortexService::isInvaderPlayer(model::gameobjects::player::Player& player) {
	for (const runtime::Ptr<vortex::DimensionalVortex>& invasion : activeInvasions.values()) {
		if (invasion->getInvaders().contains(player.getObjectId())) {
			return true;
		}
	}

	return false;
}

// Java VortexService.java:148-151
bool VortexService::isInsideVortexZone(model::gameobjects::player::Player& player) {
	runtime::Ptr<model::vortex::VortexLocation> loc = getLocationByWorld(player.getWorldId());
	return loc && loc->getPlayers().containsKey(player.getObjectId());
}

runtime::Ptr<model::vortex::VortexLocation> VortexService::getLocationByRift(int32_t npcId) {
	return getLocationByWorld(npcId == 831141 ? world::getId(world::WorldMapType::BRUSTHONIN) : world::getId(world::WorldMapType::THEOBOMOS));
}

runtime::Ptr<model::vortex::VortexLocation> VortexService::getLocationByWorld(int32_t worldId) {
	if (worldId == world::getId(world::WorldMapType::THEOBOMOS)) {
		const runtime::Ref<model::vortex::VortexLocation>* loc = dataholders::DataManager::VORTEX_DATA->getVortexLocations().get(0);
		return loc != nullptr ? runtime::Ptr<model::vortex::VortexLocation>(*loc) : nullptr;
	} else if (worldId == world::getId(world::WorldMapType::BRUSTHONIN)) {
		const runtime::Ref<model::vortex::VortexLocation>* loc = dataholders::DataManager::VORTEX_DATA->getVortexLocations().get(1);
		return loc != nullptr ? runtime::Ptr<model::vortex::VortexLocation>(*loc) : nullptr;
	} else {
		return nullptr;
	}
}

VortexService& VortexService::getInstance() {
	static VortexService instance; // Java VortexServiceHolder
	return instance;
}

} // namespace aion::gameserver::services
