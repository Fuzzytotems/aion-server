// P4-04 GeoWorldLoader on generated files (GeoWorldLoader.java): big endian models.mesh with byte and short indices and an `a|b` name, a
// .geo file with a plain placement, material geometries, a town object with higher town levels, door states and a missing model, terrain
// PNGs (a heightmap, a materials image, a heightmap shared by two maps, an image without a map), a map without .geo file (with and without
// the missing file warning) and the load errors. The files are written by this test (PNG data in stored deflate blocks). Expected zone names
// use the vector hashes 260974 = hash(45, 45, 1) and 418423 = hash(41, 41, 1), computed with tools/oracle/geo (jgeo.vector_hash).

#include <gtest/gtest.h>

#include <algorithm>
#include <bit>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <limits>
#include <tuple>
#include <mutex>
#include <random>
#include <string>
#include <string_view>
#include <vector>

#include "GeoTestSupport.h"
#include "aion/commons/utils/Exception.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/MaterialData.bind.h"
#include "aion/gameserver/dataholders/MaterialData.h"
#include "aion/gameserver/dataholders/WorldMapsData.bind.h"
#include "aion/gameserver/dataholders/WorldMapsData.h"
#include "aion/gameserver/dataholders/loadingutils/LoadContext.h"
#include "aion/gameserver/dataholders/loadingutils/StaticDataLoader.h"
#include "aion/gameserver/geoEngine/GeoCallbacks.h"
#include "aion/gameserver/geoEngine/GeoWorldLoader.h"
#include "aion/gameserver/geoEngine/collision/CollisionResults.h"
#include "aion/gameserver/geoEngine/models/GeoMap.h"
#include "aion/gameserver/geoEngine/scene/DespawnableNode.h"
#include "aion/gameserver/geoEngine/scene/Geometry.h"
#include "aion/gameserver/geoEngine/scene/Mesh.h"

namespace aion::gameserver::geoEngine::test {
namespace {

constexpr float NaN = std::numeric_limits<float>::quiet_NaN();

// ---- file writers ---------------------------------------------------------------------------------------------------------------------------

class BigEndianWriter {
public:
	std::vector<uint8_t> bytes;

	BigEndianWriter& u8(uint8_t value) {
		bytes.push_back(value);
		return *this;
	}
	BigEndianWriter& i16(int16_t value) {
		bytes.push_back(static_cast<uint8_t>(static_cast<uint16_t>(value) >> 8));
		bytes.push_back(static_cast<uint8_t>(value));
		return *this;
	}
	BigEndianWriter& u32(uint32_t value) {
		for (int shift = 24; shift >= 0; shift -= 8)
			bytes.push_back(static_cast<uint8_t>(value >> shift));
		return *this;
	}
	BigEndianWriter& f32(float value) { return u32(std::bit_cast<uint32_t>(value)); }
	BigEndianWriter& name(std::string_view text) {
		i16(static_cast<int16_t>(text.size()));
		bytes.insert(bytes.end(), text.begin(), text.end());
		return *this;
	}
};

struct Model {
	std::vector<float> vertices;
	std::vector<int32_t> indices;
	int8_t indexSize;
	int8_t materialId;
	int8_t intentions;
};

void writeMeshEntry(BigEndianWriter& out, std::string_view name, const std::vector<Model>& models) {
	out.name(name).u8(static_cast<uint8_t>(models.size()));
	for (const Model& model : models) {
		out.i16(static_cast<int16_t>(model.vertices.size() / 3));
		for (float value : model.vertices)
			out.f32(value);
		out.i16(static_cast<int16_t>(model.indices.size() / 3)).u8(static_cast<uint8_t>(model.indexSize));
		for (int32_t index : model.indices) {
			if (model.indexSize == 2)
				out.i16(static_cast<int16_t>(index));
			else
				out.u8(static_cast<uint8_t>(index));
		}
		out.u8(static_cast<uint8_t>(model.materialId)).u8(static_cast<uint8_t>(model.intentions));
	}
}

void writePlacement(BigEndianWriter& out, std::string_view name, float x, float y, float z, int8_t type = 0, int16_t id = 0, int8_t level = 0) {
	out.name(name).f32(x).f32(y).f32(z);
	for (int i = 0; i < 3; i++)
		for (int j = 0; j < 3; j++)
			out.f32(i == j ? 1.0f : 0.0f);
	out.f32(1).f32(1).f32(1).u8(static_cast<uint8_t>(type)).i16(id).u8(static_cast<uint8_t>(level));
}

/** a PNG (one sample per pixel, 8 or 16 bits) with its image data in stored deflate blocks; the chunk CRCs are not checked by the reader */
std::vector<uint8_t> storedPng(int32_t width, int32_t height, int32_t bitDepth, const std::vector<uint16_t>& samples) {
	std::vector<uint8_t> raw;
	for (int32_t y = 0; y < height; y++) {
		raw.push_back(0); // filter: none
		for (int32_t x = 0; x < width; x++) {
			uint16_t value = samples[static_cast<size_t>(y * width + x)];
			if (bitDepth == 16)
				raw.push_back(static_cast<uint8_t>(value >> 8));
			raw.push_back(static_cast<uint8_t>(value));
		}
	}
	BigEndianWriter zlib;
	zlib.u8(0x78).u8(0x01).u8(0x01); // zlib header, final stored block
	zlib.u8(static_cast<uint8_t>(raw.size())).u8(static_cast<uint8_t>(raw.size() >> 8));
	zlib.u8(static_cast<uint8_t>(~raw.size())).u8(static_cast<uint8_t>(~raw.size() >> 8));
	zlib.bytes.insert(zlib.bytes.end(), raw.begin(), raw.end());
	uint32_t a = 1, b = 0;
	for (uint8_t value : raw) {
		a = (a + value) % 65521;
		b = (b + a) % 65521;
	}
	zlib.u32((b << 16) | a);
	BigEndianWriter png;
	png.bytes = {0x89, 'P', 'N', 'G', '\r', '\n', 0x1A, '\n'};
	png.u32(13).u8('I').u8('H').u8('D').u8('R').u32(static_cast<uint32_t>(width)).u32(static_cast<uint32_t>(height));
	png.u8(static_cast<uint8_t>(bitDepth)).u8(0).u8(0).u8(0).u8(0).u32(0);
	png.u32(static_cast<uint32_t>(zlib.bytes.size())).u8('I').u8('D').u8('A').u8('T');
	png.bytes.insert(png.bytes.end(), zlib.bytes.begin(), zlib.bytes.end());
	png.u32(0).u32(0).u8('I').u8('E').u8('N').u8('D').u32(0);
	return png.bytes;
}

void writeFile(const std::filesystem::path& path, const std::vector<uint8_t>& bytes) {
	std::ofstream out(path, std::ios::binary | std::ios::trunc);
	out.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
}

const Model QUAD{{0, 0, 0, 10, 0, 0, 0, 10, 0, 10, 10, 0}, {0, 1, 2, 1, 3, 2}, 1, 0, 1};
const Model FIRE_QUAD{{0, 0, 0, 10, 0, 0, 0, 10, 0, 10, 10, 0}, {0, 1, 2, 1, 3, 2}, 1, 11, 3};
const Model FIRE_TRIANGLE{{0, 0, 0, 2, 0, 0, 0, 2, 0}, {0, 1, 2}, 2, 11, 3};
const Model DOOR{{0, 0, 0, 1, 0, 0, 0, 0, 3}, {0, 1, 2}, 2, 0, 17};

// ---- material zone sink ---------------------------------------------------------------------------------------------------------------------

std::mutex zoneMutex;
std::vector<std::string> zoneNames;

void collectZone(scene::Spatial& geometry, int32_t worldId, std::string_view zoneName) {
	std::lock_guard lock(zoneMutex);
	zoneNames.push_back(std::string(zoneName) + "|" + geometry.getName() + "|" + std::to_string(worldId));
}

class GeoWorldLoaderFilesTest : public ::testing::Test {
protected:
	runtime::TaskScope scope{AION_TASK_INFO(runtime::TaskKind::TEST)};
	// unique per test process: ctest runs the tests of this fixture in parallel processes, and SetUp/TearDown remove the directory
	std::filesystem::path dir = std::filesystem::temp_directory_path() / ("aion_gs_geo_loader_files_test_" + std::to_string(std::random_device()()));

	void SetUp() override {
		std::filesystem::remove_all(dir);
		std::filesystem::create_directories(dir);
		zoneNames.clear();
		GeoCallbacks::setMaterialZoneSink(&collectZone);
		xml::LoadContext context;
		dataholders::DataManager::WORLD_MAPS_DATA.publish(xml::bindString<dataholders::WorldMapsData>(context, R"(<world_maps>)"
			R"(<map id="110010000" cName="A" death_level="0" water_level="0" world_size="1024" flags="RECALL"/>)"
			R"(<map id="120010000" cName="B" death_level="0" water_level="0" world_size="1024" flags="RECALL"/>)"
			R"(<map id="130090000" cName="C" death_level="0" water_level="0" world_size="1024" prison="true" flags="RECALL"/>)"
			R"(</world_maps>)"));
		// the models use materials 0 and 11, which have no material template (GeoWorldLoader.loadMeshes names nodes by the material data)
		dataholders::DataManager::MATERIAL_DATA.publish(xml::bindString<dataholders::MaterialData>(context,
			R"(<material_templates><material id="12"><skill id="8269" level="1" target="PLAYER" frequency="3"/></material></material_templates>)"));
	}

	void TearDown() override {
		dataholders::DataManager::WORLD_MAPS_DATA.resetForTests();
		dataholders::DataManager::MATERIAL_DATA.resetForTests();
		GeoCallbacks::setMaterialZoneSink(nullptr);
		std::error_code ignored;
		std::filesystem::remove_all(dir, ignored);
	}

	void writeMeshes(const std::vector<std::pair<std::string, std::vector<Model>>>& entries) {
		BigEndianWriter out;
		for (const auto& [name, models] : entries)
			writeMeshEntry(out, name, models);
		writeFile(dir / "models.mesh", out.bytes);
	}

	std::vector<runtime::Ref<models::GeoMap>> maps{models::GeoMap::create(110010000), models::GeoMap::create(120010000),
		models::GeoMap::create(130090000)};

	std::vector<runtime::Ptr<models::GeoMap>> mapPtrs() const { return {maps[0], maps[1], maps[2]}; }
};

TEST_F(GeoWorldLoaderFilesTest, LoadsTerrainsMeshesAndPlacements) {
	writeMeshes({{"levels/common/rock_a.cgf", {QUAD}},
		{"levels/common/fire_a.cgf|levels/common/fire_b.cgf", {FIRE_QUAD, FIRE_TRIANGLE}},
		{"town/house_01.cgf", {QUAD}},
		{"town/house_03.cgf", {QUAD}},
		{"door/door_a.cgf", {DOOR}}});
	BigEndianWriter geo;
	writePlacement(geo, "levels/common/rock_a.cgf", 20, 20, 5);
	writePlacement(geo, "levels/common/fire_b.cgf", 40, 40, 1);
	writePlacement(geo, "town/house_01.cgf", 100, 100, 0, 5, 7, 1);
	writePlacement(geo, "door/door_a.cgf", 60, 60, 0, 6, 5);
	writePlacement(geo, "door/door_a.cgf", 60, 60, 0, 7, 5);
	writePlacement(geo, "missing/model.cgf", 0, 0, 0);
	writeFile(dir / "110010000.geo", geo.bytes);
	writeFile(dir / "110010000.png", storedPng(4, 4, 16, std::vector<uint16_t>(16, 64)));                        // z 2
	writeFile(dir / "110010000_materials.png", storedPng(4, 4, 8, std::vector<uint16_t>(16, 7)));                // material 7
	writeFile(dir / "120010000,130090000.png", storedPng(2, 2, 16, {0, 32, 64, 96}));
	writeFile(dir / "999999999.png", storedPng(1, 1, 16, {0}));

	LogCapture log("com.aionemu.gameserver.geoEngine.GeoWorldLoader");
	GeoWorldLoader::Statistics statistics = GeoWorldLoader::load(mapPtrs(), dir);

	EXPECT_EQ(statistics.meshEntries, 5);
	EXPECT_EQ(statistics.meshes, 6);
	EXPECT_EQ(statistics.meshNames, 6) << "rock_a, fire_a, fire_b, house_01, house_03, door_a";
	EXPECT_EQ(statistics.geoFiles, 1);
	EXPECT_EQ(statistics.placements, 6);
	EXPECT_EQ(statistics.missingMeshPlacements, 1);
	EXPECT_EQ(statistics.attachedNodes, 6) << "5 placements with a model and the house_03 town level node";
	EXPECT_EQ(statistics.geometries, 7);
	EXPECT_EQ(statistics.placementGeometries, 6);
	EXPECT_EQ(statistics.materialGeometries, 2);
	EXPECT_EQ(statistics.terrainMaps, 3);
	EXPECT_EQ(statistics.collisionTrees, 6);

	EXPECT_TRUE(log.contains("info|Loaded terrains for 3 maps"));
	EXPECT_TRUE(log.contains("info|Loaded 6 meshes"));
	EXPECT_TRUE(log.contains("warning|1 meshes are missing:\nmissing/model.cgf"));
	EXPECT_TRUE(log.contains("info|Loaded 6 entities on 1 maps"));
	EXPECT_TRUE(log.contains("120010000.geo is missing")) << log.text();
	EXPECT_FALSE(log.contains("130090000.geo is missing")) << "a prison map needs no geo file";
	EXPECT_TRUE(log.contains("warning|999999999.png of ")) << log.text();
	EXPECT_TRUE(log.contains("999999999.png could not be associated with a map"));

	// material zones: the first child of the alias clone is renamed to the alias, the second keeps the full name
	std::vector<std::string> names = zoneNames;
	std::sort(names.begin(), names.end());
	EXPECT_EQ(names, (std::vector<std::string>{"FIRE_B_CHILD1_260974_110010000|FIRE_B_CHILD1_260974|110010000",
						 "FIRE_B_CHILD2_418423_110010000|FIRE_B_CHILD2_418423|110010000"}));

	models::GeoMap& a = *maps[0];
	EXPECT_EQ(a.getEntityCount(), 6);
	EXPECT_EQ(a.getGeometries().size(), 7u);
	for (const runtime::Ptr<scene::Geometry>& geometry : a.getGeometries())
		EXPECT_NE(geometry->getMesh()->getCollisionTree(), nullptr) << "built eagerly";
	EXPECT_GEO_FLOAT(a.getZ(25, 25, 20, -10, 1), 5.0f) << "the rock";
	EXPECT_GEO_FLOAT(a.getZ(3.3f, 3.3f, 20, -10, 1), 2.0f) << "the terrain";
	EXPECT_EQ(a.getTerrainMaterialAt(2.5f, 2.5f, 2, 1), 7);
	EXPECT_TRUE(maps[1]->hasTerrain());
	EXPECT_TRUE(maps[2]->hasTerrain());
	EXPECT_FALSE(maps[1]->hasTerrainMaterials());
	EXPECT_EQ(maps[1]->getQuantity(), 0);

	// town levels: house_01 has mask 1 | 2 (level 2 has no model), house_03 has 4 | 8 | 16; both start inactive
	EXPECT_GEO_FLOAT(a.getZ(105, 105, 20, -10, 1), NaN);
	a.updateTownToLevel(7, 2);
	EXPECT_GEO_FLOAT(a.getZ(105, 105, 20, -10, 1), 0.0f) << "house_01 is active at level 2";
	a.updateTownToLevel(7, 4);
	EXPECT_GEO_FLOAT(a.getZ(105, 105, 20, -10, 1), 0.0f) << "house_03 is active at level 4";
	a.updateTownToLevel(7, 6);
	EXPECT_GEO_FLOAT(a.getZ(105, 105, 20, -10, 1), NaN) << "no node has bit 1 << 5";
	std::vector<int8_t> masks;
	for (const runtime::Ptr<scene::Geometry>& geometry : a.getGeometries()) {
		runtime::Ptr<scene::DespawnableNode> parent = runtime::as<scene::DespawnableNode>(geometry->getParent());
		if (parent != nullptr && parent->type.get() == scene::DespawnableNode::DespawnableType::TOWN_OBJECT)
			masks.push_back(parent->levelBitMask.get());
	}
	std::sort(masks.begin(), masks.end());
	EXPECT_EQ(masks, (std::vector<int8_t>{3, 28}));

	// doors: a horizontal ray through the door triangle (y 60, x + z / 3 <= 1) hits the active state only
	const int8_t defaultCollisions = -111;
	EXPECT_EQ(a.getCollisions(60.2f, 59, 1, 60.2f, 61, 1, 1, defaultCollisions, nullptr).size(), 0) << "both states start inactive";
	a.setDoorState(1, 5, false);
	EXPECT_EQ(a.getCollisions(60.2f, 59, 1, 60.2f, 61, 1, 1, defaultCollisions, nullptr).size(), 1) << "closed: state 1";
	a.setDoorState(1, 5, true);
	EXPECT_EQ(a.getCollisions(60.2f, 59, 1, 60.2f, 61, 1, 1, defaultCollisions, nullptr).size(), 1) << "open: state 2";
	EXPECT_EQ(a.getCollisions(60.2f, 59, 1, 60.2f, 61, 1, 2, defaultCollisions, nullptr).size(), 0) << "another instance";
}

TEST_F(GeoWorldLoaderFilesTest, LoadErrors) {
	// index size 3
	writeMeshes({{"bad.cgf", {Model{{0, 0, 0, 1, 0, 0, 0, 1, 0}, {0, 1, 2}, 3, 0, 1}}}});
	try {
		GeoWorldLoader::load(mapPtrs(), dir);
		FAIL() << "expected an exception";
	} catch (const commons::utils::Exception& e) {
		EXPECT_STREQ(e.what(), "Could not load meshes") << "Java: GameServerError";
		ASSERT_TRUE(e.cause());
		try {
			std::rethrow_exception(e.cause());
		} catch (const commons::utils::IOException& cause) {
			EXPECT_STREQ(cause.what(), "Index size 3 is not supported");
		}
	}

	// an unknown despawnable type, a town level above 8 and a level for a non-town entity
	writeMeshes({{"a.cgf", {QUAD}}});
	for (auto [type, level, message] : {std::tuple<int8_t, int8_t, const char*>{9, 0, "Invalid ID 9"}, {5, 9, "9 doesn't fit in bit mask"},
			 {2, 1, "Unexpected value in town level field for non-town entity"}}) {
		BigEndianWriter geo;
		writePlacement(geo, "a.cgf", 1, 1, 1, type, 1, level);
		writeFile(dir / "110010000.geo", geo.bytes);
		std::vector<runtime::Ref<models::GeoMap>> fresh{models::GeoMap::create(110010000)};
		try {
			GeoWorldLoader::load({fresh[0]}, dir);
			FAIL() << "expected an exception for " << message;
		} catch (const commons::utils::Exception& e) {
			EXPECT_TRUE(std::string(e.what()).starts_with("Could not load ")) << e.what();
			ASSERT_TRUE(e.cause());
			try {
				std::rethrow_exception(e.cause());
			} catch (const runtime::IllegalArgumentException& cause) {
				EXPECT_STREQ(cause.what(), message);
			}
		}
	}
}

} // namespace
} // namespace aion::gameserver::geoEngine::test
