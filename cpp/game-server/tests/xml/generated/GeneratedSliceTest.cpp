// Data-only slice of the generated static data code (tools/xmlgen, docs/design/static-data.md §5 WP6): world_maps.xml,
// tribe/tribe_relations.xml and the npc_walker/ directory (13 files) are loaded from the Java data tree through the binder runtime with the
// generated enums, data structs, member blocks and XmlBinding specializations, strict and with hooks. The results are compared with the
// independent oracle outputs in cpp/tools/oracle/expected:
// - V2: the "Loaded N ..." counts of these holders (static_data_counts.json);
// - V3: element and attribute totals per import (totals.json byImport), per tag and attribute (census.json paths of these holders), and the
//   global per-tag totals (totals.json byTag) for tags that occur in no other holder.
// The hand-written classes are test shells (tests/xml/generated/aion/...), created with `xmlgen.py scaffold`.
//
// Includes: aion_gs_xml_tests has cpp/game-server/generated as include directory. The test shells next to this file win over the shells of
// the same classes in game-server/src, because MSVC searches the directories of the open files before the /I directories; for the same reason
// this executable must not link the static data library (aion_gs_staticdata), whose shells define the same classes.

#include <algorithm>
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
#include <vector>

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include "aion/gameserver/dataholders/loadingutils/StaticDataLoader.h"
#include "aion/gameserver/runtime/sched/Pin.h"
#include "aion/gameserver/runtime/sched/TaskConcepts.h"

#include "aion/gameserver/dataholders/WorldMapsData.bind.ipp"
#include "aion/gameserver/model/templates/world/WorldMapTemplate.bind.ipp"
#include "aion/gameserver/model/templates/world/AiInfo.bind.ipp"
#include "aion/gameserver/dataholders/TribeRelationsData.bind.ipp"
#include "aion/gameserver/model/templates/tribe/Tribe.bind.ipp"
#include "aion/gameserver/dataholders/WalkerData.bind.ipp"
#include "aion/gameserver/model/templates/walker/WalkerTemplate.bind.ipp"
#include "aion/gameserver/model/templates/walker/RouteStep.bind.ipp"

namespace aion::gameserver::xml::generated_slice {
namespace {

using dataholders::TribeRelationsData;
using dataholders::WalkerData;
using dataholders::WorldMapsData;
using json = nlohmann::json;
using model::TribeClass;
using model::templates::walker::WalkerTemplate;
using model::templates::world::WorldMapTemplate;

const std::filesystem::path STATIC_DATA = std::filesystem::path(AION_GAMESERVER_JAVA_DIR) / "data/static_data";
const std::filesystem::path ORACLE = std::filesystem::path(AION_GAMESERVER_JAVA_DIR) / "../cpp/tools/oracle/expected";
const std::set<std::string, std::less<>> SLICE{"npc_walker", "tribe_relations", "world_maps"};

json readJson(const std::filesystem::path& file) {
	std::ifstream in(file, std::ios::binary);
	if (!in)
		throw std::runtime_error("cannot open " + file.string());
	return json::parse(in);
}

/** holder root of a census path ("npc_walker/walker_template@route_id" -> npc_walker) */
std::string holderOf(const std::string& path) {
	return path.substr(0, path.find_first_of("/@#"));
}

class GeneratedSliceTest : public testing::Test {
protected:
	static void SetUpTestSuite() {
		if (!std::filesystem::exists(STATIC_DATA / "static_data.xml") || !std::filesystem::exists(ORACLE / "totals.json"))
			return;
		HolderRegistry registry;
		registry.add(HolderRegistration::of<WorldMapsData>("world_maps"));
		registry.add(HolderRegistration::of<TribeRelationsData>("tribe_relations"));
		registry.add(HolderRegistration::of<WalkerData>("npc_walker"));
		LoadOptions options;
		options.strict = true;
		options.collectStats = true;
		options.holders = SLICE;
		context = std::make_unique<LoadContext>(options);
		WorldMapTemplate::hookCalls = 0;
		auto start = std::chrono::steady_clock::now();
		StaticDataLoader(registry).load(*context, STATIC_DATA / "static_data.xml");
		context->resolveIdRefs();
		auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count();
		std::cout << "generated slice: world_maps, tribe_relations, npc_walker loaded in " << millis << " ms\n";
		maps = context->takeHolder<WorldMapsData>();
		tribes = context->takeHolder<TribeRelationsData>();
		walkers = context->takeHolder<WalkerData>();
		counts = readJson(ORACLE / "static_data_counts.json");
		totals = readJson(ORACLE / "totals.json");
		census = readJson(ORACLE / "census.json");
	}

	static void TearDownTestSuite() {
		walkers.reset();
		tribes.reset();
		maps.reset();
		context.reset();
	}

	void SetUp() override {
		if (!context)
			GTEST_SKIP() << "Java data tree or oracle outputs not found: " << STATIC_DATA << ", " << ORACLE;
	}

	/** the value of the oracle's "Loaded N ..." line of one holder */
	static int64_t expectedCount(std::string_view holder) {
		for (const json& line : counts.at("lines")) {
			const json& holders = line.at("holders");
			if (holders.size() == 1 && holders[0].get<std::string>() == holder)
				return line.at("values").at(0).get<int64_t>();
		}
		throw std::runtime_error("no count line for " + std::string(holder));
	}

	static inline std::unique_ptr<LoadContext> context;
	static inline std::unique_ptr<WorldMapsData> maps;
	static inline std::unique_ptr<TribeRelationsData> tribes;
	static inline std::unique_ptr<WalkerData> walkers;
	static inline json counts;
	static inline json totals;
	static inline json census;
};

TEST_F(GeneratedSliceTest, HolderCountsMatchTheCountOracle) {
	EXPECT_EQ(static_cast<int64_t>(maps->size()), expectedCount("world_maps")) << "Loaded N maps";
	EXPECT_EQ(static_cast<int64_t>(tribes->size()), expectedCount("tribe_relations")) << "Loaded N tribe relation entries";
	EXPECT_EQ(static_cast<int64_t>(walkers->size()), expectedCount("npc_walker")) << "Loaded N walker routes (first route id wins)";
	EXPECT_EQ(WorldMapTemplate::hookCalls, maps->getWorldMaps().size()) << "every bound map ran its hook once";
}

TEST_F(GeneratedSliceTest, ElementAndAttributeTotalsMatchTheImports) {
	uint64_t elements = 0;
	uint64_t attributes = 0;
	size_t imports = 0;
	for (const json& entry : totals.at("byImport")) {
		if (!SLICE.contains(entry.at("root").get<std::string>()))
			continue;
		++imports;
		// like the oracle, the binder does not count the dropped roots of the later files of a directory import per tag (skippedRoots)
		elements += entry.at("elements").get<uint64_t>();
		attributes += entry.at("attributes").get<uint64_t>();
	}
	EXPECT_EQ(imports, SLICE.size());
	const BindStats& stats = context->stats();
	EXPECT_EQ(stats.totalElements().bound, elements);
	EXPECT_EQ(stats.totalElements().unknown, 0u);
	EXPECT_EQ(stats.totalAttributes().bound, attributes) << "xmlns/xsi attributes and later root attributes are never counted per tag";
	EXPECT_EQ(stats.totalAttributes().unknown, 0u);
	for (const BindStats::ElementStat& element : stats.snapshot())
		EXPECT_EQ(element.unknownText, 0u) << element.tag;
	EXPECT_EQ(stats.element("npc_walker").bound, 1u) << "the root of the first file only";
	EXPECT_EQ(stats.element("world_maps").bound, 1u);
	EXPECT_EQ(stats.element("tribe_relations").bound, 1u);

	// the totals document (BindStats::writeTotals) lists the same skipped roots as the oracle
	std::ostringstream written;
	stats.writeTotals(written, STATIC_DATA.generic_string());
	json actual = json::parse(written.str());
	EXPECT_EQ(actual.at("format"), "aion-staticdata-totals");
	EXPECT_EQ(actual.at("elements").get<uint64_t>(), elements);
	json expectedRoots = json::array();
	for (const json& root : totals.at("skippedRoots")) {
		if (SLICE.contains(root.at("tag").get<std::string>()))
			expectedRoots.push_back(root);
	}
	EXPECT_EQ(expectedRoots.size(), 12u) << "npc_walker has 13 files";
	EXPECT_EQ(actual.at("skippedRoots"), expectedRoots);
}

TEST_F(GeneratedSliceTest, TagAndAttributeCountsMatchTheCensus) {
	// census paths: holder/element/...@attribute (value count, elementCount = elements on that path) and holder/.../element#text
	std::map<std::string, uint64_t> elementPaths;                  // element path -> elements
	std::map<std::pair<std::string, std::string>, uint64_t> expectedAttributes; // (tag, attribute) -> values
	for (const auto& [path, entry] : census.at("paths").items()) {
		if (!SLICE.contains(holderOf(path)))
			continue;
		size_t split = path.find_first_of("@#");
		std::string elementPath = path.substr(0, split);
		uint64_t& elementCount = elementPaths[elementPath];
		elementCount = std::max(elementCount, entry.at("elementCount").get<uint64_t>());
		if (split != std::string::npos && path[split] == '@') {
			std::string tag = elementPath.substr(elementPath.rfind('/') + 1);
			expectedAttributes[{tag, path.substr(split + 1)}] += entry.at("count").get<uint64_t>();
		}
	}
	std::map<std::string, uint64_t> expectedElements;
	for (const auto& [elementPath, count] : elementPaths)
		expectedElements[elementPath.substr(elementPath.rfind('/') + 1)] += count;

	const BindStats& stats = context->stats();
	size_t tagsChecked = 0;
	for (const BindStats::ElementStat& element : stats.snapshot()) {
		if (SLICE.contains(element.tag))
			continue;
		ASSERT_TRUE(expectedElements.contains(element.tag)) << "bound tag <" << element.tag << "> is not in the census of the slice holders";
		EXPECT_EQ(element.counts.bound, expectedElements.at(element.tag)) << element.tag;
		EXPECT_EQ(element.counts.ignored, 0u) << element.tag;
		for (const BindStats::AttributeStat& attribute : element.attributes) {
			auto it = expectedAttributes.find({element.tag, attribute.name});
			ASSERT_NE(it, expectedAttributes.end()) << element.tag << "@" << attribute.name;
			EXPECT_EQ(attribute.counts.bound, it->second) << element.tag << "@" << attribute.name;
		}
		++tagsChecked;
	}
	for (const auto& [key, count] : expectedAttributes)
		EXPECT_EQ(stats.attribute(key.first, key.second).bound, count) << key.first << "@" << key.second;
	size_t censusTags = 0;
	for (const auto& [tag, count] : expectedElements)
		censusTags += SLICE.contains(tag) ? 0 : 1;
	EXPECT_EQ(tagsChecked, censusTags) << "every census tag of the slice is bound";
}

TEST_F(GeneratedSliceTest, TagsOfOnlyTheseHoldersMatchTheGlobalTotals) {
	std::set<std::string> sliceTags;
	std::set<std::string> otherTags;
	for (const auto& [path, entry] : census.at("paths").items()) {
		std::string elementPath = path.substr(0, path.find_first_of("@#"));
		std::set<std::string>& target = SLICE.contains(holderOf(path)) ? sliceTags : otherTags;
		for (size_t start = 0; start <= elementPath.size();) {
			size_t end = elementPath.find('/', start);
			if (end == std::string::npos)
				end = elementPath.size();
			target.insert(elementPath.substr(start, end - start));
			start = end + 1;
		}
	}
	const BindStats& stats = context->stats();
	size_t exclusive = 0;
	for (const std::string& tag : sliceTags) {
		if (otherTags.contains(tag) || SLICE.contains(tag))
			continue;
		++exclusive;
		const json& entry = totals.at("byTag").at(tag);
		EXPECT_EQ(stats.element(tag).bound, entry.at("count").get<uint64_t>()) << tag;
		for (const auto& [name, count] : entry.at("attributes").items())
			EXPECT_EQ(stats.attribute(tag, name).bound, count.get<uint64_t>()) << tag << "@" << name;
	}
	EXPECT_GE(exclusive, 5u) << "walker_template, routestep, tribe, the relation lists and ai_info";
}

TEST_F(GeneratedSliceTest, BoundValues) {
	// <map id="110010000" cName="LC1" name="Sanctum" name_id="400437" water_level="16" death_level="400" world_type="ELYSEA" world_size="3072"
	//      drop_type="NONE" flags="RECALL GLIDE RIDE PVP DUEL_SAME_RACE" pve_attack_ratio="150" pve_defend_ratio="50"/>
	const WorldMapTemplate* sanctum = maps->getTemplate(110010000);
	ASSERT_NE(sanctum, nullptr);
	EXPECT_EQ(sanctum->getCName(), "LC1");
	EXPECT_EQ(sanctum->getName(), "Sanctum");
	EXPECT_EQ(sanctum->getWaterLevel(), 16);
	EXPECT_EQ(sanctum->getDeathLevel(), 400);
	EXPECT_EQ(sanctum->getWorldType(), world::WorldType::ELYSEA);
	EXPECT_EQ(sanctum->getWorldSize(), 3072);
	EXPECT_EQ(sanctum->getWorldDropType(), world::WorldDropType::NONE);
	EXPECT_EQ(sanctum->getPvEAttackRatio(), 150);
	EXPECT_FALSE(sanctum->isInstance()) << "Java default";
	EXPECT_EQ(sanctum->getBoundAiInfo(), nullptr) << "no <ai_info>: null, the Java getter substitutes AiInfo.DEFAULT";
	EXPECT_EQ(sanctum->getAiInfo().getChaseTarget(), 50);
	using world::zone::ZoneAttributes;
	ASSERT_TRUE(sanctum->getFlagValues().has_value());
	EXPECT_EQ(*sanctum->getFlagValues(), (std::vector<ZoneAttributes>{ZoneAttributes::RECALL, ZoneAttributes::GLIDE, ZoneAttributes::RIDE,
	                                                                   ZoneAttributes::PVP_ENABLED, ZoneAttributes::DUEL_SAME_RACE_ENABLED}))
	  << "@XmlList attribute of an enum with @XmlEnumValue";
	size_t withAiInfo = 0;
	for (const WorldMapTemplate& map : maps->getWorldMaps())
		withAiInfo += map.getBoundAiInfo() != nullptr ? 1 : 0;
	EXPECT_EQ(withAiInfo, 3u);

	// <tribe name="AB1_DOORKILLER" base="NONE"><aggro>AB1_DOOR_DA AB1_DOOR_LI</aggro></tribe>
	const model::templates::tribe::Tribe* doorKiller = tribes->getTribe(TribeClass::AB1_DOORKILLER);
	ASSERT_NE(doorKiller, nullptr);
	EXPECT_EQ(doorKiller->getBase(), TribeClass::AB1_DOORKILLER);
	ASSERT_TRUE(doorKiller->getAggroList().has_value()) << "@XmlList element of a 724-constant enum";
	EXPECT_EQ(*doorKiller->getAggroList(), (std::vector<TribeClass>{TribeClass::AB1_DOOR_DA, TribeClass::AB1_DOOR_LI}));
	EXPECT_FALSE(doorKiller->getFriendList().has_value()) << "absent @XmlList element stays null";
	EXPECT_EQ(xml::enumName(TribeClass::AB1_DOORKILLER), "AB1_DOORKILLER");

	// npc_walker/300100000_Steel Rake.xml: <walker_template route_id="0222C904738D487BE4D4F76C9DC76E63A8FD1EB9" pool="2" formation="SQUARE" rows="1,1">
	const WalkerTemplate* route = walkers->getWalkerTemplate("0222C904738D487BE4D4F76C9DC76E63A8FD1EB9");
	ASSERT_NE(route, nullptr);
	EXPECT_EQ(route->getPool(), 2);
	EXPECT_EQ(route->getType(), spawnengine::WalkerGroupType::SQUARE);
	EXPECT_EQ(route->getRowValues(), "1,1");
	EXPECT_EQ(route->getLoopType(), WalkerTemplate::LoopType::NORMAL) << "Java default of a nested enum";
	ASSERT_FALSE(route->getRouteSteps().empty());
	EXPECT_TRUE(route->getRouteSteps().back()->isLastStep());
	EXPECT_EQ(route->getRouteSteps().back()->getStepIndex(), static_cast<int32_t>(route->getRouteSteps().size() - 1));
	float z = route->getRouteSteps().front()->getZ();
	route->getRouteSteps().front()->setZ(z + 1.0f);
	EXPECT_EQ(route->getRouteSteps().front()->getZ(), z + 1.0f) << "runtime_mutable RouteStep.z is written through a const template";
	route->getRouteSteps().front()->setZ(z);
	EXPECT_EQ(walkers->getBoundTemplates().size(), 6449u + walkers->getDuplicateRoutes());
}

TEST(GeneratedSliceStrictTest, UnknownAttributesAndMissingRequiredOnesFail) {
	LoadContext context;
	EXPECT_THROW(bindString<TribeRelationsData>(context, R"(<tribe_relations><tribe name="AB1_DOORKILLER" colour="red"/></tribe_relations>)"),
	             StaticDataException);
	LoadContext second;
	EXPECT_THROW(bindString<TribeRelationsData>(second, R"(<tribe_relations><tribe base="NONE"/></tribe_relations>)"), StaticDataException)
	  << "@XmlAttribute(required = true) name";
	LoadContext third;
	EXPECT_THROW(bindString<WalkerData>(third, R"(<npc_walker><walker_template route_id="x"/></npc_walker>)"), StaticDataException)
	  << "@XmlElement(name = \"routestep\", required = true)";
	LoadContext fourth;
	std::unique_ptr<WorldMapsData> parsed = bindString<WorldMapsData>(
	  fourth, R"(<world_maps><map id="1" cName="A" death_level="0" water_level="1" flags=""><ai_info chase_target="7"/></map></world_maps>)");
	ASSERT_EQ(parsed->size(), 1u);
	const WorldMapTemplate* map = parsed->getTemplate(1);
	ASSERT_NE(map, nullptr);
	ASSERT_NE(map->getBoundAiInfo(), nullptr);
	EXPECT_EQ(map->getAiInfo().getChaseTarget(), 7);
	EXPECT_EQ(map->getAiInfo().getChaseHome(), 200) << "Java initializer of an absent attribute";
	ASSERT_TRUE(map->getFlagValues().has_value());
	EXPECT_TRUE(map->getFlagValues()->empty()) << "present-empty list attribute";
}

// K1 marker: generated data structs and behaviour shells of hierarchy roots derive runtime::StaticTemplate, so template pointers pass the
// task capture and pin rules (runtime-architecture.md §7.3) without waivers
static_assert(runtime::IsTemplatePtr<const WorldMapTemplate*>);
static_assert(runtime::IsTemplatePtr<const model::templates::world::AiInfo*>);
static_assert(runtime::IsTemplatePtr<const model::templates::walker::RouteStep*>);
static_assert(runtime::IsTemplatePtr<const TribeRelationsData*>);
static_assert(runtime::Pinnable<WalkerTemplate>);
static_assert(sizeof(model::templates::world::AiInfo) == 2 * sizeof(int32_t), "empty base optimization: the marker adds no storage");

} // namespace
} // namespace aion::gameserver::xml::generated_slice
