// P4-07b on the real static data (docs/design/handlers-and-porting-plan.md §3.2): the imports whose templates live in the P4-07b packages are bound
// through the xml runtime with hooks (strict mode) into test holders of this file (the real holders and most of their hooks belong to P4-09;
// holders without a hook, and Mails, which is a P4-07b class, are bound directly). Checks:
// - the holder counts of the count oracle (tools/oracle/expected/static_data_counts.json), recomputed from the bound templates with each Java
//   holder's size() rule, and the element/attribute totals per import (totals.json byImport, bound plus deliberately ignored);
// - hook results against values recomputed from the DOM (mail lookups, walker steps and rows, door and town lookups, building parts, house
//   addresses, assault waves, goods list item ids, bonus group entries);
// - a field dump of 20 random quests against their XML attributes.
// Skips: the item <actions> elements are removed from the DOM while item action hooks (P5-07) are unported, the event <spawns> elements while
// the SpawnsData hook (P4-09) is unported (the totals are then compared only for the imports without removals).

#include <algorithm>
#include <charconv>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <map>
#include <memory>
#include <optional>
#include <random>
#include <set>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <typeinfo>
#include <utility>
#include <vector>

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>
#include <pugixml.hpp>

#include "aion/commons/utils/StringUtils.h"
#include "aion/gameserver/dataholders/GlobalDropData.bind.h"
#include "aion/gameserver/dataholders/LegionDominionData.bind.h"
#include "aion/gameserver/dataholders/loadingutils/BindContext.h"
#include "aion/gameserver/dataholders/loadingutils/StaticDataImports.h"
#include "aion/gameserver/dataholders/loadingutils/StaticDataLoader.h"
#include "aion/gameserver/model/limiteditems/LimitedItem.h"
#include "aion/gameserver/model/siege/Assaulter.h"
#include "aion/gameserver/model/templates/QuestTemplate.bind.h"
#include "aion/gameserver/model/templates/StorageExpansionTemplate.bind.h"
#include "aion/gameserver/model/templates/TitleTemplate.bind.h"
#include "aion/gameserver/model/templates/ai/AITemplate.bind.h"
#include "aion/gameserver/model/templates/challenge/ChallengeTaskTemplate.bind.h"
#include "aion/gameserver/model/templates/cp/CPRank.bind.h"
#include "aion/gameserver/model/templates/event/EventTemplate.bind.h"
#include "aion/gameserver/model/templates/event/upgradearcade/ArcadeLevels.bind.h"
#include "aion/gameserver/model/templates/event/upgradearcade/ArcadeRewards.bind.h"
#include "aion/gameserver/model/templates/factions/NpcFactionTemplate.bind.h"
#include "aion/gameserver/model/templates/flypath/FlyPathEntry.bind.h"
#include "aion/gameserver/model/templates/flyring/FlyRingTemplate.bind.h"
#include "aion/gameserver/model/templates/gather/GatherableTemplate.bind.h"
#include "aion/gameserver/model/templates/goods/GoodsList.bind.h"
#include "aion/gameserver/model/templates/housing/Building.bind.h"
#include "aion/gameserver/model/templates/housing/HousePart.bind.h"
#include "aion/gameserver/model/templates/housing/HousingLand.bind.h"
#include "aion/gameserver/model/templates/instance_bonusatrr/InstanceBonusAttr.bind.h"
#include "aion/gameserver/model/templates/item/ItemTemplate.bind.h"
#include "aion/gameserver/model/templates/itemgroups/BossGroup.bind.h"
#include "aion/gameserver/model/templates/itemgroups/CraftItemGroup.bind.h"
#include "aion/gameserver/model/templates/itemgroups/CraftRecipeGroup.bind.h"
#include "aion/gameserver/model/templates/itemgroups/EnchantGroup.bind.h"
#include "aion/gameserver/model/templates/itemgroups/EventGroup.bind.h"
#include "aion/gameserver/model/templates/itemgroups/FeedGroups.bind.h"
#include "aion/gameserver/model/templates/itemgroups/FoodGroup.bind.h"
#include "aion/gameserver/model/templates/itemgroups/GatherGroup.bind.h"
#include "aion/gameserver/model/templates/itemgroups/ManastoneGroup.bind.h"
#include "aion/gameserver/model/templates/itemgroups/MedalGroup.bind.h"
#include "aion/gameserver/model/templates/itemgroups/MedicineGroup.bind.h"
#include "aion/gameserver/model/templates/itemgroups/OreGroup.bind.h"
#include "aion/gameserver/model/templates/itemset/ItemSetTemplate.bind.h"
#include "aion/gameserver/model/templates/mail/Mails.bind.h"
#include "aion/gameserver/model/templates/materials/MaterialTemplate.bind.h"
#include "aion/gameserver/model/templates/npcshout/ShoutGroup.bind.h"
#include "aion/gameserver/model/templates/panels/SkillPanel.bind.h"
#include "aion/gameserver/model/templates/portal/InstanceExit.bind.h"
#include "aion/gameserver/model/templates/recipe/RecipeTemplate.bind.h"
#include "aion/gameserver/model/templates/ride/RideInfo.bind.h"
#include "aion/gameserver/model/templates/siegelocation/SiegeLocationTemplate.bind.h"
#include "aion/gameserver/model/templates/staticdoor/StaticDoorWorld.bind.h"
#include "aion/gameserver/model/templates/teleport/TelelocationTemplate.bind.h"
#include "aion/gameserver/model/templates/teleport/TeleporterTemplate.bind.h"
#include "aion/gameserver/model/templates/towns/TownSpawnMap.bind.h"
#include "aion/gameserver/model/templates/tradelist/TradeListTemplate.bind.h"
#include "aion/gameserver/model/templates/tribe/Tribe.bind.h"
#include "aion/gameserver/model/templates/walker/RouteParent.bind.h"
#include "aion/gameserver/model/templates/walker/WalkerTemplate.bind.h"
#include "aion/gameserver/model/templates/windstreams/WindstreamTemplate.bind.h"
#include "aion/gameserver/model/templates/worldraid/WorldRaidLocation.bind.h"
#include "aion/gameserver/runtime/base/TaskInfo.h"
#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/runtime/lifetime/TaskScope.h"

namespace aion::gameserver::model::templates::realdatab {

// ---- test holders: one list (or single element) per child element name, like the Java holders' bound fields ------------------------------------

/** a child element of a test holder: bound into a std::vector<T> (or a std::unique_ptr<T> for a single element) */
template <class T, const char* NAME, bool SINGLE = false>
struct Child {
	using Type = T;
	using Storage = std::conditional_t<SINGLE, std::unique_ptr<T>, std::vector<T>>;
	static constexpr std::string_view name = NAME;
	static constexpr bool single = SINGLE;
};

template <class... C>
struct Holder {
	std::tuple<typename C::Storage...> children;

	template <size_t I>
	auto& get() {
		return std::get<I>(children);
	}
};

} // namespace aion::gameserver::model::templates::realdatab

namespace aion::gameserver::xml {

template <class... C>
struct XmlBinding<::aion::gameserver::model::templates::realdatab::Holder<C...>> {
	using H = ::aion::gameserver::model::templates::realdatab::Holder<C...>;
	static constexpr std::string_view CLASS_NAME = "TestHolder";

	static bool element(H& o, BindContext& c, pugi::xml_node e, std::string_view name) { return bindAt<0>(o, c, e, name); }

	static void reserve(H& o, const ChildCounts& counts) { reserveAt<0>(o, counts); }

private:
	template <size_t I>
	static bool bindAt(H& o, BindContext& c, pugi::xml_node e, std::string_view name) {
		if constexpr (I == sizeof...(C)) {
			return false;
		} else {
			using Entry = std::tuple_element_t<I, std::tuple<C...>>;
			if (name != Entry::name)
				return bindAt<I + 1>(o, c, e, name);
			if constexpr (Entry::single)
				c.bindSingle(std::get<I>(o.children), e);
			else
				c.bindList(std::get<I>(o.children), e);
			return true;
		}
	}

	template <size_t I>
	static void reserveAt(H& o, const ChildCounts& counts) {
		if constexpr (I < sizeof...(C)) {
			using Entry = std::tuple_element_t<I, std::tuple<C...>>;
			if constexpr (!Entry::single)
				std::get<I>(o.children).reserve(counts[Entry::name]);
			reserveAt<I + 1>(o, counts);
		}
	}
};

} // namespace aion::gameserver::xml

namespace aion::gameserver::model::templates::realdatab {
namespace {

using json = nlohmann::json;
using Documents = std::vector<std::unique_ptr<xml::XmlDocument>>;

// element names (the Child NTTP needs objects with linkage)
inline constexpr char SHOUT_GROUP[] = "shout_group";
inline constexpr char WALKER_TEMPLATE[] = "walker_template";
inline constexpr char WORLD[] = "world";
inline constexpr char BUILDING[] = "building";
inline constexpr char LAND[] = "land";
inline constexpr char HOUSE_PART[] = "house_part";
inline constexpr char SPAWN_MAP[] = "spawn_map";
inline constexpr char SIEGE_LOCATION[] = "siege_location";
inline constexpr char QUEST[] = "quest";
inline constexpr char TRIBE[] = "tribe";
inline constexpr char TRADELIST_TEMPLATE[] = "tradelist_template";
inline constexpr char TRADE_IN_LIST_TEMPLATE[] = "trade_in_list_template";
inline constexpr char PURCHASE_TEMPLATE[] = "purchase_template";
inline constexpr char LIST[] = "list";
inline constexpr char IN_LIST[] = "in_list";
inline constexpr char PURCHASE_LIST[] = "purchase_list";
inline constexpr char AI[] = "ai";
inline constexpr char PANEL[] = "panel";
inline constexpr char RECIPE_TEMPLATE[] = "recipe_template";
inline constexpr char TITLE[] = "title";
inline constexpr char GATHERABLE_TEMPLATE[] = "gatherable_template";
inline constexpr char NPC_FACTION[] = "npc_faction";
inline constexpr char TELEPORTER_TEMPLATE[] = "teleporter_template";
inline constexpr char TELELOC_TEMPLATE[] = "teleloc_template";
inline constexpr char INSTANCE_BONUSATTR[] = "instance_bonusattr";
inline constexpr char RIDE_INFO[] = "ride_info";
inline constexpr char INSTANCE_EXIT[] = "instance_exit";
inline constexpr char WORLD_RAID_LOCATION[] = "world_raid_location";
inline constexpr char TASK[] = "task";
inline constexpr char RANK[] = "rank";
inline constexpr char WINDSTREAM[] = "windstream";
inline constexpr char FLYPATH_LOCATION[] = "flypath_location";
inline constexpr char FLY_RING[] = "fly_ring";
inline constexpr char MATERIAL[] = "material";
inline constexpr char LEVELS[] = "levels";
inline constexpr char REWARDS[] = "rewards";
inline constexpr char EXPANSION_NPC[] = "expansion_npc";
inline constexpr char WALK_PARENT[] = "walk_parent";
inline constexpr char ITEM_TEMPLATE[] = "item_template";
inline constexpr char ITEMSET[] = "itemset";
inline constexpr char EVENT[] = "event";

using ShoutGroups = Holder<Child<npcshout::ShoutGroup, SHOUT_GROUP>>;
using Walkers = Holder<Child<walker::WalkerTemplate, WALKER_TEMPLATE>>;
using DoorWorlds = Holder<Child<staticdoor::StaticDoorWorld, WORLD>>;
using Buildings = Holder<Child<housing::Building, BUILDING>>;
using Lands = Holder<Child<housing::HousingLand, LAND>>;
using HouseParts = Holder<Child<housing::HousePart, HOUSE_PART>>;
using TownMaps = Holder<Child<towns::TownSpawnMap, SPAWN_MAP>>;
using SiegeLocations = Holder<Child<siegelocation::SiegeLocationTemplate, SIEGE_LOCATION>>;
using Quests = Holder<Child<QuestTemplate, QUEST>>;
using Tribes = Holder<Child<tribe::Tribe, TRIBE>>;
using TradeLists = Holder<Child<tradelist::TradeListTemplate, TRADELIST_TEMPLATE>, Child<tradelist::TradeListTemplate, TRADE_IN_LIST_TEMPLATE>,
                          Child<tradelist::TradeListTemplate, PURCHASE_TEMPLATE>>;
using GoodsLists = Holder<Child<goods::GoodsList, LIST>, Child<goods::GoodsList, IN_LIST>, Child<goods::GoodsList, PURCHASE_LIST>>;
using AiTemplates = Holder<Child<ai::AITemplate, AI>>;
using Panels = Holder<Child<panels::SkillPanel, PANEL>>;
using Recipes = Holder<Child<recipe::RecipeTemplate, RECIPE_TEMPLATE>>;
using Titles = Holder<Child<TitleTemplate, TITLE>>;
using Gatherables = Holder<Child<gather::GatherableTemplate, GATHERABLE_TEMPLATE>>;
using Factions = Holder<Child<factions::NpcFactionTemplate, NPC_FACTION>>;
using Teleporters = Holder<Child<teleport::TeleporterTemplate, TELEPORTER_TEMPLATE>>;
using TeleLocations = Holder<Child<teleport::TelelocationTemplate, TELELOC_TEMPLATE>>;
using InstanceBuffs = Holder<Child<instance_bonusatrr::InstanceBonusAttr, INSTANCE_BONUSATTR>>;
using Rides = Holder<Child<ride::RideInfo, RIDE_INFO>>;
using InstanceExits = Holder<Child<portal::InstanceExit, INSTANCE_EXIT>>;
using WorldRaids = Holder<Child<worldraid::WorldRaidLocation, WORLD_RAID_LOCATION>>;
using ChallengeTasks = Holder<Child<challenge::ChallengeTaskTemplate, TASK>>;
using Ranks = Holder<Child<cp::CPRank, RANK>>;
using Windstreams = Holder<Child<windstreams::WindstreamTemplate, WINDSTREAM>>;
using FlyPaths = Holder<Child<flypath::FlyPathEntry, FLYPATH_LOCATION>>;
using FlyRings = Holder<Child<flyring::FlyRingTemplate, FLY_RING>>;
using Materials = Holder<Child<materials::MaterialTemplate, MATERIAL>>;
using Arcade = Holder<Child<event::upgradearcade::ArcadeLevels, LEVELS, true>, Child<event::upgradearcade::ArcadeRewards, REWARDS>>;
using Expansions = Holder<Child<StorageExpansionTemplate, EXPANSION_NPC>>;
using WalkerVersions = Holder<Child<walker::RouteParent, WALK_PARENT>>;
using Items = Holder<Child<item::ItemTemplate, ITEM_TEMPLATE>>;
using ItemSets = Holder<Child<itemset::ItemSetTemplate, ITEMSET>>;
using Events = Holder<Child<event::EventTemplate, EVENT>>;

// the 38 single groups of ItemGroupsData, in field order
inline constexpr char G0[] = "craft_materials";
inline constexpr char G1[] = "craft_shop";
inline constexpr char G2[] = "craft_bundles";
inline constexpr char G3[] = "craft_recipes";
inline constexpr char G4[] = "manastones_common";
inline constexpr char G5[] = "manastones_rare";
inline constexpr char G6[] = "medals";
inline constexpr char G7[] = "food";
inline constexpr char G8[] = "medicine_common";
inline constexpr char G9[] = "medicine_rare";
inline constexpr char G10[] = "medicine_legendary";
inline constexpr char G11[] = "ores_rare";
inline constexpr char G12[] = "ores_legendary";
inline constexpr char G13[] = "ores_unique";
inline constexpr char G14[] = "ores_epic";
inline constexpr char G15[] = "gather_rare";
inline constexpr char G16[] = "enchants";
inline constexpr char G17[] = "events";
inline constexpr char G18[] = "boss_rare";
inline constexpr char G19[] = "boss_legendary";
inline constexpr char F0[] = "feed_fluid";
inline constexpr char F1[] = "feed_armor";
inline constexpr char F2[] = "feed_thorn";
inline constexpr char F3[] = "feed_bone";
inline constexpr char F4[] = "feed_balaur_material";
inline constexpr char F5[] = "feed_soul";
inline constexpr char F6[] = "feed_exclude";
inline constexpr char F7[] = "stinking_junk";
inline constexpr char F8[] = "feed_healthy_all";
inline constexpr char F9[] = "feed_healthy_spicy";
inline constexpr char F10[] = "feed_powder_biscuit";
inline constexpr char F11[] = "feed_crystal_biscuit";
inline constexpr char F12[] = "feed_gem_biscuit";
inline constexpr char F13[] = "poppy_snack";
inline constexpr char F14[] = "tasty_poppy_snack";
inline constexpr char F15[] = "nutritious_poppy_snack";
inline constexpr char F16[] = "feed_shugo_event_coin";
inline constexpr char F17[] = "feed_aether_cherry";

using namespace itemgroups;
using ItemGroups =
  Holder<Child<CraftItemGroup, G0, true>, Child<CraftItemGroup, G1, true>, Child<CraftRecipeGroup, G2, true>, Child<CraftRecipeGroup, G3, true>,
         Child<ManastoneGroup, G4, true>, Child<ManastoneGroup, G5, true>, Child<MedalGroup, G6, true>, Child<FoodGroup, G7, true>,
         Child<MedicineGroup, G8, true>, Child<MedicineGroup, G9, true>, Child<MedicineGroup, G10, true>, Child<OreGroup, G11, true>,
         Child<OreGroup, G12, true>, Child<OreGroup, G13, true>, Child<OreGroup, G14, true>, Child<GatherGroup, G15, true>,
         Child<EnchantGroup, G16, true>, Child<EventGroup, G17, true>, Child<BossGroup, G18, true>, Child<BossGroup, G19, true>,
         Child<FeedGroups_FeedFluidGroup, F0, true>, Child<FeedGroups_FeedArmorGroup, F1, true>, Child<FeedGroups_FeedThornGroup, F2, true>,
         Child<FeedGroups_FeedBoneGroup, F3, true>, Child<FeedGroups_FeedBalaurGroup, F4, true>, Child<FeedGroups_FeedSoulGroup, F5, true>,
         Child<FeedGroups_FeedExcludeGroup, F6, true>, Child<FeedGroups_StinkingJunkGroup, F7, true>, Child<FeedGroups_HealthyFoodAllGroup, F8, true>,
         Child<FeedGroups_HealthyFoodSpicyGroup, F9, true>, Child<FeedGroups_AetherPowderBiscuitGroup, F10, true>,
         Child<FeedGroups_AetherCrystalBiscuitGroup, F11, true>, Child<FeedGroups_AetherGemBiscuitGroup, F12, true>,
         Child<FeedGroups_PoppySnackGroup, F13, true>, Child<FeedGroups_PoppySnackTastyGroup, F14, true>,
         Child<FeedGroups_PoppySnackNutritiousGroup, F15, true>, Child<FeedGroups_ShugoEventCoinGroup, F16, true>,
         Child<FeedGroups_AetherCherryGroup, F17, true>>;

const std::filesystem::path STATIC_DATA = std::filesystem::path(AION_GAMESERVER_JAVA_DIR) / "data/static_data";
const std::filesystem::path ORACLE = std::filesystem::path(AION_GAMESERVER_JAVA_DIR) / "../cpp/tools/oracle/expected";

json readJson(const std::filesystem::path& file) {
	std::ifstream in(file, std::ios::binary);
	if (!in)
		throw std::runtime_error("cannot open " + file.string());
	return json::parse(in);
}

// ---- DOM access (the independent side of the checks) --------------------------------------------------------------------------------------------

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

std::vector<int64_t> numbers(std::string_view list, char separator = ' ') {
	std::vector<int64_t> values;
	for (size_t pos = 0; pos < list.size();) {
		size_t end = std::min(list.find(separator, pos), list.size());
		if (end > pos) {
			int64_t value = 0;
			std::from_chars(list.data() + pos, list.data() + end, value);
			values.push_back(value);
		}
		pos = end + 1;
	}
	return values;
}

/** the children of all roots with the element name, in binding order */
std::vector<pugi::xml_node> children(const Documents& documents, const char* name) {
	std::vector<pugi::xml_node> nodes;
	for (const std::unique_ptr<xml::XmlDocument>& document : documents)
		for (pugi::xml_node child : document->root().children(name))
			nodes.push_back(child);
	return nodes;
}

size_t childCount(pugi::xml_node node, const char* name) {
	return static_cast<size_t>(std::distance(node.children(name).begin(), node.children(name).end()));
}

/** 20 distinct indexes in [0, size), always the same ones */
std::vector<size_t> sample(size_t size, uint32_t seed) {
	std::mt19937 generator(seed);
	std::set<size_t> chosen;
	while (chosen.size() < std::min<size_t>(20, size))
		chosen.insert(std::uniform_int_distribution<size_t>(0, size - 1)(generator));
	return {chosen.begin(), chosen.end()};
}

template <class T, class Key>
size_t distinctCount(const std::vector<T>& list, Key key) {
	std::set<decltype(key(list.front()))> keys;
	for (const T& entry : list)
		keys.insert(key(entry));
	return keys.size();
}

/**
 * ItemGroupsData.bonusSize() adds the item counts of the 19 bonus groups except medals (field index 6); every entry of the bonus groups is also
 * read through BonusItemGroup::getItems(), whose entries dispatch getCount/getChance on their entry class
 */
template <class Group>
void checkBonusGroup(const Group& group, size_t index, int64_t& bonusSize, size_t& checkedEntries) {
	if constexpr (std::is_base_of_v<BonusItemGroup, Group>) {
		if (index != 6)
			bonusSize += static_cast<int64_t>(group.getItems().size());
		std::vector<const ItemRaceEntry*> entries = static_cast<const BonusItemGroup&>(group).getItems();
		EXPECT_EQ(entries.size(), group.getItems().size());
		for (const ItemRaceEntry* entry : entries) {
			EXPECT_GT(entry->getCount(), 0);
			EXPECT_GE(entry->getChance(), 0.0f);
			++checkedEntries;
		}
	}
}

class TemplatesBRealDataTest : public testing::Test {
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

	static Documents parse(std::string_view file) { return xml::StaticDataLoader::parseFiles(findImport(file).files, false); }

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

	/** binds one import into a new context, compares its totals and returns the holder */
	template <class H>
	static std::unique_ptr<H> bindImport(std::string_view file, Documents& documents) {
		documents = parse(file);
		xml::LoadContext context(options());
		std::unique_ptr<H> holder = bindHolder<H>(context, documents);
		expectTotals(context, {file});
		return holder;
	}

	static int64_t expectedCount(std::string_view holder) {
		for (const json& line : counts.at("lines")) {
			const json& holders = line.at("holders");
			if (holders.size() == 1 && holders[0].get<std::string>() == holder)
				return line.at("values").at(0).get<int64_t>();
		}
		throw std::runtime_error("no count line for " + std::string(holder));
	}

	static int64_t expectedLineValue(std::string_view wording, size_t value) {
		for (const json& line : counts.at("lines")) {
			if (line.at("line").get<std::string>().find(wording) != std::string::npos)
				return line.at("values").at(value).get<int64_t>();
		}
		throw std::runtime_error("no count line with " + std::string(wording));
	}

	static void expectTotals(const xml::LoadContext& context, std::initializer_list<std::string_view> files) {
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
		const xml::BindStats& stats = context.stats();
		EXPECT_EQ(stats.totalElements().bound + stats.totalElements().ignored, elements) << "elements of the imports";
		EXPECT_EQ(stats.totalElements().unknown, 0u);
		EXPECT_EQ(stats.totalAttributes().bound + stats.totalAttributes().ignored, attributes) << "attributes of the imports";
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

// ---- mails, shouts, walkers, doors ---------------------------------------------------------------------------------------------------------------

TEST_F(TemplatesBRealDataTest, MailsShoutsWalkersAndDoors) {
	Documents mailDocuments;
	std::unique_ptr<mail::Mails> mails = bindImport<mail::Mails>("mail_templates.xml", mailDocuments);
	EXPECT_EQ(mails->size(), expectedCount("mails"));
	size_t mailTemplates = 0;
	for (pugi::xml_node mailNode : children(mailDocuments, "mail")) {
		std::string mailName = text(mailNode, "name");
		for (pugi::xml_node templateNode : mailNode.children("template")) {
			std::string eventName = commons::utils::StringUtils::toLowerCase(text(templateNode, "name"));
			std::optional<Race> race = xml::enumFromXml<Race>(text(templateNode, "race"));
			ASSERT_TRUE(race.has_value());
			const mail::MailTemplate* found = mails->getMailTemplate(mailName, text(templateNode, "name"), *race);
			ASSERT_NE(found, nullptr) << mailName << "/" << eventName;
			EXPECT_EQ(commons::utils::StringUtils::toLowerCase(found->getName()), eventName);
			EXPECT_TRUE(found->getRace() == *race || found->getRace() == Race::PC_ALL);
			if (found == nullptr || found->getTitle() == nullptr || !found->getTitle()->getId().has_value())
				continue;
			EXPECT_NO_THROW(found->getFormattedTitle(nullptr));
			++mailTemplates;
		}
	}
	EXPECT_GT(mailTemplates, 0u);

	Documents shoutDocuments;
	std::unique_ptr<ShoutGroups> shouts = bindImport<ShoutGroups>("npc_shouts/npc_shouts.xml", shoutDocuments);
	int64_t shoutCount = 0;
	for (const npcshout::ShoutGroup& group : shouts->get<0>())
		for (const npcshout::ShoutList& list : group.getShoutNpcs())
			shoutCount += static_cast<int64_t>(list.getNpcShouts().size());
	EXPECT_EQ(shoutCount, expectedCount("npc_shouts")) << "NpcShoutData count";

	Documents walkerDocuments;
	std::unique_ptr<Walkers> walkers = bindImport<Walkers>("npc_walker", walkerDocuments);
	const std::vector<walker::WalkerTemplate>& routes = walkers->get<0>();
	EXPECT_EQ(static_cast<int64_t>(distinctCount(routes, [](const walker::WalkerTemplate& w) { return w.getRouteId(); })), expectedCount("npc_walker"));
	std::vector<pugi::xml_node> walkerNodes = children(walkerDocuments, "walker_template");
	ASSERT_EQ(walkerNodes.size(), routes.size());
	size_t walkBack = 0;
	size_t squares = 0;
	for (size_t i = 0; i < routes.size(); ++i) {
		const walker::WalkerTemplate& route = routes[i];
		size_t steps = childCount(walkerNodes[i], "routestep");
		bool back = text(walkerNodes[i], "loop_type") == "WALK_BACK";
		walkBack += back ? 1 : 0;
		ASSERT_EQ(route.getRouteSteps().size(), back ? steps + steps - 2 : steps) << route.getRouteId();
		for (size_t s = 0; s < route.getRouteSteps().size(); ++s) {
			EXPECT_EQ(route.getRouteSteps()[s]->getStepIndex(), static_cast<int32_t>(s));
			EXPECT_EQ(route.getRouteSteps()[s]->isLastStep(), s + 1 == route.getRouteSteps().size());
		}
		if (number(walkerNodes[i], "pool", 1) == 2) {
			EXPECT_EQ(route.getRows(), (std::vector<int32_t>{2}));
		} else if (text(walkerNodes[i], "formation") == "SQUARE" && walkerNodes[i].attribute("rows")) {
			++squares;
			std::vector<int64_t> expectedRows = numbers(text(walkerNodes[i], "rows"), ',');
			ASSERT_TRUE(route.getRows().has_value());
			EXPECT_EQ(std::vector<int64_t>(route.getRows()->begin(), route.getRows()->end()), expectedRows);
		}
	}
	std::cout << walkBack << " walk back routes, " << squares << " square formations with rows\n";

	Documents doorDocuments;
	std::unique_ptr<DoorWorlds> doors = bindImport<DoorWorlds>("staticdoors/staticdoor_templates.xml", doorDocuments);
	EXPECT_EQ(static_cast<int64_t>(distinctCount(doors->get<0>(), [](const staticdoor::StaticDoorWorld& w) { return w.getWorldId(); })),
	          expectedCount("staticdoor_templates"));
	std::vector<pugi::xml_node> worldNodes = children(doorDocuments, "world");
	for (size_t i = 0; i < worldNodes.size(); ++i) {
		std::set<int64_t> ids;
		for (pugi::xml_node door : worldNodes[i].children("staticdoor")) {
			ids.insert(number(door, "id"));
			const staticdoor::StaticDoorTemplate* found = doors->get<0>()[i].getStaticDoor(static_cast<int32_t>(number(door, "id")));
			ASSERT_NE(found, nullptr);
			EXPECT_EQ(found->getKeyId(), number(door, "keyid"));
		}
		EXPECT_EQ(doors->get<0>()[i].getStaticDoors().size(), ids.size());
	}
}

// ---- housing, towns, sieges ----------------------------------------------------------------------------------------------------------------------

TEST_F(TemplatesBRealDataTest, HousingTownsAndSieges) {
	Documents buildingDocuments;
	std::unique_ptr<Buildings> buildings = bindImport<Buildings>("housing/house_buildings.xml", buildingDocuments);
	EXPECT_EQ(static_cast<int64_t>(distinctCount(buildings->get<0>(), [](const housing::Building& b) { return b.getId(); })),
	          expectedCount("buildings"));
	std::vector<pugi::xml_node> buildingNodes = children(buildingDocuments, "building");
	for (size_t i = 0; i < buildingNodes.size(); ++i) {
		pugi::xml_node parts = buildingNodes[i].child("parts");
		if (!parts)
			continue;
		size_t expectedParts = 0;
		for (const char* optionalPart : {"fence", "frame", "garden", "outwall", "roof"})
			expectedParts += parts.child(optionalPart) ? 1 : 0;
		for (const char* numberPart : {"door", "infloor", "inwall"})
			expectedParts += parts.child(numberPart) && std::string_view(parts.child(numberPart).text().get()) != "0" ? 1 : 0;
		EXPECT_EQ(buildings->get<0>()[i].getDefaultPartIds().size(), expectedParts) << buildings->get<0>()[i].getId();
		EXPECT_EQ(buildings->get<0>()[i].getPartsMatchTag(), text(buildingNodes[i], "parts_match"));
	}

	Documents landDocuments;
	std::unique_ptr<Lands> lands = bindImport<Lands>("housing/houses.xml", landDocuments);
	EXPECT_EQ(static_cast<int64_t>(lands->get<0>().size()), expectedCount("house_lands"));
	size_t addresses = 0;
	for (const housing::HousingLand& land : lands->get<0>()) {
		ASSERT_NE(land.getDefaultBuilding(), nullptr);
		for (const housing::HouseAddress& address : *land.getAddresses()) {
			EXPECT_EQ(address.getLand(), &land);
			++addresses;
		}
	}
	EXPECT_GT(addresses, 0u);

	Documents partDocuments;
	std::unique_ptr<HouseParts> parts = bindImport<HouseParts>("housing/house_parts.xml", partDocuments);
	EXPECT_EQ(static_cast<int64_t>(distinctCount(parts->get<0>(), [](const housing::HousePart& p) { return p.getId(); })),
	          expectedCount("house_parts"));
	size_t matches = 0;
	for (const housing::HousePart& part : parts->get<0>()) {
		for (const housing::Building& building : buildings->get<0>()) {
			if (building.getPartsMatchTag().empty())
				continue;
			bool expected = part.getTags()->contains(building.getPartsMatchTag());
			EXPECT_EQ(part.isForBuilding(building), expected);
			matches += expected ? 1 : 0;
		}
	}
	EXPECT_GT(matches, 0u);

	Documents townDocuments;
	std::unique_ptr<TownMaps> towns = bindImport<TownMaps>("town_spawns", townDocuments);
	std::map<int32_t, const towns::TownSpawnMap*> mapsById; // TownSpawnsData: last map wins
	for (const towns::TownSpawnMap& map : towns->get<0>())
		mapsById[map.getMapId()] = &map;
	int64_t townSpawns = 0;
	for (const auto& [mapId, map] : mapsById)
		for (const towns::TownSpawn* town : map->getTownSpawns())
			for (const towns::TownLevel* level : town->getTownLevels())
				townSpawns += static_cast<int64_t>(level->getSpawns().size());
	EXPECT_EQ(townSpawns, expectedCount("town_spawns_data")) << "TownSpawnsData.getSpawnsCount through the hooks' maps";

	Documents siegeDocuments;
	std::unique_ptr<SiegeLocations> sieges = bindImport<SiegeLocations>("siege/siege_locations.xml", siegeDocuments);
	std::set<int32_t> siegeIds;
	for (const siegelocation::SiegeLocationTemplate& location : sieges->get<0>()) {
		siege::SiegeType type = location.getType().value();
		if (type == siege::SiegeType::FORTRESS || type == siege::SiegeType::ARTIFACT || type == siege::SiegeType::OUTPOST ||
		    type == siege::SiegeType::AGENT_FIGHT)
			siegeIds.insert(location.getId());
	}
	EXPECT_EQ(static_cast<int64_t>(siegeIds.size()), expectedCount("siege_locations"));
	std::vector<pugi::xml_node> siegeNodes = children(siegeDocuments, "siege_location");
	size_t waves = 0;
	runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
	for (size_t i = 0; i < siegeNodes.size(); ++i) {
		const siegelocation::SiegeLocationTemplate& location = sieges->get<0>()[i];
		for (pugi::xml_node stone : siegeNodes[i].child("door_repair_data").children("door_repair_stone"))
			EXPECT_EQ(location.getDoorRepairData()->getRepairStone(static_cast<int32_t>(number(stone, "static_id")))->getDoorId(),
			          number(stone, "door_id"));
		for (pugi::xml_node assaulter : siegeNodes[i].child("assault_data").children("assaulter")) {
			std::optional<siege::AssaulterType> type = xml::enumFromXml<siege::AssaulterType>(text(assaulter, "type"));
			ASSERT_TRUE(type.has_value());
			size_t npcs = numbers(text(assaulter, "npc_ids")).size();
			const auto& processed = location.getAssaultData()->getProcessedAssaulters()[static_cast<size_t>(*type)];
			ASSERT_TRUE(processed.has_value());
			EXPECT_LE(processed->size(), npcs);
			if (*type == siege::AssaulterType::TELEPORT)
				EXPECT_EQ(processed->size(), npcs);
			++waves;
		}
	}
	std::cout << waves << " assault waves checked\n";
}

// ---- quests, tribes, trade and goods lists and the other lists ---------------------------------------------------------------------------------

TEST_F(TemplatesBRealDataTest, QuestsTribesTradeGoodsAndOtherLists) {
	Documents questDocuments;
	std::unique_ptr<Quests> quests = bindImport<Quests>("quest_data/quest_data.xml", questDocuments);
	EXPECT_EQ(static_cast<int64_t>(distinctCount(quests->get<0>(), [](const QuestTemplate& q) { return q.getId(); })), expectedCount("quests"));
	std::vector<pugi::xml_node> questNodes = children(questDocuments, "quest");
	for (size_t index : sample(questNodes.size(), 8043)) {
		pugi::xml_node node = questNodes[index];
		const QuestTemplate& quest = quests->get<0>()[index];
		SCOPED_TRACE("quest id=" + text(node, "id"));
		EXPECT_EQ(quest.getId(), number(node, "id"));
		EXPECT_EQ(quest.getL10nId(), number(node, "nameId"));
		EXPECT_EQ(quest.getMaxRepeatCount(), number(node, "max_repeat_count", 1));
		EXPECT_EQ(quest.isRepeatable(), number(node, "max_repeat_count", 1) > 1);
		std::string cycle = text(node, "repeat_cycle");
		EXPECT_EQ(quest.isTimeBased(), static_cast<bool>(node.attribute("repeat_cycle")));
		EXPECT_EQ(quest.isDaily(), cycle.find("ALL") != std::string::npos);
		EXPECT_EQ(quest.getQuestDrop().size(), childCount(node, "quest_drop"));
		EXPECT_EQ(quest.getRewards().size(), childCount(node, "rewards"));
		EXPECT_EQ(quest.getXMLStartConditions().size(), childCount(node, "start_conditions"));
		for (size_t d = 0; d < quest.getQuestDrop().size(); ++d)
			EXPECT_EQ(quest.getQuestDrop()[d].getChance(),
			          number(*std::next(node.children("quest_drop").begin(), static_cast<std::ptrdiff_t>(d)), "chance", 100));
	}

	Documents tribeDocuments;
	std::unique_ptr<Tribes> tribes = bindImport<Tribes>("tribe/tribe_relations.xml", tribeDocuments);
	EXPECT_EQ(static_cast<int64_t>(distinctCount(tribes->get<0>(), [](const tribe::Tribe& t) { return t.getName(); })),
	          expectedCount("tribe_relations"));
	std::vector<pugi::xml_node> tribeNodes = children(tribeDocuments, "tribe");
	for (size_t i = 0; i < tribeNodes.size(); ++i) {
		const tribe::Tribe& tribe = tribes->get<0>()[i];
		std::string base = text(tribeNodes[i], "base", "NONE");
		EXPECT_EQ(xml::enumName(tribe.getBase()), base == "NONE" ? text(tribeNodes[i], "name") : base);
		EXPECT_EQ(tribe.getAggro().empty(), !tribeNodes[i].child("aggro"));
	}

	Documents tradeDocuments;
	std::unique_ptr<TradeLists> trades = bindImport<TradeLists>("npc_trade_list.xml", tradeDocuments);
	EXPECT_EQ(static_cast<int64_t>(distinctCount(trades->get<0>(), [](const tradelist::TradeListTemplate& t) { return t.getNpcId(); })),
	          expectedCount("npc_trade_list"));
	std::vector<pugi::xml_node> tradeNodes = children(tradeDocuments, "tradelist_template");
	for (size_t i = 0; i < tradeNodes.size(); ++i)
		EXPECT_EQ(static_cast<size_t>(trades->get<0>()[i].getCount()), childCount(tradeNodes[i], "tradelist"));

	Documents goodsDocuments;
	std::unique_ptr<GoodsLists> goods = bindImport<GoodsLists>("goodslists/goodslists.xml", goodsDocuments);
	auto goodsId = [](const goods::GoodsList& g) { return g.getId(); };
	EXPECT_EQ(
	  static_cast<int64_t>(distinctCount(goods->get<0>(), goodsId) + distinctCount(goods->get<1>(), goodsId) + distinctCount(goods->get<2>(), goodsId)),
	  expectedCount("goodslists"));
	std::vector<pugi::xml_node> goodsNodes = children(goodsDocuments, "list");
	size_t limited = 0;
	{
		runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
		for (size_t i = 0; i < goodsNodes.size(); ++i) {
			std::vector<int32_t> ids;
			for (pugi::xml_node item : goodsNodes[i].children("item"))
				ids.push_back(static_cast<int32_t>(number(item, "id")));
			EXPECT_EQ(goods->get<0>()[i].getItemIdList(), ids);
			limited += goods->get<0>()[i].getLimitedItems().size();
		}
	}
	std::cout << limited << " limited goods list items\n";

	Documents documents;
	EXPECT_EQ(
	  static_cast<int64_t>(distinctCount(bindImport<AiTemplates>("ai", documents)->get<0>(), [](const ai::AITemplate& a) { return a.getNpcId(); })),
	  expectedCount("ai_templates"));
	EXPECT_EQ(static_cast<int64_t>(distinctCount(bindImport<Panels>("polymorph_panels/polymorph_panels.xml", documents)->get<0>(),
	                                             [](const panels::SkillPanel& p) { return p.getPanelId(); })),
	          expectedCount("polymorph_panels"));
	EXPECT_EQ(static_cast<int64_t>(distinctCount(bindImport<Recipes>("recipe/recipe_templates.xml", documents)->get<0>(),
	                                             [](const recipe::RecipeTemplate& r) { return r.getId(); })),
	          expectedCount("recipe_templates"));
	EXPECT_EQ(static_cast<int64_t>(
	            distinctCount(bindImport<Titles>("player_titles.xml", documents)->get<0>(), [](const TitleTemplate& t) { return t.getTitleId(); })),
	          expectedCount("player_titles"));
	EXPECT_EQ(static_cast<int64_t>(distinctCount(bindImport<Gatherables>("gatherables/gatherable_templates.xml", documents)->get<0>(),
	                                             [](const gather::GatherableTemplate& g) { return g.getTemplateId(); })),
	          expectedCount("gatherable_templates"));
	EXPECT_EQ(static_cast<int64_t>(distinctCount(bindImport<Factions>("npc_factions/npc_factions.xml", documents)->get<0>(),
	                                             [](const factions::NpcFactionTemplate& f) { return f.getId(); })),
	          expectedCount("npc_factions"));
	EXPECT_EQ(static_cast<int64_t>(distinctCount(bindImport<Teleporters>("npc_teleporter.xml", documents)->get<0>(),
	                                             [](const teleport::TeleporterTemplate& t) { return t.getTeleportId(); })),
	          expectedCount("npc_teleporter"));
	EXPECT_EQ(static_cast<int64_t>(distinctCount(bindImport<TeleLocations>("teleport_location.xml", documents)->get<0>(),
	                                             [](const teleport::TelelocationTemplate& t) { return t.getLocId(); })),
	          expectedCount("teleport_location"));
	EXPECT_EQ(static_cast<int64_t>(distinctCount(bindImport<InstanceBuffs>("instance_bonusattr/instance_bonusattr.xml", documents)->get<0>(),
	                                             [](const instance_bonusatrr::InstanceBonusAttr& b) { return b.getBuffId(); })),
	          expectedCount("instance_bonusattrs"));
	EXPECT_EQ(static_cast<int64_t>(
	            distinctCount(bindImport<Rides>("ride/ride.xml", documents)->get<0>(), [](const ride::RideInfo& r) { return r.getNpcId(); })),
	          expectedCount("rides"));
	EXPECT_EQ(static_cast<int64_t>(bindImport<InstanceExits>("instance_exit/instance_exit.xml", documents)->get<0>().size()),
	          expectedCount("instance_exits"));
	EXPECT_EQ(static_cast<int64_t>(distinctCount(bindImport<WorldRaids>("world_raid/world_raids.xml", documents)->get<0>(),
	                                             [](const worldraid::WorldRaidLocation& w) { return w.getLocationId(); })),
	          expectedCount("world_raid_locations"));
	EXPECT_EQ(static_cast<int64_t>(distinctCount(bindImport<ChallengeTasks>("quest_data/challenge_tasks.xml", documents)->get<0>(),
	                                             [](const challenge::ChallengeTaskTemplate& t) { return t.getId(); })),
	          expectedCount("challenge_tasks"));
	EXPECT_EQ(static_cast<int64_t>(bindImport<Ranks>("conqueror_protector_ranks/conqueror_protector_ranks.xml", documents)->get<0>().size()),
	          expectedCount("conqueror_protector_ranks"));
	EXPECT_EQ(static_cast<int64_t>(distinctCount(bindImport<Windstreams>("windstreams/windstreams.xml", documents)->get<0>(),
	                                             [](const windstreams::WindstreamTemplate& w) { return w.getMapId(); })),
	          expectedCount("windstreams"));
	std::unique_ptr<FlyPaths> flyPaths = bindImport<FlyPaths>("flypath_template.xml", documents);
	EXPECT_EQ(static_cast<int64_t>(distinctCount(flyPaths->get<0>(), [](const flypath::FlyPathEntry& f) { return f.getId(); })),
	          expectedCount("flypath_template"));
	std::vector<pugi::xml_node> flyPathNodes = children(documents, "flypath_location");
	for (size_t i = 0; i < flyPathNodes.size(); ++i)
		EXPECT_EQ(flyPaths->get<0>()[i].getTimeInMs(), static_cast<int32_t>(flyPathNodes[i].attribute("time").as_float() * 1000.0f));
	EXPECT_EQ(static_cast<int64_t>(bindImport<FlyRings>("fly_rings/fly_rings.xml", documents)->get<0>().size()), expectedCount("fly_rings"));
	EXPECT_EQ(static_cast<int64_t>(distinctCount(bindImport<Materials>("mesh_materials/material_templates.xml", documents)->get<0>(),
	                                             [](const materials::MaterialTemplate& m) { return m.getId(); })),
	          expectedCount("material_templates"));
	std::unique_ptr<Arcade> arcade = bindImport<Arcade>("events/arcadelist.xml", documents);
	EXPECT_EQ(static_cast<int64_t>(arcade->get<1>().size()), expectedCount("arcadelist"));
	ASSERT_NE(arcade->get<0>(), nullptr);
	EXPECT_EQ(arcade->get<0>()->getMaxUpgradeLevel(), &arcade->get<0>()->getLevels().back());
	for (std::string_view file : {"storage_expander/cube_expander.xml", "storage_expander/warehouse_expander.xml"}) {
		std::set<int32_t> npcIds;
		for (const StorageExpansionTemplate& expansion : bindImport<Expansions>(file, documents)->get<0>()) {
			npcIds.insert(expansion.getNpcIds()->begin(), expansion.getNpcIds()->end());
			EXPECT_LE(expansion.getMinExpansionLevel(), expansion.getMaxExpansionLevel());
			EXPECT_TRUE(expansion.getPrice(expansion.getMinExpansionLevel()).has_value());
		}
		EXPECT_EQ(static_cast<int64_t>(npcIds.size()),
		          expectedCount(file.find("cube") != std::string_view::npos ? "cube_expander" : "warehouse_expander"));
	}
	std::set<std::string> versionIds;
	for (const walker::RouteParent& parent : bindImport<WalkerVersions>("walker_versions.xml", documents)->get<0>())
		for (const walker::RouteVersion& version : parent.getRouteVersion())
			versionIds.insert(version.getId());
	EXPECT_EQ(static_cast<int64_t>(versionIds.size()), expectedCount("walker_versions"));
	xml::LoadContext legionContext(options());
	std::unique_ptr<dataholders::LegionDominionData> legionDominions =
	  bindHolder<dataholders::LegionDominionData>(legionContext, parse("legion_dominion_template.xml"));
	EXPECT_EQ(static_cast<int64_t>(legionDominions->getLocationTemplates().size()), expectedCount("legion_dominion_template"));
	expectTotals(legionContext, {"legion_dominion_template.xml"});
}

// ---- items, item groups, global drop rules, item sets, events --------------------------------------------------------------------------------

TEST_F(TemplatesBRealDataTest, ItemGroupsGlobalDropsItemSetsAndEvents) {
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
		for (pugi::xml_node item : children(itemDocuments, "item_template"))
			while (pugi::xml_node actions = item.child("actions")) {
				item.remove_child(actions);
				++removedActions;
			}
		items = bindHolder<Items>(*context, itemDocuments);
	}
	std::cout << "real items bound in " << std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count()
	          << " ms\n";

	// the holders below need the item XmlIDs of the context; their totals are compared through a context of their own statistics
	Documents groupDocuments = parse("items/item_groups.xml");
	std::unique_ptr<ItemGroups> groups = bindHolder<ItemGroups>(*context, groupDocuments);
	int64_t bonusSize = 0;
	size_t checkedEntries = 0;
	std::apply(
	  [&](const auto&... group) {
		  size_t index = 0;
		  ((checkBonusGroup(*group, index++, bonusSize, checkedEntries)), ...);
	  },
	  groups->children);
	EXPECT_EQ(bonusSize, expectedLineValue("bonus item group templates", 0)) << "ItemGroupsData.bonusSize";
	std::cout << checkedEntries << " bonus group entries dispatched through ItemRaceEntry\n";

	Documents ruleDocuments = parse("global_drops/rules");
	std::unique_ptr<dataholders::GlobalDropData> rules = bindHolder<dataholders::GlobalDropData>(*context, ruleDocuments);
	EXPECT_EQ(static_cast<int64_t>(rules->getAllRules().size()), expectedLineValue("global drop rules", 0));
	size_t dropItems = 0;
	for (const globaldrops::GlobalRule& rule : rules->getAllRules()) {
		if (!rule.getDropItems())
			continue;
		for (const globaldrops::GlobalDropItem& item : *rule.getDropItems()) {
			EXPECT_GE(item.getMaxCount(), item.getMinCount()) << "hook: max_count defaults to min_count";
			++dropItems;
		}
	}
	EXPECT_GT(dropItems, 0u);

	Documents setDocuments = parse("item_sets/item_sets.xml");
	std::unique_ptr<ItemSets> sets = bindHolder<ItemSets>(*context, setDocuments);
	EXPECT_EQ(static_cast<int64_t>(distinctCount(sets->get<0>(), [](const itemset::ItemSetTemplate& s) { return s.getId(); })),
	          expectedCount("item_sets"));
	for (const itemset::ItemSetTemplate& set : sets->get<0>()) {
		if (set.getFullbonus() != nullptr)
			EXPECT_EQ(set.getFullbonus()->getCount(), static_cast<int32_t>(set.getItempart().size()));
	}

	Documents eventDocuments = parse("events/timed_events");
	size_t removedSpawns = 0;
	std::unique_ptr<Events> events;
	try {
		events = bindHolder<Events>(*context, eventDocuments);
	} catch (const runtime::UnportedException& e) {
		std::cout << "the event spawns have unported hooks (" << e.what() << "): removing the <spawns> elements\n";
		for (pugi::xml_node eventNode : children(eventDocuments, "event")) {
			while (pugi::xml_node spawns = eventNode.child("spawns")) {
				eventNode.remove_child(spawns);
				++removedSpawns;
			}
		}
		events = bindHolder<Events>(*context, eventDocuments);
	}
	EXPECT_EQ(static_cast<int64_t>(events->get<0>().size()), expectedCount("timed_events"));
	std::vector<pugi::xml_node> eventNodes = children(eventDocuments, "event");
	ASSERT_EQ(eventNodes.size(), events->get<0>().size());
	for (size_t i = 0; i < eventNodes.size(); ++i) {
		const event::EventTemplate& eventTemplate = events->get<0>()[i];
		EXPECT_EQ(eventTemplate.hasConfigProperties(), static_cast<bool>(eventNodes[i].child("config_properties")));
		if (eventTemplate.hasConfigProperties())
			EXPECT_NO_THROW(eventTemplate.loadConfigProperties());
		EXPECT_EQ(eventTemplate.getStartableQuests().size(), numbers(eventNodes[i].child("quests").child("startable").text().get()).size());
	}
	std::cout << removedSpawns << " event spawns elements removed\n";
	if (removedSpawns == 0 && removedActions == 0)
		expectTotals(*context,
		             {"items/item_templates.xml", "items/item_groups.xml", "global_drops/rules", "item_sets/item_sets.xml", "events/timed_events"});
	EXPECT_EQ(context->stats().totalElements().unknown, 0u);
	EXPECT_EQ(context->stats().totalAttributes().unknown, 0u);
}

} // namespace
} // namespace aion::gameserver::model::templates::realdatab
