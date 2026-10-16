#include "aion/gameserver/spawnengine/WalkerFormator.h"

#include <optional>
#include <string>

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/WalkerData.h"
#include "aion/gameserver/model/gameobjects/Npc.h"
#include "aion/gameserver/model/templates/spawns/SpawnTemplate.h"
#include "aion/gameserver/model/templates/walker/WalkerTemplate.h"
#include "aion/gameserver/spawnengine/ClusteredNpc.h"
#include "aion/gameserver/spawnengine/InstanceWalkerFormations.h"
#include "aion/gameserver/spawnengine/WalkerFormationsCache.h"
#include "aion/gameserver/spawnengine/WalkerGroup.h"

namespace aion::gameserver::spawnengine {

static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.spawnengine.WalkerFormator");

bool WalkerFormator::processClusteredNpc(model::gameobjects::Npc& npc, int32_t worldId, int32_t instanceId) {
	std::optional<std::string> walkerId = npc.getSpawn()->getWalkerId();
	if (walkerId) {
		const model::templates::walker::WalkerTemplate* template_ = dataholders::DataManager::WALKER_DATA->getWalkerTemplate(*walkerId);
		if (template_ == nullptr) {
			log.warn("Missing walker ID: " + *walkerId);
			return false;
		}
		if (template_->getPool() < 2)
			return false;

		runtime::Ptr<InstanceWalkerFormations> formations = WalkerFormationsCache::getInstanceFormations(worldId, instanceId);
		runtime::Ptr<WalkerGroup> wg = formations->getSpawnWalkerGroup(*walkerId);

		if (wg) {
			npc.setWalkerGroup(wg);
			wg->respawn(npc);
			return true;
		}

		return formations->cacheWalkerCandidate(*ClusteredNpc::create(npc, instanceId, template_));
	}
	return false;
}

void WalkerFormator::organizeAndSpawn(int32_t worldId, int32_t instanceId) {
	runtime::Ptr<InstanceWalkerFormations> formations = WalkerFormationsCache::getInstanceFormations(worldId, instanceId);
	formations->organizeAndSpawn();
}

void WalkerFormator::changeWalkerGroup(int32_t worldId, int32_t instanceId, WalkerGroup& walkerGroup) {
	runtime::Ptr<InstanceWalkerFormations> formations = WalkerFormationsCache::getInstanceFormations(worldId, instanceId);
	formations->changeCluster(walkerGroup);
}

void WalkerFormator::onInstanceDestroy(int32_t worldId, int32_t instanceId) {
	WalkerFormationsCache::onInstanceDestroy(worldId, instanceId);
}

} // namespace aion::gameserver::spawnengine
