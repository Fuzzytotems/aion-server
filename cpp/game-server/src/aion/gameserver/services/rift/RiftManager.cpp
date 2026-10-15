#include "aion/gameserver/services/rift/RiftManager.h"

#include "aion/gameserver/controllers/RVController.h"
#include "aion/gameserver/controllers/effect/EffectController.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/NpcData.h"
#include "aion/gameserver/dataholders/loadingutils/EnumTraits.h"
#include "aion/gameserver/model/rift/RiftLocation.h"
#include "aion/gameserver/model/vortex/VortexLocation.h"
#include "aion/gameserver/services/rift/RiftEnumInfo.h"
#include "aion/gameserver/world/World.h"
#include "aion/gameserver/world/WorldMap.h"
#include "aion/gameserver/world/knownlist/NpcKnownList.h"
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

// Java RiftManager.java:44-47
void RiftManager::spawnRift(model::rift::RiftLocation& loc, bool isWithGuards) {
	RiftEnum rift = getRift(loc.getId());
	spawnRift(rift, nullptr, runtime::Ptr<model::rift::RiftLocation>(loc), isWithGuards);
}

// Java RiftManager.java:49-52
void RiftManager::spawnVortex(model::vortex::VortexLocation& loc) {
	RiftEnum rift = getVortex(loc.getDefendersRace());
	spawnRift(rift, runtime::Ptr<model::vortex::VortexLocation>(loc), nullptr, false);
}

// Java RiftManager.java:54-85
void RiftManager::spawnRift(RiftEnum rift, runtime::Ptr<model::vortex::VortexLocation> vl, runtime::Ptr<model::rift::RiftLocation> rl,
	bool isWithGuards) {
	runtime::Ptr<model::templates::spawns::SpawnTemplate> masterTemplate = riftGroups.get(std::string(getMaster(rift)));
	runtime::Ptr<model::templates::spawns::SpawnTemplate> slaveTemplate = riftGroups.get(std::string(getSlave(rift)));

	if (!masterTemplate || !slaveTemplate) {
		return;
	}

	int32_t spawned = 0;
	const int32_t instanceCount = world::World::getInstance().getWorldMap(masterTemplate->getWorldId())->getInstanceCount();
	for (int32_t i = 1; i <= instanceCount; i++) {
		if (slaveTemplate->hasPool()) {
			slaveTemplate = slaveTemplate->changeTemplate(i);
		}
		runtime::Ptr<model::gameobjects::Npc> slave = spawnInstance(i, *slaveTemplate, std::make_unique<controllers::RVController>(nullptr, rift));
		runtime::Ptr<model::gameobjects::Npc> master =
			spawnInstance(i, *masterTemplate, std::make_unique<controllers::RVController>(slave, rift, isWithGuards));
		if (isVortex(rift)) {
			vl->setVortexController(runtime::cast<controllers::RVController>(master->getController()));
			spawned = vl->getSpawned().size();
			vl->getSpawned().add(runtime::Ref<model::gameobjects::VisibleObject>(master));
			vl->getSpawned().add(runtime::Ref<model::gameobjects::VisibleObject>(slave));
		} else {
			spawned = rl->getSpawned().size();
			rl->addSpawned(*master);
			rl->addSpawned(*slave);
		}
	}
	log.info("Rift opened: " + std::string(xml::enumName(rift)) + ", spawned " + std::to_string(spawned) + " Npcs (guards=" +
		(isWithGuards ? "true" : "false") + ").");
}

// Java RiftManager.java:87-101
runtime::Ptr<model::gameobjects::Npc> RiftManager::spawnInstance(int32_t instance, model::templates::spawns::SpawnTemplate& template_,
	std::unique_ptr<controllers::RVController> controller) {
	const model::templates::npc::NpcTemplate* masterObjectTemplate = dataholders::DataManager::NPC_DATA->getNpcTemplate(template_.getNpcId());
	runtime::Ref<model::gameobjects::Npc> npc =
		model::gameobjects::VisibleObject::create<model::gameobjects::Npc>(std::move(controller), template_, masterObjectTemplate);
	npc->setKnownlist(std::make_unique<world::knownlist::NpcKnownList>(*npc));
	npc->setEffectController(std::make_unique<controllers::effect::EffectController>(*npc));
	world::World& world = world::World::getInstance();
	world.storeObject(*npc);
	world.setPosition(runtime::Ptr<model::gameobjects::VisibleObject>(npc), template_.getWorldId(), instance, template_.getX(), template_.getY(),
		template_.getZ(), template_.getHeading());
	world.spawn(runtime::Ptr<model::gameobjects::VisibleObject>(npc));
	addSpawnedRift(*npc);
	return npc;
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
