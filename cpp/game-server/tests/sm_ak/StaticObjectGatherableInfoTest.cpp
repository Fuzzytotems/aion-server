// SM_GATHERABLE_INFO of the Sanctum crafting benches (realdata): in the play session of 2026-09-29 the owner saw the ovens and workbenches of
// Sanctum, STATIC spawns that PlayerController::see shows with this packet, appear late. Reading the code found the packet identical to Java;
// this test pins it byte by byte, so a difference can no longer hide behind the gate's looser decoder. The objects are spawned the way
// SpawnEngine hands a handler="STATIC" group to StaticObjectSpawnManager::spawnTemplate (StaticObjectSpawnManager.java:22-54), from the Java
// tree's own data: the Sanctum row of world_maps.xml, the item_templates.xml rows of the oven and the workbench (the template is the ITEM
// template of the group's npc_id), and spawns/Statics/110010000_Sanctum.xml, bound whole.
//
// The expected bodies are written from SM_GATHERABLE_INFO.java:21-43 and the data files, never from the port: x, y and z are the IEEE 754 bits
// Java's Float.parseFloat gives the spot's attributes (computed outside the port, e.g. Python struct.pack('<f', 1853.085)), the static id is
// the spot's static_id, the template id the item id, the state 1 (an item template is no StaticDoor), the heading the spot's absent h (the
// xsd default 0), the l10n the row's desc (ItemTemplate.getL10nId), then H0 H0 H0 C100. The verbatim rows below pin the data the constants
// come from, so a data change fails here first, with the row. Only the object id is the port's (IDFactory.nextId).

#include <gtest/gtest.h>

#include <bit>
#include <cstdint>
#include <deque>
#include <filesystem>
#include <fstream>
#include <memory>
#include <optional>
#include <set>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include "aion/gameserver/configs/main/WorldConfig.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/ItemData.bind.h"
#include "aion/gameserver/dataholders/ItemData.h"
#include "aion/gameserver/dataholders/MaterialData.bind.h"
#include "aion/gameserver/dataholders/MaterialData.h"
#include "aion/gameserver/dataholders/ShieldData.bind.h"
#include "aion/gameserver/dataholders/ShieldData.h"
#include "aion/gameserver/dataholders/SpawnsData.bind.h"
#include "aion/gameserver/dataholders/SpawnsData.h"
#include "aion/gameserver/dataholders/WorldMapsData.bind.h"
#include "aion/gameserver/dataholders/WorldMapsData.h"
#include "aion/gameserver/dataholders/ZoneData.bind.h"
#include "aion/gameserver/dataholders/ZoneData.h"
#include "aion/gameserver/dataholders/loadingutils/LoadContext.h"
#include "aion/gameserver/dataholders/loadingutils/StaticDataLoader.h"
#include "aion/gameserver/model/gameobjects/StaticObject.h"
#include "aion/gameserver/model/gameobjects/VisibleObject.h"
#include "aion/gameserver/model/templates/spawns/SpawnGroup.h"
#include "aion/gameserver/model/templates/spawns/SpawnTemplate.h"
#include "aion/gameserver/network/aion/serverpackets/SM_GATHERABLE_INFO.h"
#include "aion/gameserver/spawnengine/StaticObjectSpawnManager.h"
#include "aion/gameserver/world/World.h"
#include "aion/gameserver/world/WorldMap.h"
#include "aion/gameserver/world/WorldMapInstance.h"
#include "aion/gameserver/world/geo/GeoService.h"
#include "SmAkTestSupport.h"

namespace aion::gameserver::network::aion::serverpackets::test {
namespace {

using model::gameobjects::VisibleObject;

const std::filesystem::path STATIC_DATA = std::filesystem::path(AION_GAMESERVER_JAVA_DIR) / "data/static_data";

constexpr int32_t SANCTUM = 110010000;
constexpr int32_t OVEN = 150000009;
constexpr int32_t WORKBENCH = 150000012;

/** world_maps.xml:3 */
constexpr std::string_view SANCTUM_ROW =
	R"(<map id="110010000" cName="LC1" name="Sanctum" name_id="400437" water_level="16" death_level="400" world_type="ELYSEA" world_size="3072" drop_type="NONE" flags="RECALL GLIDE RIDE PVP DUEL_SAME_RACE" pve_attack_ratio="150" pve_defend_ratio="50"/>)";
/** items/item_templates.xml:743547 */
constexpr std::string_view OVEN_ROW =
	R"(<item_template id="150000009" name="Oven" level="1" cName="cooking" mask="4190" quality="COMMON" price="100" restrict="0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0" desc="700104"/>)";
/** items/item_templates.xml:743550 */
constexpr std::string_view WORKBENCH_ROW =
	R"(<item_template id="150000012" name="Workbench" level="1" cName="handiwork" mask="4190" quality="COMMON" price="100" restrict="0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0" desc="700107"/>)";
/** spawns/Statics/110010000_Sanctum.xml:49, the first spot of the oven group (npc_id 150000009, :48) */
constexpr std::string_view OVEN_104_SPOT = R"(<spot x="1853.085" y="1548.216" z="590.021" static_id="104"/>)";
/** spawns/Statics/110010000_Sanctum.xml:13, the first spot of the workbench group (npc_id 150000012, :12) */
constexpr std::string_view WORKBENCH_109_SPOT = R"(<spot x="1882.533" y="1464.432" z="590.419" static_id="109"/>)";

/** One expected SM_GATHERABLE_INFO body: the spot's coordinates as Float.parseFloat bits, the static id, the item id and its desc */
struct JavaGatherableInfo {
	uint32_t xBits, yBits, zBits;
	int32_t staticId;
	int32_t templateId;
	int32_t l10nId;
};

/** Float.parseFloat("1853.085") = 0x44E7A2B8, "1548.216" = 0x44C186E9, "590.021" = 0x44138158; desc="700104" */
constexpr JavaGatherableInfo OVEN_104{0x44E7A2B8u, 0x44C186E9u, 0x44138158u, 104, OVEN, 700104};
/** Float.parseFloat("1882.533") = 0x44EB510E, "1464.432" = 0x44B70DD3, "590.419" = 0x44139AD1; desc="700107" */
constexpr JavaGatherableInfo WORKBENCH_109{0x44EB510Eu, 0x44B70DD3u, 0x44139AD1u, 109, WORKBENCH, 700107};

std::string readText(const std::filesystem::path& file) {
	std::ifstream in(file, std::ios::binary);
	std::ostringstream text;
	text << in.rdbuf();
	return text.str();
}

/** The trimmed line of `text` that starts with `start` (the rows used here are one self-closing element per line), empty if there is none */
std::string lineStartingWith(std::string_view text, std::string_view start) {
	for (size_t at = text.find(start); at != std::string_view::npos; at = text.find(start, at + 1)) {
		const size_t lineStart = text.rfind('\n', at) == std::string_view::npos ? 0 : text.rfind('\n', at) + 1;
		if (text.find_first_not_of(" \t", lineStart) != at)
			continue;
		size_t end = text.find('\n', at);
		std::string_view line = text.substr(at, end == std::string_view::npos ? std::string_view::npos : end - at);
		while (!line.empty() && (line.back() == '\r' || line.back() == ' ' || line.back() == '\t'))
			line.remove_suffix(1);
		return std::string(line);
	}
	return {};
}

/**
 * The holders World and GeoService read, once per process (a holder is published once): the Sanctum row of the real world_maps.xml, no zones,
 * shields or materials, the shipped region size (WorldConfig.java:15) and no twin instances; then GeoService.init's empty GeoMap per map (geo
 * data is off by default), which VisibleObjectController.onBeforeSpawn needs for a spot with a static id (GeoService.spawnPlaceableObject).
 * No other test of this executable uses the World.
 */
void publishSanctumWorldOnce() {
	static const bool published = [] {
		runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
		configs::main::WorldConfig::WORLD_REGION_SIZE.store(128);
		configs::main::WorldConfig::WORLD_MAX_TWINS_USUAL.store(0);
		configs::main::WorldConfig::WORLD_MAX_TWINS_BEGINNER.store(0);
		static std::deque<xml::LoadContext> contexts;
		const std::string row = lineStartingWith(readText(STATIC_DATA / "world_maps.xml"), R"(<map id="110010000")");
		dataholders::DataManager::WORLD_MAPS_DATA.publish(
			xml::bindString<dataholders::WorldMapsData>(contexts.emplace_back(), "<world_maps>" + row + "</world_maps>"));
		dataholders::DataManager::ZONE_DATA.publish(xml::bindString<dataholders::ZoneData>(contexts.emplace_back(), "<zones/>"));
		dataholders::DataManager::SHIELD_DATA.publish(xml::bindString<dataholders::ShieldData>(contexts.emplace_back(), "<shields/>"));
		dataholders::DataManager::MATERIAL_DATA.publish(
			xml::bindString<dataholders::MaterialData>(contexts.emplace_back(), "<material_templates/>"));
		world::geo::GeoService::getInstance().init();
		return true;
	}();
	static_cast<void>(published);
}

class StaticObjectGatherableInfoTest : public PacketTest {
protected:
	void SetUp() override {
		PacketTest::SetUp();
		if (dataholders::DataManager::ITEM_DATA)
			GTEST_SKIP() << "another test of this process published ITEM_DATA (run the test on its own)";
		const std::string items = readText(STATIC_DATA / "items/item_templates.xml");
		ovenRow = lineStartingWith(items, R"(<item_template id="150000009")");
		workbenchRow = lineStartingWith(items, R"(<item_template id="150000012")");
		statics = readText(STATIC_DATA / "spawns/Statics/110010000_Sanctum.xml");
		ASSERT_FALSE(statics.empty()) << "no spawns/Statics/110010000_Sanctum.xml below " << STATIC_DATA;
		publishSanctumWorldOnce();
		// StaticObjectSpawnManager.java:23: DataManager.ITEM_DATA.getItemTemplate(spawn.getNpcId()); the two rows verbatim from the real file
		xml::LoadContext context;
		dataholders::DataManager::ITEM_DATA.publish(
			xml::bindString<dataholders::ItemData>(context, "<item_templates>" + ovenRow + workbenchRow + "</item_templates>"));
		spawns = xml::bindString<dataholders::SpawnsData>(spawnContext, statics, "110010000_Sanctum.xml");
	}

	void TearDown() override {
		for (const runtime::Ref<VisibleObject>& object : spawned)
			world::World::getInstance().removeObject(*object);
		spawned.clear();
		spawns.reset();
		dataholders::DataManager::ITEM_DATA.resetForTests();
		PacketTest::TearDown();
	}

	static world::WorldMapInstance& sanctum() { return *world::World::getInstance().getWorldMap(SANCTUM)->getMainWorldMapInstance(); }

	/** SpawnEngine.spawnInstance's STATIC arm for the groups of one npc id on Sanctum's main instance; returns the objects it spawned */
	std::vector<runtime::Ref<VisibleObject>> spawnStatics(int32_t npcId) {
		world::WorldMapInstance& instance = sanctum();
		std::set<int32_t> before;
		for (const runtime::Ptr<VisibleObject>& object : instance)
			before.insert(object->getObjectId());
		const std::vector<runtime::Ref<model::templates::spawns::SpawnGroup>> groups = spawns->getSpawnsForNpc(SANCTUM, npcId);
		EXPECT_EQ(groups.size(), 1u) << "one handler=\"STATIC\" group of " << npcId << " in 110010000_Sanctum.xml";
		for (const runtime::Ref<model::templates::spawns::SpawnGroup>& group : groups)
			spawnengine::StaticObjectSpawnManager::spawnTemplate(*group, instance.getInstanceId());
		std::vector<runtime::Ref<VisibleObject>> result;
		for (const runtime::Ptr<VisibleObject>& object : instance) {
			if (!before.contains(object->getObjectId())) {
				result.emplace_back(*object);
				spawned.emplace_back(*object);
			}
		}
		return result;
	}

	/** the spawned StaticObject of the spot with `staticId` */
	static VisibleObject* withStaticId(const std::vector<runtime::Ref<VisibleObject>>& objects, int32_t staticId) {
		for (const runtime::Ref<VisibleObject>& object : objects) {
			if (object->getSpawn() && object->getSpawn()->getStaticId() == staticId)
				return object.get();
		}
		return nullptr;
	}

	/** SM_GATHERABLE_INFO.java:21-43 for the expected values and the object's runtime object id */
	static std::vector<uint8_t> javaBody(const JavaGatherableInfo& java, int32_t objectId) {
		return Bytes()
			.D(static_cast<int32_t>(java.xBits))
			.D(static_cast<int32_t>(java.yBits))
			.D(static_cast<int32_t>(java.zBits))
			.D(objectId)
			.D(java.staticId)
			.D(java.templateId)
			.H(1) // not a StaticDoor
			.C(0) // the spot's heading: no h attribute
			.D(java.l10nId)
			.H(0)
			.H(0)
			.H(0)
			.C(100)
			.data;
	}

	std::string ovenRow, workbenchRow, statics;
	xml::LoadContext spawnContext;
	std::unique_ptr<dataholders::SpawnsData> spawns;
	std::vector<runtime::Ref<VisibleObject>> spawned;
};

TEST_F(StaticObjectGatherableInfoTest, TheDataRowsAreTheOnesTheExpectedBytesComeFrom) {
	EXPECT_EQ(lineStartingWith(readText(STATIC_DATA / "world_maps.xml"), R"(<map id="110010000")"), SANCTUM_ROW);
	EXPECT_EQ(ovenRow, OVEN_ROW);
	EXPECT_EQ(workbenchRow, WORKBENCH_ROW);
	EXPECT_EQ(lineStartingWith(statics, R"(<spot x="1853.085")"), OVEN_104_SPOT);
	EXPECT_EQ(lineStartingWith(statics, R"(<spot x="1882.533")"), WORKBENCH_109_SPOT);
	// Java writes x, y and z with writeF (Float.floatToIntBits, little endian): the bits are those of the nearest float
	EXPECT_EQ(std::bit_cast<uint32_t>(1853.085f), OVEN_104.xBits);
	EXPECT_EQ(std::bit_cast<uint32_t>(590.419f), WORKBENCH_109.zBits);
}

TEST_F(StaticObjectGatherableInfoTest, TheOvenWithStaticId104IsWrittenAsJavaWritesIt) {
	const std::vector<runtime::Ref<VisibleObject>> ovens = spawnStatics(OVEN);
	// StaticObjectSpawnManager.java:39-44: one StaticObject per spot of a group without a pool (110010000_Sanctum.xml:48-53)
	std::set<int32_t> staticIds;
	for (const runtime::Ref<VisibleObject>& oven : ovens)
		staticIds.insert(oven->getSpawn()->getStaticId());
	EXPECT_EQ(staticIds, (std::set<int32_t>{38, 103, 104, 118}));
	VisibleObject* oven = withStaticId(ovens, 104);
	ASSERT_NE(oven, nullptr);
	ASSERT_NE(dynamic_cast<model::gameobjects::StaticObject*>(oven), nullptr);
	ASSERT_NE(oven->getObjectId(), 0);

	const std::vector<uint8_t> body = dataOf(SM_GATHERABLE_INFO(*oven));

	EXPECT_EQ(body.size(), 38u);
	EXPECT_EQ(body, javaBody(OVEN_104, oven->getObjectId()));
}

TEST_F(StaticObjectGatherableInfoTest, TheWorkbenchWithStaticId109IsWrittenAsJavaWritesIt) {
	const std::vector<runtime::Ref<VisibleObject>> workbenches = spawnStatics(WORKBENCH);
	EXPECT_EQ(workbenches.size(), 6u) << "110010000_Sanctum.xml:12-19";
	VisibleObject* workbench = withStaticId(workbenches, 109);
	ASSERT_NE(workbench, nullptr);
	ASSERT_NE(dynamic_cast<model::gameobjects::StaticObject*>(workbench), nullptr);

	const std::vector<uint8_t> body = dataOf(SM_GATHERABLE_INFO(*workbench));

	EXPECT_EQ(body.size(), 38u);
	EXPECT_EQ(body, javaBody(WORKBENCH_109, workbench->getObjectId()));
}

} // namespace
} // namespace aion::gameserver::network::aion::serverpackets::test
