// World chunk (P4-10) on the real static data (realdata; run as its own process, e.g. by ctest): the holders World and ZoneService read (world
// maps, zones, siege locations, vortex locations, shields, material templates) are bound through the xml runtime with hooks and published,
// then ZoneService creates the zone instances of every map and World creates all world maps with their instances and map regions.
// Expectations: the count oracle (tools/oracle/expected/static_data_counts.json: 161 maps) and, recomputed from the zone XML with pugixml
// (independent of the binder), per map the zone count (distinct zone names of the map with an area, plus the whole map zone), the zone
// instance class of every zone by its zone_type (ZoneService.java:104-142: FLY, NO_FLY, FORT/ARTIFACT, PVP, the 10 invasion zones of maps with
// a vortex location, otherwise ZoneInstance), and the zones every siege, artifact and vortex location received (one per zone naming the
// location, for every getZoneInstancesByWorldId call of the zone's map: the test's own pass plus one per map instance World creates).

#include <gtest/gtest.h>

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <memory>
#include <set>
#include <sstream>
#include <string>
#include <string_view>
#include <typeinfo>
#include <unordered_map>
#include <vector>

#include <nlohmann/json.hpp>
#include <pugixml.hpp>

#include "WorldTestSupport.h"

#include "aion/commons/utils/StringUtils.h"
#include "aion/gameserver/dataholders/SiegeLocationData.bind.h"
#include "aion/gameserver/dataholders/SiegeLocationData.h"
#include "aion/gameserver/dataholders/VortexData.bind.h"
#include "aion/gameserver/dataholders/VortexData.h"
#include "aion/gameserver/dataholders/loadingutils/BindContext.h"
#include "aion/gameserver/dataholders/loadingutils/StaticDataImports.h"
#include "aion/gameserver/dataholders/loadingutils/XmlDocument.h"
#include "aion/gameserver/model/templates/world/WorldMapTemplate.h"
#include "aion/gameserver/runtime/base/TaskInfo.h"
#include "aion/gameserver/runtime/lifetime/TaskScope.h"
#include "aion/gameserver/world/MapRegion.h"
#include "aion/gameserver/world/World.h"
#include "aion/gameserver/world/WorldMap.h"
#include "aion/gameserver/world/WorldMapInstance.h"
#include "aion/gameserver/model/siege/ArtifactLocation.h"
#include "aion/gameserver/model/siege/SiegeLocation.h"
#include "aion/gameserver/model/vortex/VortexLocation.h"
#include "aion/gameserver/world/zone/FlyZoneInstance.h"
#include "aion/gameserver/world/zone/InvasionZoneInstance.h"
#include "aion/gameserver/world/zone/NoFlyZoneInstance.h"
#include "aion/gameserver/world/zone/PvPZoneInstance.h"
#include "aion/gameserver/world/zone/SiegeZoneInstance.h"
#include "aion/gameserver/world/zone/ZoneInstance.h"
#include "aion/gameserver/world/zone/ZoneName.h"
#include "aion/gameserver/world/zone/ZoneService.h"

namespace aion::gameserver::world::test {
namespace {

using json = nlohmann::json;

const std::filesystem::path STATIC_DATA = std::filesystem::path(AION_GAMESERVER_JAVA_DIR) / "data/static_data";
const std::filesystem::path ORACLE = std::filesystem::path(AION_GAMESERVER_JAVA_DIR) / "../cpp/tools/oracle/expected";

json readJson(const std::filesystem::path& file) {
	std::ifstream in(file, std::ios::binary);
	std::stringstream text;
	text << in.rdbuf();
	return json::parse(text.str());
}

const xml::StaticDataImport& findImport(const std::vector<xml::StaticDataImport>& imports, std::string_view file) {
	for (const xml::StaticDataImport& entry : imports) {
		if (entry.file == file)
			return entry;
	}
	throw std::runtime_error("no import " + std::string(file));
}

/** parses and binds the files of an import into a new holder, strictly and with hooks (the loader's bindHolder path) */
template <class H>
std::unique_ptr<H> bindImport(xml::LoadContext& context, const std::vector<std::unique_ptr<xml::XmlDocument>>& documents) {
	std::vector<const xml::XmlDocument*> roots;
	for (const std::unique_ptr<xml::XmlDocument>& document : documents)
		roots.push_back(document.get());
	auto holder = std::make_unique<H>();
	xml::BindContext binding(context);
	binding.bindHolder(*holder, roots, context.root());
	return holder;
}

int64_t expectedCount(const json& counts, std::string_view holder) {
	for (const json& line : counts.at("lines")) {
		const json& holders = line.at("holders");
		if (holders.size() == 1 && holders[0].get<std::string>() == holder)
			return line.at("values").at(0).get<int64_t>();
	}
	throw std::runtime_error("no count line for " + std::string(holder));
}

TEST(WorldRealDataTest, WorldCreatesAllMapsAndZoneServiceTheirZoneInstances) {
	if (!std::filesystem::exists(STATIC_DATA / "static_data.xml") || !std::filesystem::exists(ORACLE / "static_data_counts.json"))
		GTEST_SKIP() << "Java data tree or oracle outputs not found: " << STATIC_DATA << ", " << ORACLE;
	{
		std::scoped_lock lock(publishedDataMutex);
		if (publishedData != PublishedData::NONE)
			GTEST_SKIP() << "this process published the test static data (run the test on its own)";
		publishedData = PublishedData::REAL;
	}
	// WorldConfig defaults (config/main/world.properties does not override them)
	configs::main::WorldConfig::WORLD_REGION_SIZE.store(128);
	configs::main::WorldConfig::WORLD_MAX_TWINS_USUAL.store(1);
	configs::main::WorldConfig::WORLD_MAX_TWINS_BEGINNER.store(-1);

	runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
	json counts = readJson(ORACLE / "static_data_counts.json");
	std::vector<xml::StaticDataImport> imports = xml::StaticDataImports::resolve(STATIC_DATA / "static_data.xml", 0, true);
	auto start = std::chrono::steady_clock::now();
	xml::LoadContext context;
	auto parse = [&imports](std::string_view file) { return xml::StaticDataLoader::parseFiles(findImport(imports, file).files, false); };
	dataholders::DataManager::WORLD_MAPS_DATA.publish(bindImport<dataholders::WorldMapsData>(context, parse("world_maps.xml")));
	dataholders::DataManager::MATERIAL_DATA.publish(bindImport<dataholders::MaterialData>(context, parse("mesh_materials/material_templates.xml")));
	dataholders::DataManager::SHIELD_DATA.publish(bindImport<dataholders::ShieldData>(context, parse("siege/siege_shields.xml")));
	dataholders::DataManager::SIEGE_LOCATION_DATA.publish(bindImport<dataholders::SiegeLocationData>(context, parse("siege/siege_locations.xml")));
	dataholders::DataManager::VORTEX_DATA.publish(bindImport<dataholders::VortexData>(context, parse("vortex/dimensional_vortex.xml")));
	std::vector<std::unique_ptr<xml::XmlDocument>> zoneDocuments = parse("zones");
	dataholders::DataManager::ZONE_DATA.publish(bindImport<dataholders::ZoneData>(context, zoneDocuments));
	std::cout << "holders bound in " << std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count()
			  << " ms\n";
	ASSERT_EQ(dataholders::DataManager::WORLD_MAPS_DATA->size(), expectedCount(counts, "world_maps"));
	ASSERT_EQ(dataholders::DataManager::ZONE_DATA->size(), expectedCount(counts, "zones"));

	// per map: the distinct names of the zones with an area (a SPHERE with r <= 0 has none, ZoneData.afterUnmarshal) and their XML attributes
	struct XmlZone {
		int32_t mapId;
		std::string type;
		std::vector<int32_t> siegeIds;
	};
	std::map<int32_t, std::set<std::string>> zoneNamesByMap;
	std::map<std::string, XmlZone> xmlZones; // by upper case name (no name is used with two zone types)
	for (const std::unique_ptr<xml::XmlDocument>& document : zoneDocuments) {
		for (pugi::xml_node zone : document->root().children("zone")) {
			if (std::string_view(zone.attribute("area_type").as_string()) == "SPHERE" && zone.child("sphere").attribute("r").as_float() <= 0)
				continue;
			std::string name = commons::utils::StringUtils::toUpperCase(zone.attribute("name").as_string());
			zoneNamesByMap[zone.attribute("mapid").as_int()].insert(name);
			XmlZone& entry = xmlZones[name];
			entry.mapId = zone.attribute("mapid").as_int();
			entry.type = zone.attribute("zone_type").as_string();
			entry.siegeIds.clear();
			std::istringstream ids(zone.attribute("siege_id").as_string());
			for (int32_t id; ids >> id;)
				entry.siegeIds.push_back(id);
		}
	}
	// ZoneService.getIZI: the invasion zones, InvasionZoneInstances only on maps with a vortex location
	const std::set<std::string> invasionZoneNames = {"WAILING_CLIFFS_220050000", "BALTASAR_CEMETERY_220050000", "THE_LEGEND_SHRINE_220050000",
		"SUDORVILLE_220050000", "BALTASAR_HILL_VILLAGE_220050000", "BRUSTHONIN_MITHRIL_MINE_220050000", "JAMANOK_INN_210060000",
		"THE_STALKING_GROUNDS_210060000", "BLACK_ROCK_HOT_SPRING_210060000", "FREGIONS_FLAME_210060000"};

	start = std::chrono::steady_clock::now();
	size_t zoneInstances = 0;
	std::map<std::string, int32_t> instancesByClass;
	for (const model::templates::world::WorldMapTemplate* map : *dataholders::DataManager::WORLD_MAPS_DATA) {
		std::unordered_map<const zone::ZoneName*, runtime::Ref<zone::ZoneInstance>> zones =
			zone::ZoneService::getInstance().getZoneInstancesByWorldId(map->getMapId());
		EXPECT_EQ(zones.size(), zoneNamesByMap[map->getMapId()].size() + 1) << "map " << map->getMapId();
		zoneInstances += zones.size();
		for (const auto& [zoneName, instance] : zones) {
			auto xmlZone = xmlZones.find(zoneName->name());
			const std::type_info& actual = typeid(*instance);
			if (xmlZone == xmlZones.end()) { // the whole map zone
				EXPECT_EQ(actual, typeid(zone::ZoneInstance)) << zoneName->name();
				continue;
			}
			ASSERT_EQ(xmlZone->second.mapId, map->getMapId()) << zoneName->name();
			const std::string& type = xmlZone->second.type;
			if (type == "FLY")
				EXPECT_EQ(actual, typeid(zone::FlyZoneInstance)) << zoneName->name();
			else if (type == "NO_FLY")
				EXPECT_EQ(actual, typeid(zone::NoFlyZoneInstance)) << zoneName->name();
			else if (type == "FORT" || type == "ARTIFACT")
				EXPECT_EQ(actual, typeid(zone::SiegeZoneInstance)) << zoneName->name();
			else if (type == "PVP")
				EXPECT_EQ(actual, typeid(zone::PvPZoneInstance)) << zoneName->name();
			else if (invasionZoneNames.contains(zoneName->name()) && dataholders::DataManager::VORTEX_DATA->getVortexLocation(map->getMapId()))
				EXPECT_EQ(actual, typeid(zone::InvasionZoneInstance)) << zoneName->name();
			else
				EXPECT_EQ(actual, typeid(zone::ZoneInstance)) << zoneName->name() << " " << type;
			instancesByClass[type]++;
		}
	}
	EXPECT_EQ(instancesByClass["FORT"], 20) << "zone_type counts of the zone XML";
	EXPECT_EQ(instancesByClass["ARTIFACT"], 18);
	EXPECT_EQ(instancesByClass["FLY"], 49);
	std::cout << zoneInstances << " zone instances of " << dataholders::DataManager::WORLD_MAPS_DATA->size() << " maps created in "
			  << std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count() << " ms\n";

	start = std::chrono::steady_clock::now();
	World& world = World::getInstance();
	std::cout << "World created in " << std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count()
			  << " ms\n";
	int32_t maps = 0;
	int32_t instances = 0;
	for (const model::templates::world::WorldMapTemplate* map : *dataholders::DataManager::WORLD_MAPS_DATA) {
		runtime::Ptr<WorldMap> worldMap = world.getWorldMap(map->getMapId());
		ASSERT_TRUE(worldMap) << "map " << map->getMapId();
		maps++;
		EXPECT_EQ(static_cast<int32_t>(worldMap->getAvailableInstanceIds().size()), worldMap->getInstanceCount()) << "map " << map->getMapId();
		for (runtime::Ptr<WorldMapInstance> instance : *worldMap) {
			instances++;
			runtime::Ptr<MapRegion> origin = instance->getRegion(1, 1, 1);
			ASSERT_TRUE(origin) << instance->toString();
			// every region holds the whole map zone (filterZones logs an error otherwise)
			EXPECT_GE(origin->getZoneCount(), 1) << instance->toString();
			runtime::Ptr<MapRegion> far = instance->getRegion(static_cast<float>(map->getWorldSize()), static_cast<float>(map->getWorldSize()), 1);
			EXPECT_TRUE(far) << instance->toString() << ": the last region of the map";
		}
	}
	EXPECT_EQ(maps, expectedCount(counts, "world_maps")) << "World: 161 world maps created.";
	EXPECT_GE(instances, maps);
	std::cout << maps << " world maps with " << instances << " instances\n";

	// the zones of the siege, artifact and vortex locations: ZoneService adds one zone (and its location handler) per zone naming the location
	// for every getZoneInstancesByWorldId call of the zone's map (the pass above plus one per instance of the map)
	auto callsOfMap = [&world](int32_t mapId) {
		int32_t calls = 1;
		for ([[maybe_unused]] runtime::Ptr<WorldMapInstance> instance : *world.getWorldMap(mapId))
			calls++;
		return calls;
	};
	const auto& siegeLocations = dataholders::DataManager::SIEGE_LOCATION_DATA->getSiegeLocations();
	const auto& artifacts = dataholders::DataManager::SIEGE_LOCATION_DATA->getArtifacts();
	std::map<model::siege::SiegeLocation*, int32_t> expectedSiegeZones;
	for (const auto& [id, location] : siegeLocations)
		expectedSiegeZones[location.get()] = 0;
	for (const auto& [id, artifact] : artifacts)
		expectedSiegeZones[artifact.get()] = 0;
	std::map<model::vortex::VortexLocation*, int32_t> expectedVortexZones;
	for (const auto& [id, vortex] : dataholders::DataManager::VORTEX_DATA->getVortexLocations())
		expectedVortexZones[vortex.get()] = 0;
	for (const auto& [name, xmlZone] : xmlZones) {
		if (xmlZone.type == "FORT") {
			if (const runtime::Ref<model::siege::SiegeLocation>* siege = siegeLocations.get(xmlZone.siegeIds.at(0)))
				expectedSiegeZones[siege->get()] += callsOfMap(xmlZone.mapId);
		} else if (xmlZone.type == "ARTIFACT") {
			for (int32_t artifactId : xmlZone.siegeIds) {
				if (const runtime::Ref<model::siege::ArtifactLocation>* artifact = artifacts.get(artifactId))
					expectedSiegeZones[artifact->get()] += callsOfMap(xmlZone.mapId);
			}
		} else if (invasionZoneNames.contains(name)) {
			if (runtime::Ptr<model::vortex::VortexLocation> vortex = dataholders::DataManager::VORTEX_DATA->getVortexLocation(xmlZone.mapId))
				expectedVortexZones[vortex.get()] += callsOfMap(xmlZone.mapId);
		}
	}
	int32_t locationsWithZones = 0;
	for (const auto& [location, expected] : expectedSiegeZones) {
		EXPECT_EQ(location->getZone().size(), expected) << "siege location " << location->getLocationId();
		locationsWithZones += expected > 0 ? 1 : 0;
	}
	EXPECT_GT(locationsWithZones, 0);
	int32_t vortexZones = 0;
	for (const auto& [vortex, expected] : expectedVortexZones) {
		EXPECT_EQ(vortex->getZones().size(), expected) << "vortex location " << vortex->getId();
		vortexZones += expected;
	}
	EXPECT_GT(vortexZones, 0) << "the invasion zones of the vortex maps";
}

} // namespace
} // namespace aion::gameserver::world::test
