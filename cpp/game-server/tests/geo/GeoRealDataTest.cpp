// P4-04 on the real geo data (handlers-and-porting-plan.md §2.7 M4 item 5, §3.2): GeoWorldLoader loads data/geo for the 161 maps of
// world_maps.xml, and the counts, 200 getZ probes and the material zone names (count, distinct count, a digest of the full sorted list and 50
// sampled names) are compared with the independent Python oracle (tools/oracle/geo, expected/geo_expected.json): entity counts, getZ by brute
// force over the triangles, zone names from the Java rules. Skipped when the Java tree's data directory is missing. The load runs once per test
// process (a static fixture).
// Test doubles: the event theme is EventTheme.NONE (id 0, the oracle's theme_id) and no siege location exists (the loader creates no SHIELD
// node), so the probes do not depend on the EventService and SiegeService singletons.

#include <gtest/gtest.h>

#include <algorithm>
#include <bit>
#include <cmath>
#include <iostream>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <regex>
#include <set>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include <nlohmann/json.hpp>

#include "GeoTestSupport.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/MaterialData.bind.h"
#include "aion/gameserver/dataholders/MaterialData.h"
#include "aion/gameserver/dataholders/WorldMapsData.bind.h"
#include "aion/gameserver/dataholders/WorldMapsData.h"
#include "aion/gameserver/dataholders/loadingutils/LoadContext.h"
#include "aion/gameserver/dataholders/loadingutils/StaticDataLoader.h"
#include "aion/gameserver/geoEngine/GeoCallbacks.h"
#include "aion/gameserver/geoEngine/GeoWorldLoader.h"
#include "aion/gameserver/geoEngine/models/GeoMap.h"
#include "aion/gameserver/geoEngine/scene/Spatial.h"

namespace aion::gameserver::geoEngine::test {
namespace {

const std::filesystem::path JAVA_DIR = std::filesystem::path(AION_GAMESERVER_JAVA_DIR);
const std::filesystem::path GEO_DIR = JAVA_DIR / "data/geo";
const std::filesystem::path WORLD_MAPS = JAVA_DIR / "data/static_data/world_maps.xml";
const std::filesystem::path MATERIALS = JAVA_DIR / "data/static_data/mesh_materials/material_templates.xml";
const std::filesystem::path EXPECTED = JAVA_DIR / "../cpp/tools/oracle/expected/geo_expected.json";

std::string readText(const std::filesystem::path& path) {
	std::ifstream in(path, std::ios::binary);
	return std::string((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
}

std::mutex zoneMutex;
std::vector<std::string> zoneNames;

/** 64-bit FNV-1a (tools/oracle/geo/run.py fnv1a64) */
uint64_t fnv1a64(std::string_view data) {
	uint64_t value = 0xCBF29CE484222325ull;
	for (unsigned char byte : data)
		value = (value ^ byte) * 0x100000001B3ull;
	return value;
}

std::string hex16(uint64_t value) {
	constexpr std::string_view digits = "0123456789abcdef";
	std::string text(16, '0');
	for (int i = 15; i >= 0; i--, value >>= 4)
		text[static_cast<size_t>(i)] = digits[static_cast<size_t>(value & 0xF)];
	return text;
}

void collectZone(scene::Spatial&, int32_t, std::string_view zoneName) {
	std::lock_guard lock(zoneMutex);
	zoneNames.emplace_back(zoneName);
}

struct LoadedGeo {
	std::vector<runtime::Ref<models::GeoMap>> maps;
	std::map<int32_t, size_t> byId; // index into maps (a Ptr is valid only in the scope that borrowed it)
	GeoWorldLoader::Statistics statistics;
	std::vector<std::string> zoneNames;
	double seconds = 0;
};

class GeoRealDataTest : public ::testing::Test {
protected:
	static std::unique_ptr<LoadedGeo> loaded;
	static nlohmann::json expected;
	runtime::TaskScope scope{AION_TASK_INFO(runtime::TaskKind::TEST)};

	static void SetUpTestSuite() {
		if (!std::filesystem::is_directory(GEO_DIR) || !std::filesystem::is_regular_file(EXPECTED))
			return;
		runtime::TaskScope setupScope(AION_TASK_INFO(runtime::TaskKind::TEST));
		expected = nlohmann::json::parse(readText(EXPECTED));
		std::string worldMapsXml = readText(WORLD_MAPS);
		xml::LoadContext context;
		dataholders::DataManager::WORLD_MAPS_DATA.publish(xml::bindString<dataholders::WorldMapsData>(context, worldMapsXml));
		dataholders::DataManager::MATERIAL_DATA.publish(xml::bindString<dataholders::MaterialData>(context, readText(MATERIALS)));

		auto result = std::make_unique<LoadedGeo>();
		std::regex mapId(R"re(<map\s[^>]*?\bid="(\d+)")re");
		for (std::sregex_iterator it(worldMapsXml.begin(), worldMapsXml.end(), mapId), end; it != end; ++it)
			result->maps.push_back(models::GeoMap::create(std::stoi((*it)[1].str())));
		std::vector<runtime::Ptr<models::GeoMap>> maps;
		for (const runtime::Ref<models::GeoMap>& map : result->maps) {
			result->byId[map->getMapId()] = maps.size();
			maps.emplace_back(map);
		}
		zoneNames.clear();
		GeoCallbacks::setMaterialZoneSink(&collectZone);
		GeoCallbacks::setEventThemeIdSupplier(+[] { return 0; }); // the oracle's theme_id = 0 (EventTheme.NONE)
		GeoCallbacks::setSiegeShieldLookup(+[](int32_t) -> std::optional<GeoCallbacks::SiegeShieldState> { return std::nullopt; });
		auto start = std::chrono::steady_clock::now();
		result->statistics = GeoWorldLoader::load(maps, GEO_DIR);
		result->seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
		GeoCallbacks::setMaterialZoneSink(nullptr);
		result->zoneNames = zoneNames;
		loaded = std::move(result);
	}

	static void TearDownTestSuite() {
		loaded.reset();
		GeoCallbacks::setEventThemeIdSupplier(nullptr);
		GeoCallbacks::setSiegeShieldLookup(nullptr);
		dataholders::DataManager::WORLD_MAPS_DATA.resetForTests();
		dataholders::DataManager::MATERIAL_DATA.resetForTests();
	}

	void SetUp() override {
		if (!loaded)
			GTEST_SKIP() << "no geo data at " << GEO_DIR.string() << " or no " << EXPECTED.string();
	}
};

std::unique_ptr<LoadedGeo> GeoRealDataTest::loaded;
nlohmann::json GeoRealDataTest::expected;

TEST_F(GeoRealDataTest, EntityCountsEqualTheOracle) {
	const nlohmann::json& counts = expected["counts"];
	const GeoWorldLoader::Statistics& s = loaded->statistics;
	std::cout << "geo load: " << loaded->seconds << " s" << std::endl;
	EXPECT_EQ(s.meshEntries, counts["meshEntries"].get<int32_t>());
	EXPECT_EQ(s.meshes, counts["meshes"].get<int32_t>());
	EXPECT_EQ(s.meshNames, counts["meshNames"].get<int32_t>());
	EXPECT_EQ(s.geoFiles, counts["geoFiles"].get<int32_t>());
	EXPECT_EQ(s.placements, counts["placements"].get<int32_t>());
	EXPECT_EQ(s.missingMeshPlacements, counts["missingMeshPlacements"].get<int32_t>());
	EXPECT_EQ(s.placementGeometries, counts["placementGeometries"].get<int32_t>());
	EXPECT_EQ(s.attachedNodes, counts["attachedNodes"].get<int32_t>());
	EXPECT_EQ(s.geometries, counts["geometries"].get<int32_t>());
	EXPECT_EQ(s.materialGeometries, counts["materialGeometries"].get<int32_t>());
	EXPECT_EQ(s.terrainMaps, counts["terrainMaps"].get<int32_t>());
	EXPECT_EQ(static_cast<int32_t>(loaded->maps.size()), counts["worldMaps"].get<int32_t>());

	// M4 item 5 anchors (handlers-and-porting-plan.md §2.7)
	EXPECT_EQ(s.meshEntries, 18583);
	EXPECT_EQ(s.meshes, 25437);
	EXPECT_EQ(s.geoFiles, 151);
	EXPECT_EQ(s.placements, 419707);
	EXPECT_EQ(s.placementGeometries, 484111);
	EXPECT_EQ(s.materialGeometries, 7961);

	int64_t entities = 0;
	int32_t mapsWithEntities = 0;
	for (const runtime::Ref<models::GeoMap>& map : loaded->maps) {
		entities += map->getEntityCount();
		mapsWithEntities += map->getEntityCount() > 0 ? 1 : 0;
	}
	EXPECT_EQ(entities, s.attachedNodes);
	EXPECT_EQ(mapsWithEntities, counts["mapsWithEntities"].get<int32_t>());
}

TEST_F(GeoRealDataTest, MaterialZoneNamesEqualTheOracle) {
	const nlohmann::json& zones = expected["zoneNames"];
	EXPECT_EQ(static_cast<int32_t>(loaded->zoneNames.size()), zones["count"].get<int32_t>());
	std::set<std::string> distinct(loaded->zoneNames.begin(), loaded->zoneNames.end());
	EXPECT_EQ(static_cast<int32_t>(distinct.size()), zones["distinct"].get<int32_t>());
	// the whole list: sorted by code point (UTF-8 byte order, which std::string compares) and joined with LF, like run.py zone_names_digest
	std::vector<std::string> sorted = loaded->zoneNames;
	std::sort(sorted.begin(), sorted.end());
	std::string joined;
	for (size_t i = 0; i < sorted.size(); i++) {
		if (i > 0)
			joined += '\n';
		joined += sorted[i];
	}
	EXPECT_EQ(hex16(fnv1a64(joined)), zones["fnv1a64"].get<std::string>()) << "a subset of the zone names differs from the oracle";
	int32_t checked = 0;
	for (const nlohmann::json& name : zones["sample"]) {
		EXPECT_TRUE(distinct.contains(name.get<std::string>())) << name.get<std::string>();
		checked++;
	}
	EXPECT_EQ(checked, 50);
}

TEST_F(GeoRealDataTest, GetZProbesEqualTheOracle) {
	int32_t checked = 0;
	int32_t hits = 0;
	for (const nlohmann::json& probe : expected["probes"]) {
		auto value = [&](const char* key) { return std::bit_cast<float>(probe[key]["bits"].get<uint32_t>()); };
		int32_t mapId = probe["map"].get<int32_t>();
		ASSERT_TRUE(loaded->byId.contains(mapId)) << mapId;
		float x = value("x"), y = value("y"), zMax = value("zMax"), zMin = value("zMin");
		float z = loaded->maps[loaded->byId[mapId]]->getZ(x, y, zMax, zMin, probe["instanceId"].get<int32_t>());
		EXPECT_TRUE(sameFloat(z, value("z"))) << "map " << mapId << " getZ(" << x << ", " << y << ", " << zMax << ", " << zMin << ") "
											   << probe["kind"].get<std::string>() << "/" << probe["source"].get<std::string>();
		checked++;
		hits += std::isnan(z) ? 0 : 1;
	}
	EXPECT_EQ(checked, 200);
	EXPECT_GT(hits, 150);
}

} // namespace
} // namespace aion::gameserver::geoEngine::test
