// P4-07a on the real static data (docs/design/handlers-and-porting-plan.md §3.2): the item, decomposable item, npc, spawn, world map, weather, pet
// and zone imports of the Java data tree are bound through the xml runtime with hooks (strict mode), into test holders of this file (the real
// holders and their hooks belong to P4-09). Checks:
// - the holder counts of the count oracle (tools/oracle/expected/static_data_counts.json) and the element/attribute totals per import
//   (totals.json byImport, bound plus deliberately ignored);
// - hook results against values recomputed from the DOM (world map flags, npc siege teleporters, item tune counts, temporary spawn expressions);
// - a field dump of 20 random items and 20 random npcs against their XML attributes.
// Skips: the item <actions> elements are removed from the DOM while item action hooks (P5-07) are unported, the npc <equipment> elements while
// NpcEquippedGear::init (P4-13) is unported (the totals are compared only without removals); the zones import is skipped while
// ZoneName::createOrGet (P4-10) is unported.

#include <gtest/gtest.h>

#include <algorithm>
#include <charconv>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <map>
#include <memory>
#include <random>
#include <set>
#include <string>
#include <typeinfo>
#include <vector>

#include <nlohmann/json.hpp>
#include <pugixml.hpp>

#include "aion/gameserver/dataholders/loadingutils/BindContext.h"
#include "aion/gameserver/dataholders/loadingutils/StaticDataImports.h"
#include "aion/gameserver/dataholders/loadingutils/StaticDataLoader.h"
#include "aion/gameserver/model/templates/item/DecomposableItemInfo.bind.h"
#include "aion/gameserver/model/templates/item/ItemTemplate.bind.h"
#include "aion/gameserver/model/templates/item/enums/ItemGroupInfo.h"
#include "aion/gameserver/model/templates/npc/NpcTemplate.bind.h"
#include "aion/gameserver/model/templates/pet/PetFlavour.bind.h"
#include "aion/gameserver/model/templates/pet/PetTemplate.bind.h"
#include "aion/gameserver/model/templates/spawns/SpawnMap.bind.h"
#include "aion/gameserver/model/templates/spawns/TemporarySpawn.h"
#include "aion/gameserver/model/templates/world/WeatherTable.bind.h"
#include "aion/gameserver/model/templates/world/WorldMapTemplate.bind.h"
#include "aion/gameserver/model/templates/zone/ZoneTemplate.bind.h"
#include "aion/gameserver/runtime/base/TaskInfo.h"
#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/runtime/lifetime/TaskScope.h"
#include "aion/gameserver/world/zone/ZoneName.h"

namespace aion::gameserver::model::templates::realdata {

// ---- test holders (one list per import, like the Java holders' bound fields) ------------------------------------------------------------------

struct Items {
	std::vector<item::ItemTemplate> list;
};
struct Decomposables {
	std::vector<item::DecomposableItemInfo> list;
};
struct Npcs {
	std::vector<npc::NpcTemplate> list;
};
struct SpawnMaps {
	std::vector<spawns::SpawnMap> list;
};
struct WorldMaps {
	std::vector<world::WorldMapTemplate> list;
};
struct Weather {
	std::vector<world::WeatherTable> list;
};
struct Pets {
	std::vector<pet::PetTemplate> list;
};
struct PetFeed {
	std::vector<pet::PetFlavour> list;
};
struct Zones {
	std::vector<zone::ZoneTemplate> list;
};

} // namespace aion::gameserver::model::templates::realdata

namespace aion::gameserver::xml {

/** XmlBinding of a test holder: the children named `ELEMENT` are bound in place into `list` */
template <class H, class T, const char* ELEMENT>
struct TestHolderBinding {
	static constexpr std::string_view CLASS_NAME = "TestHolder";
	static bool element(H& o, BindContext& c, pugi::xml_node e, std::string_view name) {
		if (name != ELEMENT)
			return false;
		c.bindList(o.list, e);
		return true;
	}
	static void reserve(H& o, const ChildCounts& counts) { o.list.reserve(counts[ELEMENT]); }
};

inline constexpr char ITEM_TEMPLATE[] = "item_template";
inline constexpr char DECOMPOSABLE[] = "decomposable";
inline constexpr char NPC_TEMPLATE[] = "npc_template";
inline constexpr char SPAWN_MAP[] = "spawn_map";
inline constexpr char MAP[] = "map";
inline constexpr char PET[] = "pet";
inline constexpr char FLAVOUR[] = "flavour";
inline constexpr char ZONE[] = "zone";

namespace rd = ::aion::gameserver::model::templates::realdata;
namespace tpl = ::aion::gameserver::model::templates;
template <>
struct XmlBinding<rd::Items> : TestHolderBinding<rd::Items, tpl::item::ItemTemplate, ITEM_TEMPLATE> {};
template <>
struct XmlBinding<rd::Decomposables> : TestHolderBinding<rd::Decomposables, tpl::item::DecomposableItemInfo, DECOMPOSABLE> {};
template <>
struct XmlBinding<rd::Npcs> : TestHolderBinding<rd::Npcs, tpl::npc::NpcTemplate, NPC_TEMPLATE> {};
template <>
struct XmlBinding<rd::SpawnMaps> : TestHolderBinding<rd::SpawnMaps, tpl::spawns::SpawnMap, SPAWN_MAP> {};
template <>
struct XmlBinding<rd::WorldMaps> : TestHolderBinding<rd::WorldMaps, tpl::world::WorldMapTemplate, MAP> {};
template <>
struct XmlBinding<rd::Weather> : TestHolderBinding<rd::Weather, tpl::world::WeatherTable, MAP> {};
template <>
struct XmlBinding<rd::Pets> : TestHolderBinding<rd::Pets, tpl::pet::PetTemplate, PET> {};
template <>
struct XmlBinding<rd::PetFeed> : TestHolderBinding<rd::PetFeed, tpl::pet::PetFlavour, FLAVOUR> {};
template <>
struct XmlBinding<rd::Zones> : TestHolderBinding<rd::Zones, tpl::zone::ZoneTemplate, ZONE> {};

} // namespace aion::gameserver::xml

namespace aion::gameserver::model::templates::realdata {
namespace {

using json = nlohmann::json;
using Documents = std::vector<std::unique_ptr<xml::XmlDocument>>;

const std::filesystem::path STATIC_DATA = std::filesystem::path(AION_GAMESERVER_JAVA_DIR) / "data/static_data";
const std::filesystem::path ORACLE = std::filesystem::path(AION_GAMESERVER_JAVA_DIR) / "../cpp/tools/oracle/expected";

json readJson(const std::filesystem::path& file) {
	std::ifstream in(file, std::ios::binary);
	if (!in)
		throw std::runtime_error("cannot open " + file.string());
	return json::parse(in);
}

// ---- DOM attribute access (the independent side of the field dump) ------------------------------------------------------------------------------

std::string text(pugi::xml_node node, const char* name, const char* fallback = "") {
	pugi::xml_attribute attribute = node.attribute(name);
	return attribute ? std::string(attribute.value()) : std::string(fallback);
}

int64_t number(pugi::xml_node node, const char* name, int64_t fallback = 0) {
	pugi::xml_attribute attribute = node.attribute(name);
	if (!attribute)
		return fallback;
	std::string_view value = attribute.value();
	int64_t result = 0;
	std::from_chars(value.data(), value.data() + value.size(), result);
	return result;
}

float decimal(pugi::xml_node node, const char* name, float fallback = 0.0f) {
	pugi::xml_attribute attribute = node.attribute(name);
	if (!attribute)
		return fallback;
	std::string_view value = attribute.value();
	float result = 0.0f;
	std::from_chars(value.data(), value.data() + value.size(), result);
	return result;
}

/** the children of all roots with the element name, in binding order */
std::vector<pugi::xml_node> children(const Documents& documents, const char* name) {
	std::vector<pugi::xml_node> nodes;
	for (const std::unique_ptr<xml::XmlDocument>& document : documents)
		for (pugi::xml_node child : document->root().children(name))
			nodes.push_back(child);
	return nodes;
}

/** 20 distinct indexes in [0, size), always the same ones */
std::vector<size_t> sample(size_t size, uint32_t seed) {
	std::mt19937 generator(seed);
	std::set<size_t> chosen;
	while (chosen.size() < std::min<size_t>(20, size))
		chosen.insert(std::uniform_int_distribution<size_t>(0, size - 1)(generator));
	return {chosen.begin(), chosen.end()};
}

bool zoneNamesArePorted() {
	runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
	try {
		::aion::gameserver::world::zone::ZoneName::createOrGet("P4_07A_REAL_DATA");
		return true;
	} catch (const runtime::UnportedException&) {
		return false;
	}
}

class TemplatesRealDataTest : public testing::Test {
protected:
	static void SetUpTestSuite() {
		if (!std::filesystem::exists(STATIC_DATA / "static_data.xml") || !std::filesystem::exists(ORACLE / "totals.json"))
			return;
		imports = xml::StaticDataImports::resolve(STATIC_DATA / "static_data.xml", 0, true);
		counts = readJson(ORACLE / "static_data_counts.json");
		totals = readJson(ORACLE / "totals.json");
		available = true;
	}

	static void TearDownTestSuite() {
		imports.clear();
		counts = json();
		totals = json();
	}

	void SetUp() override {
		if (!available)
			GTEST_SKIP() << "Java data tree or oracle outputs not found: " << STATIC_DATA << ", " << ORACLE;
	}

	static const xml::StaticDataImport& findImport(std::string_view file) {
		for (const xml::StaticDataImport& entry : imports) {
			if (entry.file == file)
				return entry;
		}
		throw std::runtime_error("no import " + std::string(file));
	}

	/** parses the files of an import (in binding order) */
	static Documents parse(std::string_view file) { return xml::StaticDataLoader::parseFiles(findImport(file).files, false); }

	/** binds parsed documents into a new test holder, strictly and with hooks (the loader's bindHolder path) */
	template <class H>
	static std::unique_ptr<H> bindHolder(xml::LoadContext& context, const Documents& documents) {
		std::vector<const xml::XmlDocument*> roots;
		for (const std::unique_ptr<xml::XmlDocument>& document : documents)
			roots.push_back(document.get());
		auto holder = std::make_unique<H>();
		xml::BindContext binding(context);
		binding.bindHolder(*holder, roots, context.root());
		return holder;
	}

	/** the oracle's value of the "Loaded N ..." line of exactly one holder */
	static int64_t expectedCount(std::string_view holder) {
		for (const json& line : counts.at("lines")) {
			const json& holders = line.at("holders");
			if (holders.size() == 1 && holders[0].get<std::string>() == holder)
				return line.at("values").at(0).get<int64_t>();
		}
		throw std::runtime_error("no count line for " + std::string(holder));
	}

	/** a value of the first oracle line containing `wording` (lines of two holders list them sorted, not in value order) */
	static int64_t expectedLineValue(std::string_view wording, size_t value) {
		for (const json& line : counts.at("lines")) {
			if (line.at("line").get<std::string>().find(wording) != std::string::npos)
				return line.at("values").at(value).get<int64_t>();
		}
		throw std::runtime_error("no count line with " + std::string(wording));
	}

	/** element and attribute totals of imports (totals.json byImport) */
	static std::pair<uint64_t, uint64_t> expectedTotals(std::initializer_list<std::string_view> files) {
		uint64_t elements = 0;
		uint64_t attributes = 0;
		for (std::string_view file : files) {
			bool found = false;
			for (const json& entry : totals.at("byImport")) {
				if (entry.at("import").get<std::string>() == file) {
					elements += entry.at("elements").get<uint64_t>();
					attributes += entry.at("attributes").get<uint64_t>();
					found = true;
				}
			}
			if (!found)
				throw std::runtime_error("no totals for " + std::string(file));
		}
		return {elements, attributes};
	}

	static void expectTotals(const xml::LoadContext& context, std::initializer_list<std::string_view> files) {
		auto [elements, attributes] = expectedTotals(files);
		const xml::BindStats& stats = context.stats();
		EXPECT_EQ(stats.totalElements().bound + stats.totalElements().ignored, elements);
		EXPECT_EQ(stats.totalElements().unknown, 0u);
		EXPECT_EQ(stats.totalAttributes().bound + stats.totalAttributes().ignored, attributes);
		EXPECT_EQ(stats.totalAttributes().unknown, 0u);
	}

	static xml::LoadOptions options() {
		xml::LoadOptions loadOptions;
		loadOptions.strict = true;
		loadOptions.collectStats = true;
		loadOptions.parallelParse = false;
		return loadOptions;
	}

	static inline bool available = false;
	static inline std::vector<xml::StaticDataImport> imports;
	static inline json counts;
	static inline json totals;
};

// ---- items -------------------------------------------------------------------------------------------------------------------------------------

TEST_F(TemplatesRealDataTest, ItemsAndDecomposableItemsBindWithHooks) {
	auto start = std::chrono::steady_clock::now();
	auto context = std::make_unique<xml::LoadContext>(options());
	Documents itemDocuments = parse("items/item_templates.xml");
	std::unique_ptr<Items> items;
	size_t removedActions = 0;
	try {
		items = bindHolder<Items>(*context, itemDocuments);
	} catch (const runtime::UnportedException& e) {
		// the item actions (P5-07) have unported hooks: bind again without the <actions> elements
		std::cout << "item actions have unported hooks (" << e.what() << "): removing the <actions> elements\n";
		context = std::make_unique<xml::LoadContext>(options());
		for (pugi::xml_node item : children(itemDocuments, "item_template")) {
			while (pugi::xml_node actions = item.child("actions")) {
				item.remove_child(actions);
				++removedActions;
			}
		}
		std::cout << "removed " << removedActions << " item actions elements\n";
		items = bindHolder<Items>(*context, itemDocuments);
	}
	Documents decomposableDocuments = parse("decomposable_items/decomposable_items.xml");
	std::unique_ptr<Decomposables> decomposables = bindHolder<Decomposables>(*context, decomposableDocuments); // ResultedItem hooks: item ids
	std::cout << "real items and decomposable items bound in "
			  << std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count() << " ms\n";

	EXPECT_EQ(static_cast<int64_t>(items->list.size()), expectedCount("item_templates")) << "Loaded N item templates";
	std::set<int32_t> decomposableIds;
	size_t resultedItems = 0;
	for (const item::DecomposableItemInfo& info : decomposables->list) {
		if (!info.isIsSelectable() && !info.getItemsCollections().empty()) // Java DecomposableItemsData.size(): non-selectable entries with items
			decomposableIds.insert(info.getItemId());
		for (const item::ExtractedItemsCollection& collection : info.getItemsCollections()) {
			for (const item::ResultedItem& resulted : collection.getItems()) {
				EXPECT_GE(resulted.getMaxCount(), resulted.getMinCount()) << "hook: max_count defaults to min_count";
				++resultedItems;
			}
		}
	}
	EXPECT_EQ(static_cast<int64_t>(decomposableIds.size()), expectedCount("decomposable_items")) << "Loaded N decomposable items entries";
	EXPECT_GT(resultedItems, 0u);
	if (removedActions == 0)
		expectTotals(*context, {"items/item_templates.xml", "decomposable_items/decomposable_items.xml"});

	// hooks on every item: empty weapon stats and use limits, tune count rule (recomputed from the DOM)
	std::vector<pugi::xml_node> nodes = children(itemDocuments, "item_template");
	ASSERT_EQ(nodes.size(), items->list.size());
	size_t stigmas = 0;
	for (size_t i = 0; i < nodes.size(); ++i) {
		const item::ItemTemplate& item = items->list[i];
		ASSERT_NE(item.getWeaponStats(), nullptr);
		ASSERT_NE(item.getUseLimits(), nullptr);
		int64_t slots = item::enums::getValidEquipmentSlots(item.getItemGroup());
		int64_t expectedTune = number(nodes[i], "rnd_count", -1);
		if (slots == 0)
			expectedTune = 0;
		else if (expectedTune == -1 && number(nodes[i], "max_enchant_bonus") == 0 && number(nodes[i], "option_slot_bonus") == 0 &&
			number(nodes[i], "rnd_bonus") == 0)
			expectedTune = 0;
		EXPECT_EQ(item.getMaxTuneCount(), expectedTune) << item.getTemplateId();
		if (item.getStigma() != nullptr) {
			++stigmas;
			const auto& groups = item.getStigma()->getGainSkillGroups();
			pugi::xml_node stigma = nodes[i].child("stigma");
			ASSERT_FALSE(groups.empty());
			EXPECT_EQ(groups[0], text(stigma, "gain_skill_group1"));
			EXPECT_EQ(groups.size(), stigma.attribute("gain_skill_group2") ? 2u : 1u);
		}
	}
	EXPECT_GT(stigmas, 0u);

	// field dump of 20 random items
	for (size_t index : sample(nodes.size(), 407)) {
		pugi::xml_node node = nodes[index];
		const item::ItemTemplate& item = items->list[index];
		SCOPED_TRACE("item_template id=" + text(node, "id"));
		EXPECT_EQ(item.getTemplateId(), number(node, "id"));
		EXPECT_EQ(item.getName(), text(node, "name"));
		EXPECT_EQ(item.getLevel(), number(node, "level"));
		EXPECT_EQ(item.getMask(), number(node, "mask"));
		EXPECT_EQ(item.getPrice(), number(node, "price"));
		if (!item.isKinah())
			EXPECT_EQ(item.getMaxStackCount(), number(node, "max_stack_count", 1));
		EXPECT_EQ(xml::enumName(item.getItemGroup()), text(node, "item_group", "NONE"));
		EXPECT_EQ(item.getItemQuality() ? std::string(xml::enumName(*item.getItemQuality())) : std::string(), text(node, "quality"));
		EXPECT_EQ(xml::enumName(item.getRace()), text(node, "race", "PC_ALL"));
		EXPECT_EQ(xml::enumName(item.getItemType()), text(node, "item_type", "NORMAL"));
		EXPECT_EQ(item.getL10nId(), number(node, "desc"));
		EXPECT_EQ(item.getManastoneSlots(), number(node, "m_slots"));
		EXPECT_EQ(item.getMaxEnchantLevel(), number(node, "max_enchant"));
		EXPECT_EQ(item.getExpireTime(), number(node, "expire_time"));
		EXPECT_FLOAT_EQ(item.getAttackGap(), decimal(node, "attack_gap"));
		std::vector<int64_t> restrictions(17, 1);
		if (node.attribute("restrict")) {
			restrictions.clear();
			std::string restrict = text(node, "restrict");
			for (size_t pos = 0; pos <= restrict.size();) {
				size_t end = std::min(restrict.find(' ', pos), restrict.size());
				restrictions.push_back(std::stoll(restrict.substr(pos, end - pos)));
				pos = end + 1;
			}
		}
		for (size_t playerClass = 0; playerClass < restrictions.size() && playerClass < 17; ++playerClass)
			EXPECT_EQ(item.getRequiredLevel(static_cast<PlayerClass>(playerClass)), restrictions[playerClass] == 0 ? -1 : restrictions[playerClass]);
		pugi::xml_node weapon = node.child("weapon_stats");
		EXPECT_EQ(item.getWeaponStats()->getMinDamage(), number(weapon, "min_damage"));
		EXPECT_EQ(item.getWeaponStats()->getMaxDamage(), number(weapon, "max_damage"));
		EXPECT_EQ(item.getWeaponStats()->getAttackSpeed(), number(weapon, "attack_speed"));
		pugi::xml_node limits = node.child("uselimits");
		EXPECT_EQ(item.getUseLimits()->getDelayTime(), number(limits, "usedelay"));
		EXPECT_EQ(item.getUseLimits()->getMaxRank(), number(limits, "rank_max", 18));
		EXPECT_EQ(item.isStigma(), static_cast<bool>(node.child("stigma")));
		EXPECT_EQ(item.getActions() != nullptr, static_cast<bool>(node.child("actions")));
		EXPECT_EQ(item.getModifiers() != nullptr, static_cast<bool>(node.child("modifiers")));
	}
}

// ---- npcs --------------------------------------------------------------------------------------------------------------------------------------

TEST_F(TemplatesRealDataTest, NpcsBindWithHooks) {
	Documents documents = parse("npcs");
	// NpcEquippedGear::init (P4-13) runs while an <equipment> element is bound: remove the elements while it is unported (their IDREFs need items)
	size_t removedEquipment = 0;
	{
		xml::LoadContext probe;
		try {
			xml::bindString<npc::NpcTemplate>(probe,
				R"(<npc_template npc_id="1" level="1" name_id="1"><equipment><item>1</item></equipment></npc_template>)");
		} catch (const runtime::UnportedException&) {
			for (const std::unique_ptr<xml::XmlDocument>& document : documents) {
				for (pugi::xml_node npc : document->root().children("npc_template")) {
					while (pugi::xml_node equipment = npc.child("equipment")) {
						npc.remove_child(equipment);
						++removedEquipment;
					}
				}
			}
			std::cout << "NpcEquippedGear::init is unported: removed " << removedEquipment << " npc equipment elements\n";
		} catch (const std::exception&) {
			// ported: the probe's item reference is simply unresolved here
		}
	}
	auto start = std::chrono::steady_clock::now();
	xml::LoadContext context(options());
	std::unique_ptr<Npcs> npcs = bindHolder<Npcs>(context, documents);
	std::cout << "real npcs bound in " << std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count()
			  << " ms\n";
	EXPECT_EQ(static_cast<int64_t>(npcs->list.size()), expectedCount("npc_templates")) << "Loaded N npc templates";
	if (removedEquipment == 0)
		expectTotals(context, {"npcs"});

	std::vector<pugi::xml_node> nodes = children(documents, "npc_template");
	ASSERT_EQ(nodes.size(), npcs->list.size());
	size_t siegeTeleporters = 0;
	for (size_t i = 0; i < nodes.size(); ++i) {
		std::string ai = text(nodes[i], "ai");
		if (number(nodes[i], "level") > 1 && ai != "noaction" && text(nodes[i], "abyss_type") == "TELEPORTER") {
			ai = "siege_teleporter";
			++siegeTeleporters;
		}
		EXPECT_EQ(npcs->list[i].getAiName().value_or(""), ai) << npcs->list[i].getTemplateId();
	}
	std::cout << siegeTeleporters << " npcs became siege teleporters\n";

	for (size_t index : sample(nodes.size(), 4071)) {
		pugi::xml_node node = nodes[index];
		const npc::NpcTemplate& npc = npcs->list[index];
		SCOPED_TRACE("npc_template npc_id=" + text(node, "npc_id"));
		EXPECT_EQ(npc.getTemplateId(), number(node, "npc_id"));
		EXPECT_EQ(npc.getLevel(), number(node, "level"));
		EXPECT_EQ(npc.getL10nId(), number(node, "name_id"));
		EXPECT_EQ(npc.getTitleId(), number(node, "title_id"));
		EXPECT_EQ(npc.getName(), text(node, "name"));
		EXPECT_EQ(npc.getAggroRange(), number(node, "srange"));
		EXPECT_EQ(npc.getAttackRange(), number(node, "arange"));
		EXPECT_EQ(npc.getAttackSpeed(), number(node, "attack_speed", 2000));
		EXPECT_FLOAT_EQ(npc.getHeight(), decimal(node, "height", 1.0f));
		EXPECT_EQ(xml::enumName(npc.getRace()), text(node, "race", "NONE"));
		EXPECT_EQ(npc.getRank() ? std::string(xml::enumName(*npc.getRank())) : std::string(), text(node, "rank"));
		EXPECT_EQ(npc.getRating() ? std::string(xml::enumName(*npc.getRating())) : std::string(), text(node, "rating"));
		EXPECT_EQ(npc.getTribe() ? std::string(xml::enumName(*npc.getTribe())) : std::string(), text(node, "tribe"));
		EXPECT_EQ(xml::enumName(npc.getAbyssNpcType()), text(node, "abyss_type", "NONE"));
		EXPECT_EQ(xml::enumName(npc.getNpcTemplateType()), text(node, "type", "NONE"));
		pugi::xml_node stats = node.child("stats");
		ASSERT_EQ(npc.getStatsTemplate() != nullptr, static_cast<bool>(stats));
		if (stats) {
			EXPECT_EQ(npc.getStatsTemplate()->getMaxHp(), number(stats, "maxHp"));
			pugi::xml_node speeds = stats.child("speeds");
			EXPECT_FLOAT_EQ(npc.getStatsTemplate()->getRunSpeed(), decimal(speeds, "run"));
		}
		pugi::xml_node bound = node.child("bound_radius");
		if (bound)
			EXPECT_FLOAT_EQ(npc.getBoundRadius()->getFront(), decimal(bound, "front"));
		EXPECT_EQ(npc.getTalkDistance(), number(node.child("talk_info"), "distance", 2));
		EXPECT_EQ(npc.canInteract(), static_cast<bool>(node.child("talk_info")));
	}
}

// ---- spawns ------------------------------------------------------------------------------------------------------------------------------------

TEST_F(TemplatesRealDataTest, SpawnsBindWithHooks) {
	auto start = std::chrono::steady_clock::now();
	xml::LoadContext context(options());
	Documents documents = parse("spawns");
	std::unique_ptr<SpawnMaps> maps = bindHolder<SpawnMaps>(context, documents);
	std::cout << "real spawns bound in " << std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count()
			  << " ms\n";
	std::set<int32_t> mapIds;
	size_t spots = 0;
	size_t temporarySpawns = 0;
	for (const spawns::SpawnMap& map : maps->list) {
		mapIds.insert(map.getMapId());
		for (const std::unique_ptr<spawns::Spawn>& spawn : map.getSpawns()) {
			spots += static_cast<const spawns::Spawn&>(*spawn).getSpawnSpotTemplates().size();
			temporarySpawns += spawn->getTemporarySpawn() != nullptr ? 1 : 0;
		}
	}
	EXPECT_EQ(static_cast<int64_t>(mapIds.size()), expectedCount("spawns")) << "Loaded N spawn maps entries (distinct map ids)";
	expectTotals(context, {"spawns"});
	EXPECT_GT(spots, 0u);

	// every temporary spawn of a <spawn> parsed its expressions: the spawn check at the time given by its own spawn expression holds
	size_t checked = 0;
	std::vector<pugi::xml_node> mapNodes = children(documents, "spawn_map");
	ASSERT_EQ(mapNodes.size(), maps->list.size());
	for (size_t m = 0; m < mapNodes.size(); ++m) {
		size_t s = 0;
		for (pugi::xml_node spawnNode : mapNodes[m].children("spawn")) {
			const spawns::Spawn& spawn = *maps->list[m].getSpawns()[s++];
			pugi::xml_node temporary = spawnNode.child("temporary_spawn");
			ASSERT_EQ(spawn.getTemporarySpawn() != nullptr, static_cast<bool>(temporary));
			std::string spawnTime = text(temporary, "spawn_time");
			if (!temporary || spawnTime.empty() || temporary.attribute("weekdays"))
				continue;
			// hour.day.month; "*" or "/n" parts are left at 0, which every "/n" expression matches (0 % n == 0)
			auto part = [&](size_t index) {
				size_t begin = 0;
				for (size_t i = 0; i < index; ++i)
					begin = spawnTime.find('.', begin) + 1;
				std::string value = spawnTime.substr(begin, spawnTime.find('.', begin) - begin);
				return value == "*" || value.starts_with("/") ? 0 : std::stoi(value);
			};
			EXPECT_TRUE(spawn.getTemporarySpawn()->canSpawn(java::time::DayOfWeek::MONDAY, {part(0), part(1), part(2)})) << spawnTime;
			++checked;
		}
	}
	EXPECT_GT(checked, 0u);
	std::cout << temporarySpawns << " temporary spawns, " << checked << " spawn expressions checked\n";
}

// ---- world maps, weather, pets -----------------------------------------------------------------------------------------------------------------

TEST_F(TemplatesRealDataTest, WorldMapsWeatherAndPetsBindWithHooks) {
	xml::LoadContext context(options());
	Documents mapDocuments = parse("world_maps.xml");
	std::unique_ptr<WorldMaps> maps = bindHolder<WorldMaps>(context, mapDocuments);
	std::unique_ptr<Weather> weather = bindHolder<Weather>(context, parse("weather_table.xml"));
	std::unique_ptr<Pets> pets = bindHolder<Pets>(context, parse("pets/pets.xml"));
	std::unique_ptr<PetFeed> feed = bindHolder<PetFeed>(context, parse("pets/pet_feed.xml"));
	EXPECT_EQ(static_cast<int64_t>(maps->list.size()), expectedCount("world_maps"));
	std::set<int32_t> weatherMaps;
	for (const world::WeatherTable& table : weather->list)
		weatherMaps.insert(table.getMapId());
	EXPECT_EQ(static_cast<int64_t>(weatherMaps.size()), expectedCount("weather"));
	EXPECT_EQ(static_cast<int64_t>(pets->list.size()), expectedCount("pets"));
	EXPECT_EQ(static_cast<int64_t>(feed->list.size()), expectedLineValue(" pet templates and ", 1)) << "N food flavours";
	expectTotals(context, {"world_maps.xml", "weather_table.xml", "pets/pets.xml", "pets/pet_feed.xml"});

	// the flags hook against the DOM: the id of a zone attribute is 1 << ordinal; the lexical form is the @XmlEnumValue
	std::vector<pugi::xml_node> nodes = children(mapDocuments, "map");
	ASSERT_EQ(nodes.size(), maps->list.size());
	for (size_t i = 0; i < nodes.size(); ++i) {
		int32_t expected = 0;
		std::string flags = text(nodes[i], "flags");
		for (size_t pos = 0; pos < flags.size();) {
			size_t end = std::min(flags.find(' ', pos), flags.size());
			std::optional<::aion::gameserver::world::zone::ZoneAttributes> attribute =
				xml::enumFromXml<::aion::gameserver::world::zone::ZoneAttributes>(std::string_view(flags).substr(pos, end - pos));
			ASSERT_TRUE(attribute.has_value()) << flags;
			expected |= 1 << static_cast<int32_t>(*attribute);
			pos = end + 1;
		}
		EXPECT_EQ(maps->list[i].getFlags(), expected) << maps->list[i].getMapId();
		EXPECT_EQ(maps->list[i].getName(), text(nodes[i], "name", text(nodes[i], "cName").c_str()));
	}
	size_t petsWithEmptyFunction = 0;
	for (const pet::PetTemplate& pet : pets->list) {
		std::vector<const pet::PetFunction*> functions = pet.getPetFunctions();
		ASSERT_FALSE(functions.empty());
		petsWithEmptyFunction += functions.back()->getPetFunctionType() == pet::PetFunctionType::NONE ? 1 : 0;
	}
	std::cout << petsWithEmptyFunction << " of " << pets->list.size() << " pets have the appended empty function\n";
}

// ---- zones -------------------------------------------------------------------------------------------------------------------------------------

TEST_F(TemplatesRealDataTest, ZonesBindWithTheNameSetter) {
	if (!zoneNamesArePorted())
		GTEST_SKIP() << "ZoneName::createOrGet (P4-10) is not ported yet";
	runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
	xml::LoadContext context(options());
	Documents documents = parse("zones");
	std::unique_ptr<Zones> zones = bindHolder<Zones>(context, documents);
	EXPECT_EQ(static_cast<int64_t>(zones->list.size()), expectedCount("zones"));
	expectTotals(context, {"zones"});
	std::vector<pugi::xml_node> nodes = children(documents, "zone");
	ASSERT_EQ(nodes.size(), zones->list.size());
	for (size_t index : sample(nodes.size(), 3978)) {
		ASSERT_NE(zones->list[index].getName(), nullptr);
		EXPECT_EQ(zones->list[index].getMapid(), number(nodes[index], "mapid"));
	}
}

} // namespace
} // namespace aion::gameserver::model::templates::realdata
