#include "aion/gameserver/dataholders/SpawnsData.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <optional>
#include <sstream>
#include <string>
#include <typeinfo>
#include <vector>

#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/SpawnsData.bind.h"
#include "aion/gameserver/dataholders/WorldMapsData.h"
#include "aion/commons/configuration/transformers/NumberTransformer.h"
#include "aion/commons/logging/LoggerFactory.h"
#include "aion/commons/utils/Numbers.h"
#include "aion/commons/utils/StringUtils.h"
#include "aion/gameserver/dataholders/loadingutils/EnumTraits.h"
#include "aion/gameserver/dataholders/loadingutils/LoadContext.h"
#include "aion/gameserver/dataholders/loadingutils/StaticDataLoader.h"
#include "aion/gameserver/dataholders/loadingutils/XmlDocument.h"
#include "aion/gameserver/geoEngine/math/JavaFloat.h"
#include "aion/gameserver/model/gameobjects/Gatherable.h"
#include "aion/gameserver/model/Race.h"
#include "aion/gameserver/model/gameobjects/VisibleObject.h"
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
#include "aion/gameserver/runtime/sync/Monitor.h"
#include "aion/gameserver/spawnengine/SpawnHandlerType.h"
#include "aion/gameserver/utils/PositionUtil.h"
#include "aion/gameserver/world/WorldMap.h"
#include "aion/gameserver/world/WorldMapInstance.h"
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

// ---- saveSpawn's write-back (SpawnsData.java:216-273, m5j D7/D14 = (a)) -----------------------------------------------------------------
// Java reads the spawn files with JAXB (UnprocessedSpawns, Spawn, SpawnSpotTemplate, TemporarySpawn) and writes the New file with
// JAXBUtil.serialize (JAXB_FORMATTED_OUTPUT, UTF-8). C++ reads the same files into these plain records (the attributes of the JAXB classes,
// their field defaults) and writes them in the JAXB RI's formatted form: the XML declaration with standalone="yes", four spaces per level,
// the attributes in field order, the beforeMarshal hooks' omissions (Spawn: pool 0, custom false; SpawnSpotTemplate: static_id, random_walk,
// state, walker_index 0, aerial_spawn false), floats as Float.toString, empty elements self-closed.

struct SavedTemporarySpawn {
	std::optional<std::vector<std::string>> weekdays; // xs:list of DayOfWeek names
	std::optional<std::string> spawnTime;
	std::optional<std::string> despawnTime;
};

struct SavedSpot {
	float x = 0.0f;
	float y = 0.0f;
	float z = 0.0f;
	int8_t h = 0;
	int32_t staticId = 0;
	int32_t randomWalk = 0;
	std::optional<std::string> walkerId;
	std::optional<int32_t> walkerIdx;
	std::optional<std::string> anchor;
	int32_t state = 0;
	std::optional<bool> aerialSpawn;
	std::optional<SavedTemporarySpawn> temporarySpawn;
	std::optional<std::string> ai;
};

struct SavedSpawn {
	int32_t npcId = 0;
	int32_t respawnTime = 0; // Java: Integer respawnTime = 0, never null when bound
	int32_t pool = 0;
	int8_t difficultId = 0;
	bool custom = false;
	std::optional<std::string> handler;
	std::optional<SavedTemporarySpawn> temporarySpawn;
	std::vector<SavedSpot> spots;
};

struct SavedSpawnMap {
	int32_t mapId = 0;
	std::vector<SavedSpawn> spawns;
};

std::optional<std::string> attributeOf(const pugi::xml_node& node, const char* name) {
	pugi::xml_attribute attribute = node.attribute(name);
	return attribute ? std::optional<std::string>(attribute.value()) : std::nullopt;
}

int32_t intAttribute(const pugi::xml_node& node, const char* name, int32_t defaultValue) {
	std::optional<std::string> value = attributeOf(node, name);
	return value ? commons::utils::parseInt(commons::utils::StringUtils::trim(*value)) : defaultValue;
}

std::optional<SavedTemporarySpawn> readTemporarySpawn(const pugi::xml_node& parent) {
	pugi::xml_node node = parent.child("temporary_spawn");
	if (!node)
		return std::nullopt;
	SavedTemporarySpawn temporary;
	if (std::optional<std::string> weekdays = attributeOf(node, "weekdays")) {
		std::vector<std::string> days;
		std::istringstream in(*weekdays);
		for (std::string day; in >> day;)
			days.push_back(day);
		temporary.weekdays = days;
	}
	temporary.spawnTime = attributeOf(node, "spawn_time");
	temporary.despawnTime = attributeOf(node, "despawn_time");
	return temporary;
}

/** Java: JAXBUtil.deserialize(file, UnprocessedSpawns.class, schema).getTemplates() */
std::vector<SavedSpawnMap> readSpawnFile(const std::filesystem::path& file) {
	std::unique_ptr<xml::XmlDocument> document = xml::XmlDocument::parseFile(file);
	std::vector<SavedSpawnMap> maps;
	for (pugi::xml_node mapNode : document->root().children("spawn_map")) {
		SavedSpawnMap& map = maps.emplace_back();
		map.mapId = intAttribute(mapNode, "map_id", 0);
		for (pugi::xml_node spawnNode : mapNode.children("spawn")) {
			SavedSpawn& spawn = map.spawns.emplace_back();
			spawn.npcId = intAttribute(spawnNode, "npc_id", 0);
			spawn.respawnTime = intAttribute(spawnNode, "respawn_time", 0);
			spawn.pool = intAttribute(spawnNode, "pool", 0);
			spawn.difficultId = static_cast<int8_t>(intAttribute(spawnNode, "difficult_id", 0));
			std::optional<std::string> custom = attributeOf(spawnNode, "custom");
			spawn.custom = custom && (*custom == "true" || *custom == "1");
			spawn.handler = attributeOf(spawnNode, "handler");
			spawn.temporarySpawn = readTemporarySpawn(spawnNode);
			for (pugi::xml_node spotNode : spawnNode.children("spot")) {
				SavedSpot& spot = spawn.spots.emplace_back();
				using commons::configuration::transformers::NumberParser::parseFloat;
				spot.x = parseFloat(attributeOf(spotNode, "x").value_or("0"));
				spot.y = parseFloat(attributeOf(spotNode, "y").value_or("0"));
				spot.z = parseFloat(attributeOf(spotNode, "z").value_or("0"));
				spot.h = static_cast<int8_t>(intAttribute(spotNode, "h", 0));
				spot.staticId = intAttribute(spotNode, "static_id", 0);
				spot.randomWalk = intAttribute(spotNode, "random_walk", 0);
				spot.walkerId = attributeOf(spotNode, "walker_id");
				if (attributeOf(spotNode, "walker_index"))
					spot.walkerIdx = intAttribute(spotNode, "walker_index", 0);
				spot.anchor = attributeOf(spotNode, "anchor");
				spot.state = intAttribute(spotNode, "state", 0);
				if (std::optional<std::string> aerial = attributeOf(spotNode, "aerial_spawn"))
					spot.aerialSpawn = *aerial == "true" || *aerial == "1";
				spot.temporarySpawn = readTemporarySpawn(spotNode);
				spot.ai = attributeOf(spotNode, "ai");
			}
		}
	}
	return maps;
}

/** JAXB's attribute value escaping */
std::string escaped(std::string_view value) {
	std::string out;
	for (char c : value) {
		switch (c) {
			case '&': out += "&amp;"; break;
			case '<': out += "&lt;"; break;
			case '>': out += "&gt;"; break;
			case '"': out += "&quot;"; break;
			default: out += c;
		}
	}
	return out;
}

void writeAttribute(std::string& out, const char* name, std::string_view value) {
	out += ' ';
	out += name;
	out += "=\"";
	out += escaped(value);
	out += '"';
}

void writeTemporarySpawn(std::string& out, const SavedTemporarySpawn& temporary, const std::string& indent) {
	out += indent + "<temporary_spawn";
	if (temporary.weekdays) {
		std::string days;
		for (const std::string& day : *temporary.weekdays)
			days += (days.empty() ? "" : " ") + day;
		writeAttribute(out, "weekdays", days);
	}
	if (temporary.spawnTime)
		writeAttribute(out, "spawn_time", *temporary.spawnTime);
	if (temporary.despawnTime)
		writeAttribute(out, "despawn_time", *temporary.despawnTime);
	out += "/>\n";
}

/** Java: JAXBUtil.serialize(data, schema) of the spawn maps (JAXB_FORMATTED_OUTPUT, JAXB_ENCODING UTF-8) */
std::string serializeSpawns(const std::vector<SavedSpawnMap>& maps) {
	using geoEngine::math::JavaFloat;
	std::string out = "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n<spawns>\n";
	for (const SavedSpawnMap& map : maps) {
		out += "    <spawn_map";
		writeAttribute(out, "map_id", std::to_string(map.mapId));
		if (map.spawns.empty()) {
			out += "/>\n";
			continue;
		}
		out += ">\n";
		for (const SavedSpawn& spawn : map.spawns) {
			out += "        <spawn";
			writeAttribute(out, "npc_id", std::to_string(spawn.npcId));
			writeAttribute(out, "respawn_time", std::to_string(spawn.respawnTime));
			if (spawn.pool != 0) // beforeMarshal: pool 0 is null
				writeAttribute(out, "pool", std::to_string(spawn.pool));
			writeAttribute(out, "difficult_id", std::to_string(spawn.difficultId));
			if (spawn.custom) // beforeMarshal: false is null
				writeAttribute(out, "custom", "true");
			if (spawn.handler)
				writeAttribute(out, "handler", *spawn.handler);
			if (!spawn.temporarySpawn && spawn.spots.empty()) {
				out += "/>\n";
				continue;
			}
			out += ">\n";
			if (spawn.temporarySpawn)
				writeTemporarySpawn(out, *spawn.temporarySpawn, "            ");
			for (const SavedSpot& spot : spawn.spots) {
				out += "            <spot";
				writeAttribute(out, "x", JavaFloat::toString(spot.x));
				writeAttribute(out, "y", JavaFloat::toString(spot.y));
				writeAttribute(out, "z", JavaFloat::toString(spot.z));
				writeAttribute(out, "h", std::to_string(spot.h));
				if (spot.staticId != 0) // beforeMarshal: 0 is null
					writeAttribute(out, "static_id", std::to_string(spot.staticId));
				if (spot.randomWalk != 0)
					writeAttribute(out, "random_walk", std::to_string(spot.randomWalk));
				if (spot.walkerId)
					writeAttribute(out, "walker_id", *spot.walkerId);
				if (spot.walkerIdx && *spot.walkerIdx != 0)
					writeAttribute(out, "walker_index", std::to_string(*spot.walkerIdx));
				if (spot.anchor)
					writeAttribute(out, "anchor", *spot.anchor);
				if (spot.state != 0)
					writeAttribute(out, "state", std::to_string(spot.state));
				if (spot.aerialSpawn && *spot.aerialSpawn)
					writeAttribute(out, "aerial_spawn", "true");
				if (spot.ai)
					writeAttribute(out, "ai", *spot.ai);
				if (spot.temporarySpawn) {
					out += ">\n";
					writeTemporarySpawn(out, *spot.temporarySpawn, "                ");
					out += "            </spot>\n";
				} else {
					out += "/>\n";
				}
			}
			out += "        </spawn>\n";
		}
		out += "    </spawn_map>\n";
	}
	return out + "</spawns>\n";
}

/** Java: positionMatches (SpawnsData.java:311-314) */
bool savedPositionMatches(SpawnTemplate& spawn, const SavedSpot& spot) {
	return spot.x == spawn.getX() && spot.y == spawn.getY() && spot.z == spawn.getZ() && spot.h == spawn.getHeading();
}

/** Java: findSpawnTemplate (SpawnsData.java:302-309) */
SavedSpawn* findSavedSpawn(SavedSpawnMap& spawnMap, SpawnTemplate& spawn, bool exactMatch) {
	if (spawnMap.mapId != spawn.getWorldId())
		return nullptr;
	for (SavedSpawn& s : spawnMap.spawns)
		if (s.npcId == spawn.getNpcId() &&
			(!exactMatch || std::any_of(s.spots.begin(), s.spots.end(), [&spawn](const SavedSpot& spot) { return savedPositionMatches(spawn, spot); })))
			return &s;
	return nullptr;
}

/**
 * Java: loadSpawnsFromTemplateFiles (SpawnsData.java:277-300): the first match in the folder's XML files (Files.walkFileTree's order: each
 * directory's entries as listed, depth first); an I/O error of the walk is logged and ends it.
 */
std::optional<SavedSpawn> loadSpawnFromTemplateFiles(const std::string& folder, SpawnTemplate& spawn, bool exactMatch) {
	try {
		for (const std::filesystem::directory_entry& entry : std::filesystem::recursive_directory_iterator(std::filesystem::path(folder))) {
			std::string name = commons::utils::StringUtils::toLowerCase(entry.path().string());
			if (!entry.is_regular_file() || !name.ends_with(".xml"))
				continue;
			for (SavedSpawnMap& spawnMap : readSpawnFile(entry.path()))
				if (SavedSpawn* s = findSavedSpawn(spawnMap, spawn, exactMatch))
					return *s;
		}
	} catch (const std::filesystem::filesystem_error& e) {
		commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.dataholders.SpawnsData").error("", e);
	}
	return std::nullopt;
}

/** Java: getRelativePath (SpawnsData.java:316-325) */
std::string relativePathOf(model::gameobjects::VisibleObject& visibleObject) {
	if (visibleObject.getSpawn()->getHandlerType() == spawnengine::SpawnHandlerType::RIFT)
		return "Rifts";
	else if (dynamic_cast<model::gameobjects::Gatherable*>(&visibleObject) != nullptr)
		return "Gather";
	else if (visibleObject.getPosition()->getWorldMapInstance()->getParent()->isInstanceType())
		return "Instances";
	else
		return "Npcs";
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

// Java SpawnsData.java:204-274 (synchronized). The guard prefix (:205-214) is m5j K-10; the write-back (:216-273) is m5j CP5 (D14 = (a),
// faithful): ./data/static_data/spawns/<Npcs|Gather|Instances|Rifts>/New/<world id>_<map name>.xml relative to the working directory
bool SpawnsData::saveSpawn(model::gameobjects::VisibleObject& visibleObject, bool delete_) {
	static runtime::Monitor* const lock = new runtime::Monitor{AION_LOCK_CLASS(SpawnsData::saveSpawn)}; // Java: the method's monitor
	SYNCHRONIZED(*lock) {
		runtime::Ptr<model::templates::spawns::SpawnTemplate> spawn = visibleObject.getSpawn();
		if (spawn == nullptr) // some objects like house objects have no spawn template
			return false;
		if (typeid(*spawn) != typeid(model::templates::spawns::SpawnTemplate)) // do not save special/temporary spawns (siege, base, rift spawn, ...) as world spawns
			return false;
		if (spawn->getRespawnTime() <= 0) // do not save single time spawns (world raid, handler spawn, ...) as world spawns
			return false;
		if (spawn->isTemporarySpawn()) // spawn start and end times of temporary world spawns (shugos, agrints, ...) would get lost
			return false;

		const std::string folder = "./data/static_data/spawns/" + relativePathOf(visibleObject); // Java: String folder
		std::string mapName = visibleObject.getPosition()->getWorldMapInstance()->getParent()->getName();
		std::replace(mapName.begin(), mapName.end(), ' ', '_');
		const std::filesystem::path xml(folder + "/New/" + std::to_string(visibleObject.getWorldId()) + "_" + mapName + ".xml"); // Java: new File(folder + "/New/" + fileName)
		std::vector<SavedSpawnMap> data = std::filesystem::is_regular_file(xml) ? readSpawnFile(xml) : std::vector<SavedSpawnMap>{};
		SavedSpawnMap* spawnMap = nullptr;
		for (SavedSpawnMap& m : data)
			if (m.mapId == visibleObject.getWorldId()) {
				spawnMap = &m;
				break;
			}
		if (spawnMap == nullptr) {
			data.push_back(SavedSpawnMap{visibleObject.getWorldId(), {}});
			spawnMap = &data.back();
		}
		SavedSpawn* oldGroup = findSavedSpawn(*spawnMap, *spawn, delete_); // find in new file
		if (oldGroup == nullptr) {
			if (std::optional<SavedSpawn> loaded = loadSpawnFromTemplateFiles(folder, *spawn, delete_)) { // load from old files
				spawnMap->spawns.push_back(std::move(*loaded));
				oldGroup = &spawnMap->spawns.back();
			}
		}
		if (oldGroup == nullptr) {
			SavedSpawn created;
			created.npcId = spawn->getNpcId();
			created.respawnTime = spawn->getRespawnTime();
			if (std::optional<spawnengine::SpawnHandlerType> handler = spawn->getHandlerType())
				created.handler = std::string(xml::enumName(*handler));
			spawnMap->spawns.push_back(std::move(created));
			oldGroup = &spawnMap->spawns.back();
		}
		oldGroup->custom = true;

		SavedSpot spot; // Java: new SpawnSpotTemplate(x, y, z, heading, randomWalkRange, walkerId, walkerIndex)
		spot.x = visibleObject.getX();
		spot.y = visibleObject.getY();
		spot.z = visibleObject.getZ();
		spot.h = visibleObject.getHeading();
		if (spawn->getRandomWalkRange() > 0)
			spot.randomWalk = spawn->getRandomWalkRange();
		if (std::optional<std::string> walkerId = spawn->getWalkerId())
			spot.walkerId = *walkerId;
		spot.walkerIdx = spawn->getWalkerIndex();
		int32_t oldSpotIndex = -1;
		for (int32_t i = 0; i < static_cast<int32_t>(oldGroup->spots.size()); i++) {
			if (savedPositionMatches(*spawn, oldGroup->spots[static_cast<size_t>(i)])) {
				oldSpotIndex = i;
				break;
			}
		}
		if (oldSpotIndex >= 0) {
			if (delete_)
				oldGroup->spots.erase(oldGroup->spots.begin() + oldSpotIndex);
			else
				oldGroup->spots[static_cast<size_t>(oldSpotIndex)] = spot;
		} else if (!delete_)
			oldGroup->spots.push_back(spot);

		std::error_code ignored;
		std::filesystem::create_directory(xml.parent_path(), ignored); // Java: xml.getParentFile().mkdir() (one level, the result ignored)
		const std::string text = serializeSpawns(data);
		{
			std::ofstream out(xml, std::ios::binary | std::ios::trunc);
			out << text;
			if (!out) {
				// Java: log.error("Could not save XML file!", e) with the IOException of Files.writeString
				commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.dataholders.SpawnsData")
					.error("Could not save XML file!", commons::utils::IOException(xml.string() + " (cannot be written)"));
				return false;
			}
		}
		// update spawn coords at the end, because we need previous coords above to find the old spawn template
		spawn->setX(spot.x);
		spawn->setY(spot.y);
		spawn->setZ(spot.z);
		spawn->setHeading(spot.h);
		// Java: addRegularSpawns(spawnMap) with the JAXB objects of the written map; C++ binds the written file's map as the run-time templates
		// (the bound holder is kept for the life of the process: the spawn groups refer to its spawns, like Java's live objects)
		xml::LoadContext context;
		std::unique_ptr<UnprocessedSpawns> bound = xml::bindString<UnprocessedSpawns>(context, text, xml.string());
		for (const SpawnMap& map : bound->getTemplates())
			if (map.getMapId() == visibleObject.getWorldId())
				addRegularSpawns(map);
		static_cast<void>(bound.release()); // immortal, like a published holder
		return true;
	}
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
