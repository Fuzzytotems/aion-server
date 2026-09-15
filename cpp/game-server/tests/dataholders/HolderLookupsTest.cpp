// P4-09 holder lookups ported with header requests pre-1 and pre-2 (docs/porting/header-requests.md): SkillData's indexes and Java's HashMap
// iteration order of getSkillTemplates, ItemSetData and WorldMapsData over their bound lists, and NpcData::getNpcTemplate and
// ItemGroupsData::isFood, whose index-building hooks are not ported yet (the lookups over the empty indexes behave like Java's over empty maps).
// Wave 3a-2 header requests (shells-1, geo-1, templates-b-1..3, items-1, items-2, items-4, items-5): SkillData::getSkillTemplate,
// getSkillTemplatesByStack and size, and the index hooks and lookups of MaterialData, ItemData, HouseBuildingData, WalkerVersionsData,
// ItemRandomBonusData, TemperingData and RecipeData.
// Expectations are derived by hand from SkillData.java, ItemSetData.java, WorldMapsData.java, NpcData.java, ItemGroupsData.java,
// MaterialData.java, ItemData.java, HouseBuildingData.java, WalkerVersionsData.java, ItemRandomBonusData.java, TemperingData.java,
// RecipeData.java and java.util.HashMap (putVal, resize, treeifyBin).

#include <gtest/gtest.h>

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "aion/gameserver/dataholders/HouseBuildingData.bind.h"
#include "aion/gameserver/dataholders/HouseBuildingData.h"
#include "aion/gameserver/dataholders/ItemData.bind.h"
#include "aion/gameserver/dataholders/ItemData.h"
#include "aion/gameserver/dataholders/ItemGroupsData.h"
#include "aion/gameserver/dataholders/ItemRandomBonusData.bind.h"
#include "aion/gameserver/dataholders/ItemRandomBonusData.h"
#include "aion/gameserver/dataholders/ItemSetData.bind.h"
#include "aion/gameserver/dataholders/ItemSetData.h"
#include "aion/gameserver/dataholders/MaterialData.bind.h"
#include "aion/gameserver/dataholders/MaterialData.h"
#include "aion/gameserver/dataholders/NpcData.h"
#include "aion/gameserver/dataholders/RecipeData.bind.h"
#include "aion/gameserver/dataholders/RecipeData.h"
#include "aion/gameserver/dataholders/SkillData.bind.h"
#include "aion/gameserver/dataholders/SkillData.h"
#include "aion/gameserver/dataholders/TemperingData.bind.h"
#include "aion/gameserver/dataholders/TemperingData.h"
#include "aion/gameserver/dataholders/WalkerVersionsData.bind.h"
#include "aion/gameserver/dataholders/WalkerVersionsData.h"
#include "aion/gameserver/dataholders/WorldMapsData.bind.h"
#include "aion/gameserver/dataholders/WorldMapsData.h"
#include "aion/gameserver/dataholders/loadingutils/LoadContext.h"
#include "aion/gameserver/dataholders/loadingutils/StaticDataException.h"
#include "aion/gameserver/dataholders/loadingutils/StaticDataLoader.h"
#include "aion/gameserver/model/templates/item/ItemTemplate.bind.h"
#include "aion/gameserver/model/templates/item/ItemTemplate.h"
#include "aion/gameserver/model/templates/item/bonuses/StatBonusType.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/runtime/base/Unported.h"

namespace aion::gameserver::dataholders {
namespace {

using skillengine::model::SkillTemplate;

template <class T>
std::unique_ptr<T> bindXml(const std::string& text) {
	xml::LoadContext context;
	return xml::bindString<T>(context, text);
}

std::string skillXml(int32_t skillId, const std::string& stack, const std::string& group = {}) {
	return "<skill_template skill_id=\"" + std::to_string(skillId) + "\" name=\"s" + std::to_string(skillId) +
		R"(" nameId="1" skilltype="MAGICAL" skillsubtype="BUFF" activation="ACTIVE" duration="0" stack=")" + stack + "\"" +
		(group.empty() ? std::string() : " group=\"" + group + "\"") + "/>";
}

std::vector<int32_t> idsOf(const std::vector<const SkillTemplate*>& templates) {
	std::vector<int32_t> ids;
	for (const SkillTemplate* t : templates)
		ids.push_back(t->getSkillId());
	return ids;
}

TEST(SkillDataTest, GroupIndex) {
	std::unique_ptr<SkillData> data = bindXml<SkillData>("<skill_data>" + skillXml(1, "GM_A", "GM_GROUP") + skillXml(2, "B", "GM_GROUP") +
		skillXml(3, "C") + skillXml(4, "D", "OTHER") + "</skill_data>");
	const std::vector<const SkillTemplate*>* gmGroup = data->getSkillTemplatesByGroup("GM_GROUP");
	ASSERT_NE(gmGroup, nullptr);
	EXPECT_EQ(idsOf(*gmGroup), (std::vector<int32_t>{1, 2})) << "the group list keeps the data order";
	ASSERT_NE(data->getSkillTemplatesByGroup("OTHER"), nullptr);
	EXPECT_EQ(idsOf(*data->getSkillTemplatesByGroup("OTHER")), (std::vector<int32_t>{4}));
	EXPECT_EQ(data->getSkillTemplatesByGroup("MISSING"), nullptr) << "Java: null";
	EXPECT_EQ(data->getSkillTemplatesByGroup(""), nullptr) << "a template without a group is not indexed (Java: getGroup() == null)";
	EXPECT_EQ(idsOf(data->getSkillTemplates()), (std::vector<int32_t>{1, 2, 3, 4}));
	EXPECT_TRUE(SkillData().getSkillTemplates().empty());
	EXPECT_EQ(SkillData().getSkillTemplatesByGroup("GM_GROUP"), nullptr);
}

TEST(SkillDataTest, SkillTemplatesInJavaHashMapOrder) {
	// put order 17, 1, 33, 2, 65537, 3..10, then 33 again (replaces the value, keeps the position and the size of 13).
	// Table of 16: bucket 1 holds 17, 1, 33 (in put order); 65537 spreads to 0x10001 ^ 0x1 = 0x10000, bucket 0. The 13th key exceeds the
	// threshold 12: the table doubles to 32 and bucket 1 splits into bucket 1 (1, 33, still in order) and bucket 17 (17).
	std::string xml =
		"<skill_data>" + skillXml(17, "S17") + skillXml(1, "S1") + skillXml(33, "FIRST") + skillXml(2, "S2") + skillXml(65537, "S65537");
	for (int32_t id = 3; id <= 10; ++id)
		xml += skillXml(id, "S" + std::to_string(id));
	xml += skillXml(33, "SECOND") + "</skill_data>";
	std::unique_ptr<SkillData> data = bindXml<SkillData>(xml);
	std::vector<const SkillTemplate*> templates = data->getSkillTemplates();
	EXPECT_EQ(idsOf(templates), (std::vector<int32_t>{65537, 1, 33, 2, 3, 4, 5, 6, 7, 8, 9, 10, 17}));
	ASSERT_EQ(templates.size(), 13u);
	EXPECT_EQ(templates[2]->getStack(), "SECOND") << "HashMap.put replaces the value of a present key";
}

TEST(SkillDataTest, TreeifyBinResizesSmallTables) {
	// 16, 32, ..., 144 all land in bucket 0 of the table of 16. Appending the 9th key to a bucket of 8 calls treeifyBin, which resizes a table
	// below 64 buckets instead: in the table of 32, bucket 0 holds 32, 64, 96, 128 and bucket 16 holds 16, 48, 80, 112, 144.
	std::string xml = "<skill_data>";
	for (int32_t id = 16; id <= 144; id += 16)
		xml += skillXml(id, "S" + std::to_string(id));
	xml += "</skill_data>";
	std::unique_ptr<SkillData> data = bindXml<SkillData>(xml);
	EXPECT_EQ(idsOf(data->getSkillTemplates()), (std::vector<int32_t>{32, 64, 96, 128, 16, 48, 80, 112, 144}));
	// 8 keys in one bucket stay a list of the table of 16
	std::string eight = "<skill_data>";
	for (int32_t id = 128; id >= 16; id -= 16)
		eight += skillXml(id, "S" + std::to_string(id));
	eight += "</skill_data>";
	EXPECT_EQ(idsOf(bindXml<SkillData>(eight)->getSkillTemplates()), (std::vector<int32_t>{128, 112, 96, 80, 64, 48, 32, 16}));
}

TEST(ItemSetDataTest, SetsByItemId) {
	std::unique_ptr<ItemSetData> data;
	try {
		data = bindXml<ItemSetData>(R"(<item_sets>)"
			R"(<itemset id="1" name="first"><itempart itemid="100"/><itempart itemid="101"/><partbonus count="2"/></itemset>)"
			R"(<itemset id="2" name="second"><itempart itemid="101"/><itempart itemid="102"/><partbonus count="2"/></itemset>)"
			R"(</item_sets>)");
	} catch (const runtime::UnportedException&) {
		GTEST_SKIP() << "ItemSetTemplate::afterUnmarshal (P4-07b) is not ported yet";
	}
	ASSERT_NE(data->getItemSetTemplateByItemId(100), nullptr);
	EXPECT_EQ(data->getItemSetTemplateByItemId(100)->getId(), 1);
	EXPECT_EQ(data->getItemSetTemplateByItemId(101)->getId(), 2) << "a later set replaces the item's entry (HashMap.put)";
	EXPECT_EQ(data->getItemSetTemplateByItemId(102)->getId(), 2);
	EXPECT_EQ(data->getItemSetTemplateByItemId(1), nullptr) << "set ids are not item ids";
}

TEST(ItemSetDataTest, EmptyHolder) {
	EXPECT_EQ(ItemSetData().getItemSetTemplateByItemId(100), nullptr);
}

TEST(WorldMapsDataTest, TemplatesById) {
	std::unique_ptr<WorldMapsData> data = bindXml<WorldMapsData>(R"(<world_maps>)"
		R"(<map id="110010000" cName="Sanctum" death_level="0" water_level="0" flags="BIND FLY"/>)"
		R"(<map id="210010000" cName="Poeta" death_level="0" water_level="0" flags="RECALL"/>)"
		R"(<map id="110010000" cName="SanctumAgain" death_level="0" water_level="0" flags="GLIDE"/>)"
		R"(</world_maps>)");
	ASSERT_NE(data->getTemplate(210010000), nullptr);
	EXPECT_EQ(data->getTemplate(210010000)->getCName(), "Poeta");
	EXPECT_EQ(data->getTemplate(210010000)->getFlags(), 2) << "RECALL = 1 << 1";
	ASSERT_NE(data->getTemplate(110010000), nullptr);
	EXPECT_EQ(data->getTemplate(110010000)->getCName(), "SanctumAgain") << "LinkedHashMap.put replaces the value of a present key";
	EXPECT_EQ(data->getTemplate(0), nullptr);
	EXPECT_EQ(WorldMapsData().getTemplate(110010000), nullptr);
}

TEST(NpcDataTest, UnknownNpcIsNull) {
	// NpcData.init (afterUnmarshal) is not ported yet, so no index exists: every lookup of the empty holder returns null, like Java's empty map
	EXPECT_EQ(NpcData().getNpcTemplate(210671), nullptr);
}

TEST(ItemGroupsDataTest, IsFoodOnAnEmptyHolderThrowsLikeJava) {
	// Java: petFood.get(FoodType.EXCLUDES) is null before afterUnmarshal filled the EnumMap, so contains throws NullPointerException
	using model::templates::pet::FoodType;
	ItemGroupsData data;
	EXPECT_THROW(data.isFood(182006999, FoodType::BONES), runtime::NullPointerException);
	EXPECT_THROW(data.isFood(182006999, FoodType::MISCELLANEOUS), runtime::NullPointerException);
}

TEST(SkillDataTest, SkillTemplateStackAndSize) {
	std::unique_ptr<SkillData> data = bindXml<SkillData>("<skill_data>" + skillXml(1, "STACK_A") + skillXml(2, "STACK_B") + skillXml(3, "STACK_A") +
		skillXml(1, "STACK_C") + "</skill_data>");
	ASSERT_NE(data->getSkillTemplate(2), nullptr);
	EXPECT_EQ(data->getSkillTemplate(2)->getStack(), "STACK_B");
	ASSERT_NE(data->getSkillTemplate(1), nullptr);
	EXPECT_EQ(data->getSkillTemplate(1)->getStack(), "STACK_C") << "HashMap.put replaces the value of a present key";
	EXPECT_EQ(data->getSkillTemplate(4), nullptr) << "Java: null";
	const std::vector<const SkillTemplate*>* stackA = data->getSkillTemplatesByStack("STACK_A");
	ASSERT_NE(stackA, nullptr);
	EXPECT_EQ(idsOf(*stackA), (std::vector<int32_t>{1, 3})) << "the stack list keeps the templates of the data in data order";
	EXPECT_EQ(data->getSkillTemplatesByStack("STACK_D"), nullptr);
	EXPECT_EQ(data->size(), 3) << "skillTemplateById.size(): the duplicate id counts once";
	EXPECT_EQ(SkillData().size(), 0);
	EXPECT_EQ(SkillData().getSkillTemplate(1), nullptr);
}

TEST(MaterialDataTest, TemplatesById) {
	std::unique_ptr<MaterialData> data = bindXml<MaterialData>(R"(<material_templates>)"
		R"(<material id="12"><skill id="8269" level="1" target="PLAYER" frequency="3"/></material>)"
		R"(<material id="13"><skill id="8341" level="1" target="PLAYER" frequency="3"/></material>)"
		R"(</material_templates>)");
	ASSERT_NE(data->getTemplate(13), nullptr);
	EXPECT_EQ(data->getTemplate(13)->getId(), 13);
	EXPECT_EQ(data->getTemplate(13)->getSkills().size(), 1u);
	EXPECT_EQ(data->getTemplate(11), nullptr) << "Java: null";
	EXPECT_EQ(MaterialData().getTemplate(12), nullptr);
}

TEST(ItemDataTest, TemplatesById) {
	std::unique_ptr<ItemData> data =
		bindXml<ItemData>(R"(<item_templates><item_template id="100000001" level="10"/><item_template id="100000002" level="20"/></item_templates>)");
	ASSERT_NE(data->getItemTemplate(100000002), nullptr);
	EXPECT_EQ(data->getItemTemplate(100000002)->getTemplateId(), 100000002);
	EXPECT_EQ(data->getItemTemplate(100000002)->getLevel(), 20);
	EXPECT_EQ(data->getItemTemplate(100000003), nullptr) << "Java: null";
	EXPECT_EQ(ItemData().getItemTemplate(100000001), nullptr);
}

TEST(HouseBuildingDataTest, BuildingsByIdAndDuplicates) {
	std::unique_ptr<HouseBuildingData> data =
		bindXml<HouseBuildingData>(R"(<buildings><building id="1" parts_match="CP_A"/><building id="2" parts_match="CP_B"/></buildings>)");
	ASSERT_NE(data->getBuilding(2), nullptr);
	EXPECT_EQ(data->getBuilding(2)->getId(), 2);
	EXPECT_EQ(data->getBuilding(3), nullptr) << "Java: null";
	try {
		static_cast<void>(bindXml<HouseBuildingData>(R"(<buildings><building id="1"/><building id="1"/></buildings>)"));
		FAIL() << "expected the duplicate id to fail the load";
	} catch (const xml::StaticDataException& e) {
		EXPECT_NE(std::string(e.what()).find("Duplicate building ID 1"), std::string::npos) << e.what();
	}
}

TEST(WalkerVersionsDataTest, RouteVersionIds) {
	std::unique_ptr<WalkerVersionsData> data = bindXml<WalkerVersionsData>(R"(<walker_versions>)"
		R"(<walk_parent id="PARENT_A"><version id="ROUTE_1"/><version id="ROUTE_2"/></walk_parent>)"
		R"(<walk_parent id="PARENT_B"><version id="ROUTE_2"/></walk_parent>)"
		R"(</walker_versions>)");
	EXPECT_EQ(data->getRouteVersionId("ROUTE_1"), std::optional<std::string>("PARENT_A"));
	EXPECT_EQ(data->getRouteVersionId("ROUTE_2"), std::optional<std::string>("PARENT_B")) << "HashMap.put: the later group wins";
	EXPECT_EQ(data->getRouteVersionId("PARENT_A"), std::nullopt) << "group ids are no route ids";
	EXPECT_EQ(WalkerVersionsData().getRouteVersionId("ROUTE_1"), std::nullopt);
}

TEST(ItemRandomBonusDataTest, TemplatesByTypeSetAndNumber) {
	using model::templates::item::bonuses::StatBonusType;
	std::unique_ptr<ItemRandomBonusData> data = bindXml<ItemRandomBonusData>(R"(<random_bonuses>)"
		R"(<random_bonus type="INVENTORY" id="1"><modifiers chance="50.0"><add name="MAXHP" value="100" bonus="true"/></modifiers>)"
		R"(<modifiers chance="25.0"><add name="MAXMP" value="100" bonus="true"/></modifiers></random_bonus>)"
		R"(<random_bonus type="POLISH" id="1"><modifiers chance="10.0"><add name="MAXHP" value="5" bonus="true"/></modifiers></random_bonus>)"
		R"(</random_bonuses>)");
	const model::templates::stats::ModifiersTemplate* second = data->getTemplate(StatBonusType::INVENTORY, 1, 2);
	ASSERT_NE(second, nullptr);
	EXPECT_FLOAT_EQ(second->getChance(), 25.0f) << "statBonusId is 1-based (Java getModifiers().get(statBonusId - 1))";
	ASSERT_NE(data->getTemplate(StatBonusType::POLISH, 1, 1), nullptr);
	EXPECT_FLOAT_EQ(data->getTemplate(StatBonusType::POLISH, 1, 1)->getChance(), 10.0f) << "the sets of each type are separate";
	EXPECT_EQ(data->getTemplate(StatBonusType::INVENTORY, 2, 1), nullptr) << "Java: null for an unknown set";
	EXPECT_THROW(static_cast<void>(data->getTemplate(StatBonusType::INVENTORY, 1, 3)), runtime::IndexOutOfBoundsException);
	EXPECT_THROW(static_cast<void>(data->getTemplate(StatBonusType::INVENTORY, 1, 0)), runtime::IndexOutOfBoundsException);
	EXPECT_EQ(ItemRandomBonusData().getTemplate(StatBonusType::POLISH, 1, 1), nullptr);
}

TEST(TemperingDataTest, TemplatesByTemperingNameOrItemGroup) {
	using model::templates::item::ItemTemplate;
	std::unique_ptr<TemperingData> data = bindXml<TemperingData>(R"(<tempering_templates>)"
		R"(<tempering_list item_group="TEST_1"><tempering_data level="1"><tempering_stat stat="PHYSICAL_ATTACK" value="10"/></tempering_data>)"
		R"(<tempering_data level="2"><tempering_stat stat="PHYSICAL_ATTACK" value="20"/><tempering_stat stat="MAXHP" value="5"/></tempering_data>)"
		R"(</tempering_list>)"
		R"(<tempering_list item_group="SWORD"><tempering_data level="1"><tempering_stat stat="MAXHP" value="1"/></tempering_data></tempering_list>)"
		R"(</tempering_templates>)");
	std::unique_ptr<ItemTemplate> named = bindXml<ItemTemplate>(R"(<item_template id="100000001" item_group="SWORD" tempering_name="TEST_1"/>)");
	std::unique_ptr<ItemTemplate> sword = bindXml<ItemTemplate>(R"(<item_template id="100000002" item_group="SWORD"/>)");
	std::unique_ptr<ItemTemplate> bow = bindXml<ItemTemplate>(R"(<item_template id="100000003" item_group="BOW"/>)");
	const auto* byName = data->getTemplates(named.get());
	ASSERT_NE(byName, nullptr);
	ASSERT_TRUE(byName->contains(2));
	EXPECT_EQ(byName->at(2)->size(), 2u) << "the tempering name wins over the item group";
	const auto* byGroup = data->getTemplates(sword.get());
	ASSERT_NE(byGroup, nullptr);
	EXPECT_EQ(byGroup->size(), 1u) << "ItemGroup.toString() is the constant name";
	EXPECT_EQ(data->getTemplates(bow.get()), nullptr) << "Java: null";
	EXPECT_THROW(static_cast<void>(data->getTemplates(nullptr)), runtime::NullPointerException);
}

TEST(RecipeDataTest, RecipesById) {
	std::unique_ptr<RecipeData> data = bindXml<RecipeData>(
		R"(<recipe_templates><recipe_template id="155000001" skillid="40009"/><recipe_template id="155000002" skillid="40001"/></recipe_templates>)");
	ASSERT_NE(data->getRecipeTemplateById(155000002), nullptr);
	EXPECT_EQ(data->getRecipeTemplateById(155000002)->getSkillId(), 40001);
	EXPECT_EQ(data->getRecipeTemplateById(155000003), nullptr) << "Java: null";
	EXPECT_EQ(RecipeData().getRecipeTemplateById(155000001), nullptr);
}

} // namespace
} // namespace aion::gameserver::dataholders
