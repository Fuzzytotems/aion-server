#include "aion/gameserver/dataholders/SpawnsData.h"

#include <algorithm>
#include <string>

#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/WorldMapsData.h"
#include "aion/gameserver/dataholders/loadingutils/EnumTraits.h"
#include "aion/gameserver/model/Race.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/templates/event/EventTemplate.h"
#include "aion/gameserver/model/templates/spawns/Spawn.h"
#include "aion/gameserver/model/templates/spawns/SpawnGroup.h"
#include "aion/gameserver/model/templates/spawns/SpawnMap.h"
#include "aion/gameserver/model/templates/spawns/SpawnSearchResult.h"
#include "aion/gameserver/model/templates/spawns/SpawnSpotTemplate.h"
#include "aion/gameserver/model/templates/spawns/SpawnTemplate.h"
#include "aion/gameserver/model/templates/world/WorldMapTemplate.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/runtime/base/TaskInfo.h"
#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/runtime/lifetime/TaskScope.h"
#include "aion/gameserver/utils/PositionUtil.h"
#include "aion/gameserver/world/WorldPosition.h"
#include "aion/gameserver/world/WorldType.h"

namespace aion::gameserver::dataholders {

using model::templates::spawns::Spawn;
using model::templates::spawns::SpawnGroup;
using model::templates::spawns::SpawnMap;
using model::templates::spawns::SpawnSearchResult;
using model::templates::spawns::SpawnSpotTemplate;
using model::templates::spawns::SpawnTemplate;
using SpawnGroups = std::vector<runtime::Ref<SpawnGroup>>;

namespace {

/** Java: a nullable enum passed to a SpawnGroup constructor that dereferences it */
template <class E>
E required(const std::optional<E>& value, const char* what) {
	if (!value)
		throw runtime::NullPointerException(std::string("SpawnsData: ") + what + " is null");
	return *value;
}

const SpawnGroups* findGroups(const std::unordered_map<int32_t, SpawnGroups>& map, int32_t id) {
	auto it = map.find(id);
	return it != map.end() ? &it->second : nullptr;
}

} // namespace

SpawnsData::SpawnsData() = default;

SpawnsData::~SpawnsData() = default;

void SpawnsData::afterUnmarshal(xml::LoadContext& /*ctx*/, const xml::XmlParent& /*parent*/) {
	// the hook creates RefCounted run-time objects (and SpawnsData writes collection shims): a nested TaskScope when the load runs in one
	// (DataManager), an own one when a holder is bound alone (tests binding templates outside a scope)
	runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::STARTUP));
	for (const SpawnMap& map : templates) {
		addRegularSpawns(map);
		addBaseSpawns(map);
		addRiftSpawns(map);
		addSiegeSpawns(map);
		addVortexSpawns(map);
		addMercenarySpawns(map);
		addAhserionSpawns(map);
	}
	// Java: if (!(parent instanceof EventTemplate)) templates = null (the C++ storage stays: the spawn groups refer to the bound spawns)
}

void SpawnsData::addRegularSpawns(const SpawnMap& map) {
	// Java: allSpawnMaps.compute(mapId, ...) with a nested computeIfAbsent on the npc map (runtime-architecture.md §9: nested compute is legal)
	allSpawnMaps.compute(map.getMapId(), [&map](const int32_t&, runtime::Ptr<NpcSpawnGroups> existing) -> runtime::Ref<NpcSpawnGroups> {
		runtime::Ref<NpcSpawnGroups> mapSpawns =
		  existing ? runtime::Ref<NpcSpawnGroups>(existing) : NpcSpawnGroups::create(AION_LOCK_CLASS(SpawnsData::allSpawnMaps #mapSpawns));
		std::vector<int32_t> customs;
		for (const std::unique_ptr<Spawn>& spawn : map.getSpawns()) {
			if (std::find(customs.begin(), customs.end(), spawn->getNpcId()) != customs.end())
				continue;
			if (spawn->isCustom() && spawn->getEventTemplate() == nullptr) { // custom event spawns are handled in Event class
				mapSpawns->remove(spawn->getNpcId());
				customs.push_back(spawn->getNpcId());
			}
			runtime::Ptr<SpawnGroupList> spawnGroups =
			  mapSpawns->computeIfAbsent(spawn->getNpcId(), [] { return SpawnGroupList::create(AION_LOCK_CLASS(SpawnsData::allSpawnMaps #spawnGroups)); });
			spawnGroups->add(SpawnGroup::create(map.getMapId(), spawn.get()));
		}
		return mapSpawns;
	});
}

void SpawnsData::addBaseSpawns(const SpawnMap& map) {
	for (const model::templates::spawns::basespawns::BaseSpawn& baseSpawn : map.getBaseSpawns()) {
		int32_t baseId = baseSpawn.getId();
		SpawnGroups& baseSpawns = baseSpawnMaps[baseId];
		for (const auto& simpleRace : baseSpawn.getOccupierTemplates()) {
			for (const std::unique_ptr<Spawn>& spawn : simpleRace.getSpawns())
				baseSpawns.push_back(SpawnGroup::create(map.getMapId(), spawn.get(), baseId, required(simpleRace.getOccupier(), "the base occupier")));
		}
	}
}

void SpawnsData::addRiftSpawns(const SpawnMap& map) {
	for (const model::templates::spawns::riftspawns::RiftSpawn& rift : map.getRiftSpawns()) {
		SpawnGroups& riftSpawns = riftSpawnMaps[rift.getId()];
		for (const std::unique_ptr<Spawn>& spawn : rift.getSpawns())
			riftSpawns.push_back(SpawnGroup::create(map.getMapId(), spawn.get(), rift.getId()));
	}
}

void SpawnsData::addSiegeSpawns(const SpawnMap& map) {
	for (const model::templates::spawns::siegespawns::SiegeSpawn& siegeSpawn : map.getSiegeSpawns()) {
		int32_t siegeId = siegeSpawn.getSiegeId();
		SpawnGroups& siegeSpawns = siegeSpawnMaps[siegeId];
		for (const auto& race : siegeSpawn.getSiegeRaceTemplates()) {
			for (const auto& mod : race.getSiegeModTemplates()) {
				// Java: if (mod.getSpawns() == null) continue; (an absent list is empty)
				for (const std::unique_ptr<Spawn>& spawn : mod.getSpawns())
					siegeSpawns.push_back(
					  SpawnGroup::create(map.getMapId(), spawn.get(), siegeId, race.getSiegeRace(), required(mod.getSiegeModType(), "the siege mod type")));
			}
		}
	}
}

void SpawnsData::addVortexSpawns(const SpawnMap& map) {
	for (const model::templates::spawns::vortexspawns::VortexSpawn& vortexSpawn : map.getVortexSpawns()) {
		int32_t id = vortexSpawn.getId();
		SpawnGroups& vortexSpawns = vortexSpawnMaps[id];
		for (const auto& type : vortexSpawn.getStateTemplates()) {
			for (const std::unique_ptr<Spawn>& spawn : type.getSpawns())
				vortexSpawns.push_back(SpawnGroup::create(map.getMapId(), spawn.get(), id, required(type.getStateType(), "the vortex state type")));
		}
	}
}

void SpawnsData::addMercenarySpawns(const SpawnMap& map) {
	for (const model::templates::spawns::mercenaries::MercenarySpawn& mercenarySpawn : map.getMercenarySpawns()) {
		int32_t id = mercenarySpawn.getSiegeId();
		mercenarySpawns.insert_or_assign(id, &mercenarySpawn);
		for (const auto& mrace : mercenarySpawn.getMercenaryRaces()) {
			for (const auto& mzone : mrace.getMercenaryZones()) {
				// the zone belongs to the spawn map being bound (not published yet)
				auto& zone = const_cast<model::templates::spawns::mercenaries::MercenaryZone&>(mzone);
				zone.setWorldId(map.getMapId());
				zone.setSiegeId(mercenarySpawn.getSiegeId());
			}
		}
	}
}

void SpawnsData::addAhserionSpawns(const SpawnMap& map) {
	for (const model::templates::spawns::panesterra::AhserionsFlightSpawn& ahserionSpawn : map.getAhserionSpawns()) {
		const auto faction = required(ahserionSpawn.getFaction(), "the Ahserion faction");
		int32_t teamId = xml::enumOrdinal(faction);
		SpawnGroups& ahserionSpawns = ahserionSpawnMaps[teamId];
		for (const auto& stageTemplate : ahserionSpawn.getStageSpawnTemplate()) {
			// Java: if (stageTemplate.getSpawns() == null) continue; (an absent list is empty)
			for (const std::unique_ptr<Spawn>& spawn : stageTemplate.getSpawns())
				ahserionSpawns.push_back(SpawnGroup::create(map.getMapId(), spawn.get(), stageTemplate.getStage(), faction));
		}
	}
}

SpawnGroups SpawnsData::getSpawnsByWorldId(int32_t worldId) const {
	runtime::Ptr<NpcSpawnGroups> spawnGroupsByNpcId = allSpawnMaps.get(worldId);
	SpawnGroups result;
	if (!spawnGroupsByNpcId)
		return result;
	for (runtime::Ptr<SpawnGroupList> spawnGroups : spawnGroupsByNpcId->values()) {
		for (runtime::Ptr<SpawnGroup> group : spawnGroups->snapshot())
			result.emplace_back(group);
	}
	return result;
}

SpawnGroups SpawnsData::getSpawnsForNpc(int32_t worldId, int32_t npcId) const {
	SpawnGroups result;
	runtime::Ptr<NpcSpawnGroups> spawnGroupsByNpcId = allSpawnMaps.get(worldId);
	runtime::Ptr<SpawnGroupList> spawnGroups = spawnGroupsByNpcId ? spawnGroupsByNpcId->get(npcId) : nullptr;
	if (!spawnGroups)
		return result;
	for (runtime::Ptr<SpawnGroup> group : spawnGroups->snapshot())
		result.emplace_back(group);
	return result;
}

const SpawnGroups* SpawnsData::getBaseSpawnsByLocId(int32_t id) const {
	return findGroups(baseSpawnMaps, id);
}

const SpawnGroups* SpawnsData::getRiftSpawnsByLocId(int32_t id) const {
	return findGroups(riftSpawnMaps, id);
}

const SpawnGroups* SpawnsData::getSiegeSpawnsByLocId(int32_t siegeId) const {
	return findGroups(siegeSpawnMaps, siegeId);
}

const SpawnGroups* SpawnsData::getVortexSpawnsByLocId(int32_t id) const {
	return findGroups(vortexSpawnMaps, id);
}

const model::templates::spawns::mercenaries::MercenarySpawn* SpawnsData::getMercenarySpawnBySiegeId(int32_t id) const {
	auto it = mercenarySpawns.find(id);
	return it != mercenarySpawns.end() ? it->second : nullptr;
}

bool SpawnsData::saveSpawn(model::gameobjects::VisibleObject& /*visibleObject*/, bool /*delete_*/) {
	// the //spawn admin write-back: reads and writes spawn XML files with JAXBUtil and the XSD (static-data.md §3.6 item 7, after the load path)
	AION_UNPORTED();
}

const Spawn* SpawnsData::findSpawnTemplate(const SpawnMap& spawnMap, SpawnTemplate& spawn, bool exactMatch) {
	if (spawnMap.getMapId() != spawn.getWorldId())
		return nullptr;
	for (const std::unique_ptr<Spawn>& s : spawnMap.getSpawns()) {
		if (s->getNpcId() != spawn.getNpcId())
			continue;
		if (!exactMatch)
			return s.get();
		for (const SpawnSpotTemplate& spot : s->getSpawnSpotTemplates()) {
			if (positionMatches(spawn, spot))
				return s.get();
		}
	}
	return nullptr;
}

bool SpawnsData::positionMatches(const SpawnTemplate& spawn, const SpawnSpotTemplate& spawnSpotTemplate) {
	return spawnSpotTemplate.getX() == spawn.getX() && spawnSpotTemplate.getY() == spawn.getY() && spawnSpotTemplate.getZ() == spawn.getZ() &&
	       spawnSpotTemplate.getHeading() == spawn.getHeading();
}

int32_t SpawnsData::size() const {
	return allSpawnMaps.size();
}

std::optional<SpawnSearchResult> SpawnsData::getNearestSpawnByNpcId(runtime::Ptr<model::gameobjects::player::Player> player, int32_t npcId,
                                                                    int32_t worldId) const {
	SpawnGroups spawns = getSpawnsForNpc(worldId, npcId);
	if (spawns.empty()) { // -> there are no spawns for this npcId on the current map
		if (!player)
			throw runtime::NullPointerException("Cannot invoke \"Player.getRace()\" because \"player\" is null");
		const model::Race race = player->getRace();
		// search all maps of players race
		for (const model::templates::world::WorldMapTemplate* template_ : *DataManager::WORLD_MAPS_DATA) {
			if (template_->getMapId() == worldId)
				continue;
			if ((template_->getWorldType() == world::WorldType::ELYSEA && race == model::Race::ELYOS) ||
			    (template_->getWorldType() == world::WorldType::ASMODAE && race == model::Race::ASMODIANS)) {
				spawns = getSpawnsForNpc(template_->getMapId(), npcId);
				if (!spawns.empty()) {
					worldId = template_->getMapId();
					break;
				}
			}
		}

		// -> there are no spawns for this npcId on all maps of players race
		// search all other maps
		if (spawns.empty()) {
			for (const model::templates::world::WorldMapTemplate* template_ : *DataManager::WORLD_MAPS_DATA) {
				if ((template_->getMapId() == worldId) || (template_->getWorldType() == world::WorldType::ELYSEA && race == model::Race::ELYOS) ||
				    (template_->getWorldType() == world::WorldType::ASMODAE && race == model::Race::ASMODIANS)) {
					continue;
				}
				spawns = getSpawnsForNpc(template_->getMapId(), npcId);
				if (!spawns.empty()) {
					worldId = template_->getMapId();
					break;
				}
			}
		}
	}

	return getNearestSpawn(player ? player->getPosition() : nullptr, spawns, worldId);
}

std::optional<SpawnSearchResult> SpawnsData::getNearestSpawn(runtime::Ptr<world::WorldPosition> position, const SpawnGroups& spawnGroups,
                                                             int32_t worldId) {
	if (!position || spawnGroups.empty())
		return std::nullopt;
	if (worldId != position->getMapId()) {
		const runtime::Ref<SpawnGroup>& spawnGroup = spawnGroups[0];
		return spawnGroup->getSpawnTemplates().isEmpty()
		         ? std::nullopt
		         : std::optional<SpawnSearchResult>(toSpawnSearchResult(worldId, *spawnGroup->getSpawnTemplates().get(0)));
	}

	runtime::Ptr<SpawnTemplate> temp;
	float distance = 0;
	for (const runtime::Ref<SpawnGroup>& spawnGroup : spawnGroups) {
		bool found = false;
		for (runtime::Ptr<SpawnTemplate> spot : spawnGroup->getSpawnTemplates().snapshot()) {
			if (!temp) {
				temp = spot;
				distance = static_cast<float>(
				  utils::PositionUtil::getDistance(position->getX(), position->getY(), position->getZ(), spot->getX(), spot->getY(), spot->getZ()));
				if (distance <= 1.0f) {
					found = true;
					break;
				}
			} else {
				float dist = static_cast<float>(
				  utils::PositionUtil::getDistance(position->getX(), position->getY(), position->getZ(), spot->getX(), spot->getY(), spot->getZ()));
				if (dist < distance) {
					distance = dist;
					temp = spot;
					if (distance <= 1.0f) {
						found = true;
						break;
					}
				}
			}
		}
		if (found) // Java: break outerLoop
			break;
	}

	return !temp ? std::nullopt : std::optional<SpawnSearchResult>(toSpawnSearchResult(worldId, *temp));
}

SpawnSearchResult SpawnsData::toSpawnSearchResult(int32_t worldId, SpawnTemplate& spot) {
	std::optional<std::string> walkerId = spot.getWalkerId();
	return SpawnSearchResult(worldId, SpawnSpotTemplate(spot.getX(), spot.getY(), spot.getZ(), spot.getHeading(), spot.getRandomWalkRange(),
	                                                    walkerId ? std::optional<std::string_view>(*walkerId) : std::nullopt, spot.getWalkerIndex()));
}

std::optional<SpawnSearchResult> SpawnsData::getFirstSpawnByNpcId(int32_t worldId, int32_t npcId) const {
	SpawnGroups spawnGroups = getSpawnsForNpc(worldId, npcId);

	if (spawnGroups.empty()) {
		for (const model::templates::world::WorldMapTemplate* template_ : *DataManager::WORLD_MAPS_DATA) {
			if (template_->getMapId() == worldId)
				continue;
			spawnGroups = getSpawnsForNpc(template_->getMapId(), npcId);
			if (!spawnGroups.empty()) {
				worldId = template_->getMapId();
				break;
			}
		}
		if (spawnGroups.empty())
			return std::nullopt;
	}
	runtime::PartList<SpawnTemplate>& spawnSpots = spawnGroups[0]->getSpawnTemplates();
	return spawnSpots.isEmpty() ? std::nullopt : std::optional<SpawnSearchResult>(toSpawnSearchResult(worldId, *spawnSpots.get(0)));
}

void SpawnsData::removeEventSpawnObjects(const model::templates::event::EventTemplate& eventTemplate) {
	for (runtime::Ptr<NpcSpawnGroups> spawnGroupsByNpcId : allSpawnMaps.values()) {
		// Java: allSpawnGroups.forEach(spawnGroups -> spawnGroups.removeIf(eventTemplate.equals(spawnGroup.getEventTemplate()))), then removeIf(isEmpty)
		for (runtime::Ptr<SpawnGroupList> spawnGroups : spawnGroupsByNpcId->values())
			spawnGroups->removeIf([&eventTemplate](runtime::Ptr<SpawnGroup> spawnGroup) { return spawnGroup->getEventTemplate() == &eventTemplate; });
		spawnGroupsByNpcId->values().removeIf([](runtime::Ptr<SpawnGroupList> spawnGroups) { return spawnGroups->isEmpty(); });
	}
}

const SpawnGroups* SpawnsData::getAhserionSpawnByTeamId(int32_t id) const {
	return findGroups(ahserionSpawnMaps, id);
}

void SpawnsData::addAllNpcIdsToSet(std::unordered_set<int32_t>& npcIds) const {
	for (runtime::Ptr<NpcSpawnGroups> spawnGroupsByNpcId : allSpawnMaps.values()) {
		for (int32_t npcId : spawnGroupsByNpcId->keySet())
			npcIds.insert(npcId);
	}
	for (const auto& [siegeId, mercenarySpawn] : mercenarySpawns) {
		for (const auto& race : mercenarySpawn->getMercenaryRaces()) {
			for (const auto& zone : race.getMercenaryZones()) {
				for (const std::unique_ptr<Spawn>& spawn : zone.getSpawns())
					npcIds.insert(spawn->getNpcId());
			}
		}
	}
	for (const auto* map : {&baseSpawnMaps, &riftSpawnMaps, &siegeSpawnMaps, &vortexSpawnMaps, &ahserionSpawnMaps}) {
		for (const auto& [id, groups] : *map) {
			for (const runtime::Ref<SpawnGroup>& group : groups)
				npcIds.insert(group->getNpcId());
		}
	}
}

void SpawnsData::UnprocessedSpawns::afterUnmarshal(xml::LoadContext& /*ctx*/, const xml::XmlParent& /*parent*/) {
	// Java: an empty override (no indexes for spawn files read back by saveSpawn)
}

} // namespace aion::gameserver::dataholders
