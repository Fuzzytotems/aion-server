#pragma once

#include <cstdint>
#include <optional>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "aion/gameserver/dataholders/SpawnsData.xml.h"
#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/model/templates/spawns/SpawnSearchResult.h"
#include "aion/gameserver/model/templates/spawns/fwd.h"
#include "aion/gameserver/runtime/collections/Rc.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/world/fwd.h"

namespace aion::gameserver::dataholders {

/**
 * Java com.aionemu.gameserver.dataholders.SpawnsData.
 * <p>
 * C++: one of the explicitly mutable holders (DataManager::SPAWNS_DATA is a MutableHolderRef; static-data.md amendments §6, runtime-architecture.md
 * §9). The spawn groups are RefCounted run-time objects owning their spawn templates. `allSpawnMaps`, which Event start and stop change at run
 * time, is a ConcurrentHashMap of synchronized npc id maps and spawn group lists (collection shims): the nested compute of addRegularSpawns runs
 * as written, getters return snapshots. The base, rift, siege, vortex, mercenary and Ahserion indexes are only built by afterUnmarshal (before
 * publication) and read afterwards, so they are plain maps. SpawnsData is also bound as the <spawns> child of an event template; its hook keeps
 * the bound spawn maps then (Java: `if (!(parent instanceof EventTemplate)) templates = null`; the C++ storage always stays).
 * getSpawnsByWorldId returns the groups npc id by npc id in the order the RcHashMap shim keeps (the order in which each npc id was last added to
 * the world's map), not in Java's HashMap bucket order; the order decides only the spawn order of SpawnEngine.spawnInstance (DEVIATION,
 * docs/deviations/P4-09.md). saveSpawn (the //spawn admin write-back through JAXB) comes after the load path (static-data.md §3.6 item 7) and
 * stays unported.
 *
 * @author xTz, Rolandas, Neon
 */
class SpawnsData : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/dataholders/SpawnsData.xml.inc"
public:
	using SpawnGroupList = runtime::RcArrayList<runtime::Ref<model::templates::spawns::SpawnGroup>>;
	using NpcSpawnGroups = runtime::RcHashMap<int32_t, runtime::Ref<SpawnGroupList>>;

private:
	runtime::ConcurrentHashMap<int32_t, runtime::Ref<NpcSpawnGroups>> allSpawnMaps{AION_LOCK_CLASS(SpawnsData::allSpawnMaps #stripe)};
	std::unordered_map<int32_t, std::vector<runtime::Ref<model::templates::spawns::SpawnGroup>>> baseSpawnMaps;
	std::unordered_map<int32_t, std::vector<runtime::Ref<model::templates::spawns::SpawnGroup>>> riftSpawnMaps;
	std::unordered_map<int32_t, std::vector<runtime::Ref<model::templates::spawns::SpawnGroup>>> siegeSpawnMaps;
	std::unordered_map<int32_t, std::vector<runtime::Ref<model::templates::spawns::SpawnGroup>>> vortexSpawnMaps;
	std::unordered_map<int32_t, const model::templates::spawns::mercenaries::MercenarySpawn*> mercenarySpawns;
	std::unordered_map<int32_t, std::vector<runtime::Ref<model::templates::spawns::SpawnGroup>>> ahserionSpawnMaps; // Ahserion's flight

public:
	/** C++ only: out of line, where the RefCounted element types are complete */
	SpawnsData();
	~SpawnsData();

	/** Java: adds the regular spawns of the map (afterUnmarshal, Event start, saveSpawn); a custom spawn replaces the npc's groups */
	void addRegularSpawns(const model::templates::spawns::SpawnMap& map);

private:
	void addBaseSpawns(const model::templates::spawns::SpawnMap& map);

	void addRiftSpawns(const model::templates::spawns::SpawnMap& map);

	void addSiegeSpawns(const model::templates::spawns::SpawnMap& map);

	void addVortexSpawns(const model::templates::spawns::SpawnMap& map);

	void addMercenarySpawns(const model::templates::spawns::SpawnMap& map);

	void addAhserionSpawns(const model::templates::spawns::SpawnMap& map);

public:
	/** @return a snapshot of all spawn groups of the world (empty if there are none) */
	std::vector<runtime::Ref<model::templates::spawns::SpawnGroup>> getSpawnsByWorldId(int32_t worldId) const;

	/** @return a snapshot of the npc's spawn groups in the world (empty if there are none) */
	std::vector<runtime::Ref<model::templates::spawns::SpawnGroup>> getSpawnsForNpc(int32_t worldId, int32_t npcId) const;

	/** @return the spawn groups, nullptr (Java null) if there are none */
	const std::vector<runtime::Ref<model::templates::spawns::SpawnGroup>>* getBaseSpawnsByLocId(int32_t id) const;

	const std::vector<runtime::Ref<model::templates::spawns::SpawnGroup>>* getRiftSpawnsByLocId(int32_t id) const;

	const std::vector<runtime::Ref<model::templates::spawns::SpawnGroup>>* getSiegeSpawnsByLocId(int32_t siegeId) const;

	const std::vector<runtime::Ref<model::templates::spawns::SpawnGroup>>* getVortexSpawnsByLocId(int32_t id) const;

	/** @return the mercenary spawns, nullptr (Java null) if there are none */
	const model::templates::spawns::mercenaries::MercenarySpawn* getMercenarySpawnBySiegeId(int32_t id) const;

	/** Java (synchronized): saves the object's spawn into ./data/static_data/spawns/.../New/. Unported: write-back */
	bool saveSpawn(model::gameobjects::VisibleObject& visibleObject, bool delete_);

private:
	static const model::templates::spawns::Spawn* findSpawnTemplate(const model::templates::spawns::SpawnMap& spawnMap,
	                                                                model::templates::spawns::SpawnTemplate& spawn, bool exactMatch);

	static bool positionMatches(const model::templates::spawns::SpawnTemplate& spawn,
	                            const model::templates::spawns::SpawnSpotTemplate& spawnSpotTemplate);

public:
	int32_t size() const;

	/**
	 * first search: current map
	 * second search: all maps of players race
	 * third search: all other maps
	 */
	std::optional<model::templates::spawns::SpawnSearchResult> getNearestSpawnByNpcId(runtime::Ptr<model::gameobjects::player::Player> player,
	                                                                                  int32_t npcId, int32_t worldId) const;

private:
	static std::optional<model::templates::spawns::SpawnSearchResult>
	getNearestSpawn(runtime::Ptr<world::WorldPosition> position, const std::vector<runtime::Ref<model::templates::spawns::SpawnGroup>>& spawnGroups,
	                int32_t worldId);

	static model::templates::spawns::SpawnSearchResult toSpawnSearchResult(int32_t worldId, model::templates::spawns::SpawnTemplate& spot);

public:
	/**
	 * @param worldId
	 *          Optional. If provided, searches in this world first
	 * @return template for the spot, std::nullopt (Java null) if there is none
	 */
	std::optional<model::templates::spawns::SpawnSearchResult> getFirstSpawnByNpcId(int32_t worldId, int32_t npcId) const;

	void removeEventSpawnObjects(const model::templates::event::EventTemplate& eventTemplate);

	/** @return the spawn groups of the team, nullptr (Java null) if there are none */
	const std::vector<runtime::Ref<model::templates::spawns::SpawnGroup>>* getAhserionSpawnByTeamId(int32_t id) const;

	void addAllNpcIdsToSet(std::unordered_set<int32_t>& npcIds) const;
};

/** Java com.aionemu.gameserver.dataholders.SpawnsData.UnprocessedSpawns: spawn files read back by saveSpawn, without building indexes. */
class SpawnsData::UnprocessedSpawns : public ::aion::gameserver::dataholders::SpawnsData {
#include "aion/gameserver/dataholders/SpawnsData_UnprocessedSpawns.xml.inc"
public:
};

} // namespace aion::gameserver::dataholders
