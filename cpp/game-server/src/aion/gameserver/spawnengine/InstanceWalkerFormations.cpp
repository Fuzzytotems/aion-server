#include "aion/gameserver/spawnengine/InstanceWalkerFormations.h"

#include <algorithm>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/commons/utils/Rnd.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/WalkerVersionsData.h"
#include "aion/gameserver/model/gameobjects/Npc.h"
#include "aion/gameserver/model/templates/spawns/SpawnTemplate.h"
#include "aion/gameserver/model/templates/walker/WalkerTemplate.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/runtime/sync/Monitor.h"
#include "aion/gameserver/spawnengine/ClusteredNpc.h"
#include "aion/gameserver/spawnengine/WalkerGroup.h"

namespace aion::gameserver::spawnengine {

static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.spawnengine.InstanceWalkerFormations");

namespace {

using ClusteredNpcList = runtime::RcArrayList<runtime::Ref<ClusteredNpc>>;
using WalkerGroupList = runtime::RcArrayList<runtime::Ref<WalkerGroup>>;

/** Java: Rnd.get(list) followed by a dereference (NullPointerException for an empty list) */
template <class T>
runtime::Ptr<T> randomElement(const std::vector<runtime::Ptr<T>>& elements) {
	const runtime::Ptr<T>* element = commons::utils::Rnd::get(elements);
	if (element == nullptr)
		throw runtime::NullPointerException("Rnd.get returned null for an empty list");
	return *element;
}

/** Java: routeId (candidates.get(0).getWalkTemplate().getRouteId()) */
const std::string& routeIdOf(const std::vector<runtime::Ptr<ClusteredNpc>>& candidates) {
	return candidates.at(0)->getWalkTemplate()->getRouteId();
}

} // namespace

InstanceWalkerFormations::InstanceWalkerFormations() = default;

InstanceWalkerFormations::~InstanceWalkerFormations() = default;

runtime::Ref<InstanceWalkerFormations> InstanceWalkerFormations::create() {
	return runtime::makeRef<InstanceWalkerFormations>();
}

runtime::Ptr<WalkerGroup> InstanceWalkerFormations::getSpawnWalkerGroup(std::string_view walkerId) {
	return walkFormations.get(std::string(walkerId));
}

bool InstanceWalkerFormations::cacheWalkerCandidate(ClusteredNpc& npcWalker) {
	SYNCHRONIZED(*this) {
		std::string walkerId = npcWalker.getWalkTemplate()->getRouteId();
		runtime::Ptr<ClusteredNpcList> candidateList = groupedSpawnObjects.get(walkerId);
		if (!candidateList) {
			runtime::Ref<ClusteredNpcList> created = ClusteredNpcList::create();
			groupedSpawnObjects.put(walkerId, created);
			candidateList = created;
		}
		candidateList->add(runtime::Ref<ClusteredNpc>(npcWalker));
		return true; // Java: List.add always returns true
	}
}

void InstanceWalkerFormations::organizeAndSpawn() {
	for (runtime::Ptr<ClusteredNpcList> candidateList : groupedSpawnObjects.values()) {
		std::vector<runtime::Ptr<ClusteredNpc>> candidates = candidateList->snapshot();
		// Java: candidates.stream().collect(Collectors.groupingBy(cNpc -> cNpc.getPositionHash())) - a HashMap (iteration order unspecified); the
		// groups are kept in the order of their first candidate
		std::vector<std::pair<int32_t, std::vector<runtime::Ptr<ClusteredNpc>>>> npcsByPosition;
		for (const runtime::Ptr<ClusteredNpc>& cNpc : candidates) {
			int32_t hash = cNpc->getPositionHash();
			auto group = std::find_if(npcsByPosition.begin(), npcsByPosition.end(), [hash](const auto& entry) { return entry.first == hash; });
			if (group == npcsByPosition.end())
				npcsByPosition.emplace_back(hash, std::vector<runtime::Ptr<ClusteredNpc>>{cNpc});
			else
				group->second.push_back(cNpc);
		}
		size_t maxSize = 0;
		const std::vector<runtime::Ptr<ClusteredNpc>>* npcs = nullptr;
		for (const auto& e : npcsByPosition) {
			if (e.second.size() > maxSize) {
				npcs = &e.second;
				maxSize = npcs->size();
			}
		}
		if (maxSize == 0 || npcs == nullptr) {
			log.warn("Walkers missing for route: " + routeIdOf(candidates));
			continue;
		}
		if (maxSize == 1) {
			if (candidates.size() != 1) {
				log.warn("Walkers not aligned for route: " + routeIdOf(candidates));
				for (const runtime::Ptr<ClusteredNpc>& snpc : candidates)
					snpc->spawn(snpc->getNpc()->getSpawn()->getZ());
			} else {
				runtime::Ptr<ClusteredNpc> singleNpc = candidates[0];
				std::optional<std::string> versionId = singleNpc->getWalkTemplate()->getVersionId();
				if (versionId) {
					runtime::Ptr<ClusteredNpcList> variants = walkerVariants.get(*versionId);
					if (!variants) {
						runtime::Ref<ClusteredNpcList> created = ClusteredNpcList::create();
						walkerVariants.put(*versionId, created);
						variants = created;
					}
					variants->add(runtime::Ref<ClusteredNpc>(singleNpc));
				} else
					singleNpc->spawn(singleNpc->getNpc()->getSpawn()->getZ());
			}
		} else {
			runtime::Ref<WalkerGroup> wg = WalkerGroup::create(*npcs);
			if (candidates[0]->getWalkTemplate()->getPool() != static_cast<int32_t>(candidates.size()))
				log.warn("Incorrect pool for route: " + routeIdOf(candidates));
			walkFormations.put(routeIdOf(candidates), wg);
			wg->form();
			if (wg->getVersionId().empty()) { // Java: wg.getVersionId() == null
				wg->spawn();
				// spawn the rest which didn't have the same coordinates
				for (const runtime::Ptr<ClusteredNpc>& snpc : candidates) {
					if (std::find(npcs->begin(), npcs->end(), snpc) != npcs->end())
						continue;
					snpc->spawn(snpc->getNpc()->getZ());
				}
			} else {
				runtime::Ptr<WalkerGroupList> variants = formationVariants.get(wg->getVersionId());
				if (!variants) {
					runtime::Ref<WalkerGroupList> created = WalkerGroupList::create();
					formationVariants.put(wg->getVersionId(), created);
					variants = created;
				}
				variants->add(wg);
			}
		}
		// Now that all variants are in the map, spawn one randomly
		for (runtime::Ptr<WalkerGroupList> varGroups : formationVariants.values()) {
			runtime::Ptr<WalkerGroup> spawnedGroup = randomElement(varGroups->snapshot());
			spawnedGroup->spawn();
		}
		for (runtime::Ptr<ClusteredNpcList> varWalkers : walkerVariants.values()) {
			runtime::Ptr<ClusteredNpc> spawnedWalker = randomElement(varWalkers->snapshot());
			spawnedWalker->spawn(spawnedWalker->getNpc()->getZ());
		}
	}
}

void InstanceWalkerFormations::changeCluster(WalkerGroup& walkerGroup) {
	if (walkerGroup.getVersionId().empty())
		return;
	runtime::Ptr<WalkerGroupList> varGroups = formationVariants.get(walkerGroup.getVersionId());
	if (!varGroups)
		return;
	std::vector<runtime::Ptr<WalkerGroup>> notSpawned;
	for (runtime::Ptr<WalkerGroup> group : varGroups->snapshot()) {
		if (!group->isSpawned())
			notSpawned.push_back(group);
	}
	runtime::Ptr<WalkerGroup> newGroup = randomElement(notSpawned);
	newGroup->spawn();
	if (walkerGroup.isSpawned())
		walkerGroup.despawn();
}

void InstanceWalkerFormations::changeWalker(model::gameobjects::Npc& npc) {
	std::optional<std::string> walkerId = npc.getSpawn()->getWalkerId();
	if (!walkerId)
		return;
	std::optional<std::string> versionId = dataholders::DataManager::WALKER_VERSIONS_DATA->getRouteVersionId(*walkerId);
	if (!versionId)
		return;
	runtime::Ptr<ClusteredNpcList> varWalkers = walkerVariants.get(*versionId);
	if (!varWalkers)
		return;
	std::vector<runtime::Ptr<ClusteredNpc>> notSpawned;
	for (runtime::Ptr<ClusteredNpc> cNpc : varWalkers->snapshot()) {
		if (!cNpc->getNpc()->isSpawned())
			notSpawned.push_back(cNpc);
	}
	runtime::Ptr<ClusteredNpc> newWalker = randomElement(notSpawned);
	newWalker->spawn(newWalker->getNpc()->getZ());
	if (!npc.isSpawned())
		return;
	for (runtime::Ptr<ClusteredNpc> snpc : varWalkers->snapshot()) {
		if (snpc->getNpc()->equals(npc)) {
			snpc->despawn();
			break;
		}
	}
}

void InstanceWalkerFormations::onInstanceDestroy() {
	SYNCHRONIZED(*this) {
		groupedSpawnObjects.clear();
		walkFormations.clear();
	}
}

} // namespace aion::gameserver::spawnengine
