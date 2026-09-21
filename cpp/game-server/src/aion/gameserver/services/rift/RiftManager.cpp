#include "aion/gameserver/services/rift/RiftManager.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/commons/logging/LoggerFactory.h"
#include "aion/gameserver/model/gameobjects/Npc.h"
#include "aion/gameserver/model/templates/spawns/SpawnGroup.h"
#include "aion/gameserver/model/templates/spawns/SpawnTemplate.h"

namespace aion::gameserver::services::rift {

runtime::ConcurrentHashMap<int32_t, runtime::Ref<runtime::RcArrayList<runtime::Ref<model::gameobjects::Npc>>>> RiftManager::riftsPerWorld{
	AION_LOCK_CLASS(RiftManager::riftsPerWorld#stripe)};
runtime::ConcurrentHashMap<std::string, runtime::Ref<model::templates::spawns::SpawnTemplate>> RiftManager::riftGroups{
	AION_LOCK_CLASS(RiftManager::riftGroups#stripe)};

static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.services.rift.RiftManager");

void RiftManager::addRiftSpawnTemplate(model::templates::spawns::SpawnGroup& spawn) {
	if (spawn.hasPool()) {
		runtime::Ptr<model::templates::spawns::SpawnTemplate> spawnTemplate = spawn.getSpawnTemplates().get(0);
		riftGroups.put(spawnTemplate->getAnchor(), runtime::Ref<model::templates::spawns::SpawnTemplate>(spawnTemplate));
	} else {
		for (const runtime::Ptr<model::templates::spawns::SpawnTemplate>& spawnTemplate : spawn.getSpawnTemplates().snapshot()) {
			riftGroups.put(spawnTemplate->getAnchor(), runtime::Ref<model::templates::spawns::SpawnTemplate>(spawnTemplate));
		}
	}
}

void RiftManager::spawnRift(model::rift::RiftLocation& loc, bool isWithGuards) {
	AION_UNPORTED();
}

void RiftManager::spawnVortex(model::vortex::VortexLocation& loc) {
	AION_UNPORTED();
}

void RiftManager::spawnRift(RiftEnum rift, runtime::Ptr<model::vortex::VortexLocation> vl, runtime::Ptr<model::rift::RiftLocation> rl,
	bool isWithGuards) {
	AION_UNPORTED();
}

runtime::Ptr<model::gameobjects::Npc> RiftManager::spawnInstance(int32_t instance, model::templates::spawns::SpawnTemplate& template_,
	controllers::RVController& controller) {
	AION_UNPORTED();
}

void RiftManager::addSpawnedRift(model::gameobjects::Npc& rift) {
	runtime::Ptr<runtime::RcArrayList<runtime::Ref<model::gameobjects::Npc>>> rifts = riftsPerWorld.computeIfAbsent(rift.getWorldId(),
		[] { return runtime::RcArrayList<runtime::Ref<model::gameobjects::Npc>>::create(AION_LOCK_CLASS(RiftManager::riftsPerWorld#value)); });
	rifts->add(runtime::Ref<model::gameobjects::Npc>(rift));
}

std::vector<runtime::Ptr<model::gameobjects::Npc>> RiftManager::getSpawnedRifts(int32_t worldId) {
	// Java returns the live CopyOnWriteArrayList (or Collections.emptyList()); its iterators are snapshots, so a snapshot is equivalent
	std::vector<runtime::Ptr<model::gameobjects::Npc>> spawnedRifts;
	if (runtime::Ptr<runtime::RcArrayList<runtime::Ref<model::gameobjects::Npc>>> rifts = riftsPerWorld.get(worldId)) {
		for (const auto& rift : rifts->snapshot())
			spawnedRifts.emplace_back(rift);
	}
	return spawnedRifts;
}

bool RiftManager::removeSpawnedRift(model::gameobjects::Npc& rift) {
	runtime::Ptr<runtime::RcArrayList<runtime::Ref<model::gameobjects::Npc>>> rifts = riftsPerWorld.get(rift.getWorldId());
	return rifts && rifts->remove(runtime::Ptr<model::gameobjects::Npc>(rift));
}

RiftManager& RiftManager::getInstance() {
	static RiftManager instance; // Java SingletonHolder
	return instance;
}

} // namespace aion::gameserver::services::rift
