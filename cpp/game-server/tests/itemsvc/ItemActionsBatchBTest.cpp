// M5b-3 leftovers, item actions batch B (P5-07): ApExtractAction, ExpExtractAction, AssemblyItemAction, ChargeAction with ItemChargeService,
// CompositionAction, DyeAction, InstanceTimeClear, PackAction, RideAction, SummonHouseObjectAction, TamperingAction, ToyPetSpawnAction and the
// UseTarget companion against their Java (model/templates/item/actions, services/item/ItemChargeService.java). The item rows are
// item_templates.xml's (the line in each comment; "abridged" rows drop modifiers the case does not read), the ride ride.xml's 2000000 and the
// assembly assembly_items.xml's 186000018. The player is ItemServicesTest's "Looter" (700101, ELYOS, a warrior of level 1, abyss rank 1).

#include "ItemServicesTestSupport.h"

#include <algorithm>
#include <any>
#include <chrono>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "aion/gameserver/configs/main/GSConfig.h"
#include "aion/gameserver/controllers/PlayerController.h"
#include "aion/gameserver/model/ActionStateInfo.h"
#include "aion/gameserver/dataholders/AssemblyItemsData.bind.h"
#include "aion/gameserver/dataholders/AssemblyItemsData.h"
#include "aion/gameserver/dataholders/RideData.bind.h"
#include "aion/gameserver/dataholders/RideData.h"
#include "aion/gameserver/model/TaskId.h"
#include "aion/gameserver/model/actions/PlayerMode.h"
#include "aion/gameserver/model/gameobjects/HouseObject.h"
#include "aion/gameserver/model/gameobjects/player/AbyssRank.h"
#include "aion/gameserver/model/gameobjects/player/PlayerCommonData.h"
#include "aion/gameserver/model/gameobjects/state/CreatureState.h"
#include "aion/gameserver/model/gameobjects/state/FlyState.h"
#include "aion/gameserver/model/items/ChargeInfo.h"
#include "aion/gameserver/model/skill/PlayerSkillList.h"
#include "aion/gameserver/model/templates/item/ItemTemplate.h"
#include "aion/gameserver/model/templates/item/actions/ApExtractAction.h"
#include "aion/gameserver/model/templates/item/actions/AssemblyItemAction.h"
#include "aion/gameserver/model/templates/item/actions/ChargeAction.h"
#include "aion/gameserver/model/templates/item/actions/CompositionAction.h"
#include "aion/gameserver/model/templates/item/actions/DyeAction.h"
#include "aion/gameserver/model/templates/item/actions/ExpExtractAction.h"
#include "aion/gameserver/model/templates/item/actions/InstanceTimeClear.h"
#include "aion/gameserver/model/templates/item/actions/ItemActions.h"
#include "aion/gameserver/model/templates/item/actions/PackAction.h"
#include "aion/gameserver/model/templates/item/actions/RideAction.h"
#include "aion/gameserver/model/templates/item/actions/SummonHouseObjectAction.h"
#include "aion/gameserver/model/templates/item/actions/TamperingAction.h"
#include "aion/gameserver/model/templates/item/actions/ToyPetSpawnAction.h"
#include "aion/gameserver/model/templates/item/actions/UseTargetInfo.h"
#include "aion/gameserver/network/aion/serverpackets/SM_ITEM_USAGE_ANIMATION.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/services/item/ItemChargeService.h"
#include "aion/gameserver/utils/ChatUtil.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::services::item::test {
namespace {

using namespace std::chrono_literals;
using model::gameobjects::HouseObject;
using model::templates::item::actions::UseTarget;
using network::aion::serverpackets::SM_ITEM_USAGE_ANIMATION;
using network::aion::serverpackets::SM_SYSTEM_MESSAGE;

constexpr int32_t AP_TOOL_WEAPON = 165005007;  // :836447
constexpr int32_t AP_TOOL_ARMOR = 165005008;   // :836452
constexpr int32_t WARRIORS_SWORD = 100000363;  // :1952 (abridged)
constexpr int32_t XP_EXTRACTOR = 188920000;    // :930421
constexpr int32_t GROWTH_BOX = 188052059;      // :913195
constexpr int32_t ASSEMBLY_PART_1 = 188100001; // :926170-926199, the five parts
constexpr int32_t MITHRIL_COIN = 186000018;    // :896276
constexpr int32_t CONDITIONER_1 = 168300000;   // :848488
constexpr int32_t CHARGEABLE_SWORD = 100001106; // :7708 (abridged: modifiers and uselimits dropped, so the rank allows level 2)
constexpr int32_t RANKED_SWORD = 100001105;    // :7680 (abridged: modifiers dropped; recommend_rank 4)
constexpr int32_t COMBINATION_TOOL = 165010000; // :836880
constexpr int32_t PAINT_RED = 169120000;       // :848747
constexpr int32_t DYEABLE_JACKET = 110000009;  // :188425
constexpr int32_t DRAUPNIR_SCROLL = 188600000; // :928016
constexpr int32_t WEAPON_WRAPPING = 165020008; // :836889
constexpr int32_t TAMPERING_TOOL = 166030001;  // :838047
constexpr int32_t PLUME_EARRING = 120001518;   // :639988 (abridged)
constexpr int32_t KISK = 184000001;            // :894670
constexpr int32_t HOUSE_OBJECT_ITEM = 190020003; // synthetic: the Java no-op action
constexpr int32_t ITEM = 850201;               // the used item's object id

constexpr std::string_view ACTION_ROWS = R"xml(
	<item_template id="165005007" name="Abyss Weapon 35% Extraction Tools (Superior Lvl. 40)" level="40" cName="cash_ap_r_35P_40Lv_we" mask="12414" max_stack_count="100" quality="RARE" price="5" desc="810872" activate_count="1">
		<actions>
			<apextract rate="0.35" target="WEAPON"/>
		</actions>
	</item_template>
	<item_template id="165005008" name="Abyss Armor 35% Extraction Tools (Superior Lvl. 40)" level="40" cName="cash_ap_r_35P_40Lv_ar" mask="12414" max_stack_count="100" quality="RARE" price="5" desc="810873" activate_count="1">
		<actions>
			<apextract rate="0.35" target="ARMOR"/>
		</actions>
	</item_template>
	<item_template id="100000363" name="Warrior's Sword" level="30" cName="sword_a_r2_30a" mask="203864" item_group="SWORD" item_type="ABYSS" quality="RARE" price="57600" restrict="30 30 30 30 30 30 30 30 30 30 30 30 30 30 30 30 30" desc="716704" attack_type="PHYSICAL" max_enchant="10" m_slots="4">
		<weapon_stats hit_count="2" attack_range="1500" magical_accuracy="107" parry="472" physical_accuracy="408" critical="50" attack_speed="1400" max_damage="108" min_damage="88"/>
		<acquisition type="AP" ap="4900"/>
	</item_template>
	<item_template id="188920000" name="PC XP Extraction Item01" level="60" cName="world_cash_item_exp_extraction_test01" mask="12410" max_stack_count="1000" quality="RARE" price="5" desc="811601" activate_target="STANDALONE" activate_count="1">
		<actions>
			<expextract item_id="188052059" percent="true" cost="50"/>
		</actions>
	</item_template>
	<item_template id="188052059" name="Blessing Box of Growth IV" level="1" cName="world_wrap_Cash_item_addexp_20_40" casting_delay="1000" mask="4160" quality="LEGEND" price="5" restrict="60 60 60 60 60 60 60 60 60 60 60 60 60 60 60 60 60" desc="801756" activate_target="STANDALONE" activate_count="1">
		<actions>
			<decompose/>
		</actions>
		<uselimits usedelay="5000" usedelayid="85"/>
	</item_template>
	<item_template id="188100001" name="Assembly Testing Item 01" level="50" cName="assembly_material_test_01" casting_delay="1000" mask="12414" max_stack_count="100" quality="RARE" price="1" desc="783921" activate_target="STANDALONE" activate_count="1">
		<actions>
			<assemble item="186000018"/>
		</actions>
		<uselimits usedelay="3000" usedelayid="80"/>
	</item_template>
	<item_template id="188100002" name="Assembly Testing Item 02" level="50" cName="assembly_material_test_02" casting_delay="1000" mask="12414" max_stack_count="100" quality="RARE" price="1" desc="783922" activate_target="STANDALONE" activate_count="1">
		<actions>
			<assemble item="186000018"/>
		</actions>
		<uselimits usedelay="3000" usedelayid="80"/>
	</item_template>
	<item_template id="188100003" name="Assembly Testing Item 03" level="50" cName="assembly_material_test_03" casting_delay="3000" mask="12414" max_stack_count="100" quality="RARE" price="1" desc="783923" activate_target="STANDALONE" activate_count="1">
		<actions>
			<assemble item="186000018"/>
		</actions>
		<uselimits usedelay="3000" usedelayid="80"/>
	</item_template>
	<item_template id="188100004" name="Assembly Testing Item 04" level="50" cName="assembly_material_test_04" casting_delay="3000" mask="12414" max_stack_count="100" quality="RARE" price="1" desc="783924" activate_target="STANDALONE" activate_count="1">
		<actions>
			<assemble item="186000018"/>
		</actions>
		<uselimits usedelay="3000" usedelayid="80"/>
	</item_template>
	<item_template id="188100005" name="Assembly Testing Item 05" level="50" cName="assembly_material_test_05" casting_delay="3000" mask="12414" max_stack_count="100" quality="RARE" price="1" desc="783925" activate_target="STANDALONE" activate_count="1">
		<actions>
			<assemble item="186000018"/>
		</actions>
		<uselimits usedelay="3000" usedelayid="80"/>
	</item_template>
	<item_template id="186000018" name="Mithril Coin" level="60" cName="coin_06" mask="12410" max_stack_count="10000" item_group="COINS" quality="COMMON" price="25408" desc="761266">
		<inventory id="1"/>
	</item_template>
	<item_template id="168300000" name="Test Individual Conditioning Item (Level 1)" level="1" cName="charge_item_piece_test_01" casting_delay="3000" mask="12414" max_stack_count="1000" quality="COMMON" price="900000" restrict_max="55 55 55 55 55 55 55 55 55 55 55 55 55 55 55 55 55" desc="770248" activate_count="1">
		<actions>
			<charge capacity="1"/>
		</actions>
		<improve way="1"/>
	</item_template>
	<item_template id="100001106" name="Steel Rake Sailor's Sword" level="45" cName="sword_n_r1_45b_test2" mask="138316" item_group="SWORD" quality="RARE" price="5" desc="770167" attack_type="PHYSICAL" max_enchant="10" m_slots="2">
		<weapon_stats hit_count="2" attack_range="1500" magical_accuracy="183" parry="604" physical_accuracy="606" critical="50" attack_speed="1400" max_damage="147" min_damage="119"/>
		<acquisition type="REWARD" item="186000122" count="120"/>
		<improve way="1" level="2" burn_attack="200" burn_defend="100" price1="10000" price2="200000"/>
	</item_template>
	<item_template id="100001105" name="Steel Rake Sailor's Sword" level="45" cName="sword_n_r1_45b_test1" mask="138316" item_group="SWORD" quality="RARE" price="5" desc="770167" attack_type="PHYSICAL" max_enchant="10" m_slots="2">
		<weapon_stats hit_count="2" attack_range="1500" magical_accuracy="183" parry="604" physical_accuracy="606" critical="50" attack_speed="1400" max_damage="147" min_damage="119"/>
		<acquisition type="REWARD" item="186000122" count="120"/>
		<improve way="1" level="1" burn_attack="200" burn_defend="100" price1="10000"/>
		<uselimits recommend_rank="4" rank_min="3" purchable_rank_min="4"/>
	</item_template>
	<item_template id="165010000" name="Combination Tool" level="1" cName="matter_composition_01" mask="12414" max_stack_count="100" item_group="COMBINATION" quality="COMMON" price="2000" desc="801326" activate_count="1"/>
	<item_template id="169120000" name="Paint: Red" level="1" cName="paint_red_01" mask="12414" max_stack_count="100" quality="COMMON" price="5" desc="797431" activate_count="1">
		<actions>
			<dye color="c22626"/>
		</actions>
	</item_template>
	<item_template id="110000009" name="NPC_Costume_Jacket_Kahrun_01" level="1" cName="npc_cl_torso_kahrun_01" mask="36990" item_group="TORSO" quality="COMMON" price="5" desc="793034" max_enchant="10" m_slots="1">
		<modifiers>
			<add name="PHYSICAL_DEFENSE" value="5"/>
		</modifiers>
	</item_template>
	<item_template id="188600000" name="Draupnir Cave/Sulfur Tree Nest Bonus Entry Scroll" level="1" cName="scroll_init_coolt_instance_IDDF3_Dragon" casting_delay="1000" mask="12360" max_stack_count="100" quality="COMMON" price="5" desc="770401" activate_target="STANDALONE" activate_count="1">
		<actions>
			<instancetimeclear sync_ids="2 10"/>
		</actions>
		<uselimits usedelay="21600000" usedelayid="113"/>
	</item_template>
	<item_template id="165020008" name="Weapon Wrapping Scroll (Eternal/Lv. 60 and lower)" level="60" cName="cash_pack_we_e_60a" mask="12410" max_stack_count="100" item_group="PACK_SCROLL" quality="EPIC" price="5" desc="831742" activate_count="1">
		<actions>
			<pack target="WEAPON"/>
		</actions>
	</item_template>
	<item_template id="166030001" name="Temporary Test Item" level="1" cName="matter_2stenchant_e_60" mask="12414" max_stack_count="100" item_group="TAMPERING" quality="COMMON" price="5" desc="829023" activate_count="1">
		<actions>
			<tampering/>
		</actions>
	</item_template>
	<item_template id="120001518" name="TeeShirtTest_Earrings_M_65A" level="65" cName="earring_n_m_test_feather_65a" mask="4172" item_group="PLUME" quality="MYTHIC" price="1612381" rnd_count="3" rnd_bonus="387" restrict="65 65 65 65 65 65 65 65 65 65 65 65 65 65 65 65 65" desc="839188" max_tampering="5">
	</item_template>
	<item_template id="184000001" name="BattleBindStone Elyos-Common(Test)" level="1" cName="bind_stone_light_normal_test" mask="4222" quality="COMMON" price="1" desc="700414" activate_target="STANDALONE" activate_count="1">
		<actions>
			<toypetspawn npcid="700118"/>
		</actions>
		<uselimits usedelay="15000" usedelayid="50"/>
	</item_template>
	<item_template id="190020003" name="House Object (synthetic)" level="1" cName="test_house_object" mask="4168" quality="COMMON" price="5" desc="741825">
		<actions>
			<houseobject id="3000001"/>
		</actions>
	</item_template>
)xml";

/** ride.xml :3-5 */
constexpr std::string_view RIDES_XML = R"xml(<rides>
    <ride_info id="2000000" type="0" move_speed="12.0" fly_speed="16.0" sprint_speed="15.0" start_fp="10" cost_fp="10">
        <bounds front="0.724" side="0.724" upper="2.5" altitude="0.5"/>
    </ride_info>
</rides>)xml";

/** assembly_items.xml :34 */
constexpr std::string_view ASSEMBLY_XML = R"xml(<assembly_items>
	<item id="186000018" parts="188100001 188100002 188100003 188100004 188100005" />
</assembly_items>)xml";

std::string withRows(std::string_view base, std::string_view closingTag, std::string_view rows) {
	std::string xml(base);
	xml.insert(xml.rfind(closingTag), rows);
	return xml;
}

/** DyeAction's params[0] the way CM_USE_ITEM passes it: a Ref, null for Java null */
std::any noHouseObject() {
	return std::any(runtime::Ref<HouseObject>());
}

class ItemActionsBatchBTest : public ItemServicesTest {
protected:
	void SetUp() override {
		ItemServicesTest::SetUp();
		dataholders::DataManager::ITEM_DATA.resetForTests();
		dataholders::DataManager::ITEM_DATA.publish(
			xml::bindString<dataholders::ItemData>(context, withRows(ITEM_TEMPLATES_XML, "</item_templates>", ACTION_ROWS)));
		dataholders::DataManager::RIDE_DATA.publish(xml::bindString<dataholders::RideData>(context, RIDES_XML));
		dataholders::DataManager::ASSEMBLY_ITEM_DATA.publish(xml::bindString<dataholders::AssemblyItemsData>(context, ASSEMBLY_XML));
		publishPoetaCastWorldDataOnce(); // the state changes of a mount reach ZoneUpdateService, which asks the world maps
	}

	void TearDown() override {
		ItemServicesTest::TearDown();
		dataholders::DataManager::ASSEMBLY_ITEM_DATA.resetForTests();
		dataholders::DataManager::RIDE_DATA.resetForTests();
	}

	/** The item's only action */
	template <class A>
	const A& actionOf(int32_t itemId) {
		const model::templates::item::ItemTemplate* itemTemplate = dataholders::DataManager::ITEM_DATA->getItemTemplate(itemId);
		if (itemTemplate == nullptr || itemTemplate->getActions() == nullptr)
			throw runtime::NullPointerException("no actions of item " + std::to_string(itemId));
		const auto* action = dynamic_cast<const A*>(itemTemplate->getActions()->getItemActions().front().get());
		if (action == nullptr)
			throw runtime::NullPointerException("item " + std::to_string(itemId) + " has another action");
		return *action;
	}

	int64_t count(const std::vector<std::vector<uint8_t>>& packets, const std::vector<uint8_t>& packet) {
		return std::count(packets.begin(), packets.end(), packet);
	}

	xml::LoadContext context;
};

// ---- ApExtractAction (ApExtractAction.java:31-175) ------------------------------------------------------------------------------------------

TEST_F(ItemActionsBatchBTest, AnApToolRefusesAnUnextractableAnEquippedAndAnotherKindOfItem) {
	const auto& weaponTool = actionOf<model::templates::item::actions::ApExtractAction>(AP_TOOL_WEAPON);
	Item& tool = stored(ITEM, AP_TOOL_WEAPON, 1);
	Item& potion = stored(ITEM + 1, MINOR_LIFE_POTION, 1); // mask 12414: no CAN_AP_EXTRACT bit
	clearSent();
	EXPECT_FALSE(weaponTool.canAct(player(), Ptr<Item>(tool), Ptr<Item>(potion)));
	EXPECT_EQ(sent(), cp::exactly({serialized(SM_SYSTEM_MESSAGE::STR_MSG_AP_DECOMPOSE_CANNOT(potion.getL10n()))}));

	Item& equipped = stored(ITEM + 2, WARRIORS_SWORD, 1);
	equipped.setEquipped(true);
	clearSent();
	EXPECT_FALSE(weaponTool.canAct(player(), Ptr<Item>(tool), Ptr<Item>(equipped)));
	EXPECT_EQ(sent(), cp::exactly({serialized(SM_SYSTEM_MESSAGE::STR_MSG_AP_DECOMPOSE_WRONG_EQUIPED())}));

	Item& sword = stored(ITEM + 3, WARRIORS_SWORD, 1);
	EXPECT_TRUE(weaponTool.canAct(player(), Ptr<Item>(tool), Ptr<Item>(sword))) << "level 40 >= 30, RARE >= RARE, a SWORD is a WEAPON";
	const auto& armorTool = actionOf<model::templates::item::actions::ApExtractAction>(AP_TOOL_ARMOR);
	Item& armorToolItem = stored(ITEM + 4, AP_TOOL_ARMOR, 1);
	clearSent();
	EXPECT_FALSE(armorTool.canAct(player(), Ptr<Item>(armorToolItem), Ptr<Item>(sword)));
	EXPECT_EQ(sent(), cp::exactly({serialized(SM_SYSTEM_MESSAGE::STR_MSG_AP_DECOMPOSE_CANNOT(sword.getL10n()))}));
}

TEST_F(ItemActionsBatchBTest, AnApToolTurnsTheSwordIntoItsShareOfAbyssPointsAfterThreeSeconds) {
	const auto& weaponTool = actionOf<model::templates::item::actions::ApExtractAction>(AP_TOOL_WEAPON);
	Item& tool = stored(ITEM, AP_TOOL_WEAPON, 2);
	Item& sword = stored(ITEM + 1, WARRIORS_SWORD, 1);
	player().setSkillList(model::skill::PlayerSkillList::create()); // the AP gain asks the abyss rank skills
	const int32_t apBefore = player().getAbyssRank()->getAp();
	clearSent();
	weaponTool.act(player(), Ptr<Item>(tool), Ptr<Item>(sword));
	EXPECT_EQ(sent(), cp::exactly({serialized(SM_ITEM_USAGE_ANIMATION(player().getObjectId(), ITEM, AP_TOOL_WEAPON, 3000, 0, 0))}));
	executor->advance(2999ms);
	EXPECT_TRUE(player().getInventory().getItemByObjId(ITEM + 1));
	executor->advance(1ms);
	EXPECT_EQ(tool.getItemCount(), 1);
	EXPECT_FALSE(player().getInventory().getItemByObjId(ITEM + 1)) << "the sword is deleted";
	EXPECT_EQ(player().getAbyssRank()->getAp() - apBefore, 1715) << "(int) (4900 * 0.35f)";
	const std::vector<std::vector<uint8_t>> packets = sent();
	EXPECT_EQ(count(packets, serialized(SM_SYSTEM_MESSAGE::STR_MSG_AP_DECOMPOSE_ITEM_SUCCEED(sword.getL10n()))), 1);
	EXPECT_EQ(count(packets, serialized(SM_ITEM_USAGE_ANIMATION(player().getObjectId(), ITEM, AP_TOOL_WEAPON, 0, 1, 0))), 1);
}

TEST_F(ItemActionsBatchBTest, MovingDuringTheApExtractionCancelsIt) {
	const auto& weaponTool = actionOf<model::templates::item::actions::ApExtractAction>(AP_TOOL_WEAPON);
	Item& tool = stored(ITEM, AP_TOOL_WEAPON, 1);
	Item& sword = stored(ITEM + 1, WARRIORS_SWORD, 1);
	weaponTool.act(player(), Ptr<Item>(tool), Ptr<Item>(sword));
	clearSent();
	player().getController().onMove();
	EXPECT_EQ(sent(), cp::exactly({serialized(SM_SYSTEM_MESSAGE::STR_MSG_AP_DECOMPOSE_ITEM_CANCELED(sword.getL10n())),
						  serialized(SM_ITEM_USAGE_ANIMATION(player().getObjectId(), ITEM, AP_TOOL_WEAPON, 0, 2, 0))}));
	executor->advance(3000ms);
	EXPECT_TRUE(player().getInventory().getItemByObjId(ITEM + 1)) << "the sword stays";
}

// ---- ExpExtractAction (ExpExtractAction.java:30-102) ------------------------------------------------------------------------------------------

TEST_F(ItemActionsBatchBTest, AnXpExtractorNeedsHalfTheLevelsExpAndGivesItsBox) {
	const auto& extract = actionOf<model::templates::item::actions::ExpExtractAction>(XP_EXTRACTOR);
	Item& extractor = stored(ITEM, XP_EXTRACTOR, 2);
	model::gameobjects::player::PlayerCommonData& cd = *player().getCommonData();
	const int64_t required = cd.getExpNeed() * 50 / 100;
	ASSERT_GT(required, 1);
	cd.setExp(required - 1);
	clearSent();
	EXPECT_FALSE(extract.canAct(player(), Ptr<Item>(extractor), nullptr));
	EXPECT_EQ(sent(), cp::exactly({serialized(SM_SYSTEM_MESSAGE::STR_MSG_EXP_EXTRACTION_USE_NOT_ENOUGH_EXP())}));

	cd.setExp(required + 7);
	EXPECT_TRUE(extract.canAct(player(), Ptr<Item>(extractor), nullptr));
	clearSent();
	extract.act(player(), Ptr<Item>(extractor), nullptr); // no casting_delay: finishUse at once
	EXPECT_EQ(cd.getExp(), 7);
	EXPECT_EQ(extractor.getItemCount(), 1);
	EXPECT_EQ(player().getInventory().getItemCountByItemId(GROWTH_BOX), 1);
	EXPECT_EQ(count(sent(), serialized(SM_ITEM_USAGE_ANIMATION(player().getObjectId(), ITEM, XP_EXTRACTOR, 0, 1, 0))), 1);
}

// ---- AssemblyItemAction (AssemblyItemAction.java:33-99) ------------------------------------------------------------------------------------

TEST_F(ItemActionsBatchBTest, AnAssemblyNeedsEveryPartAndTurnsThemIntoTheItem) {
	const auto& assemble = actionOf<model::templates::item::actions::AssemblyItemAction>(ASSEMBLY_PART_1);
	ASSERT_NE(assemble.getAssemblyItem(), nullptr);
	EXPECT_EQ(assemble.getAssemblyItem()->getId(), MITHRIL_COIN);
	Item& first = stored(ITEM, ASSEMBLY_PART_1, 1);
	for (int32_t part = 1; part < 4; part++)
		stored(ITEM + part, ASSEMBLY_PART_1 + part, 1);
	EXPECT_FALSE(assemble.canAct(player(), Ptr<Item>(first), nullptr)) << "the fifth part is missing";
	stored(ITEM + 4, ASSEMBLY_PART_1 + 4, 1);
	EXPECT_TRUE(assemble.canAct(player(), Ptr<Item>(first), nullptr));
	clearSent();
	assemble.act(player(), Ptr<Item>(first), nullptr);
	EXPECT_EQ(sent(), cp::exactly({serialized(SM_ITEM_USAGE_ANIMATION(player().getObjectId(), ITEM, ASSEMBLY_PART_1, 1000, 0, 0))}));
	executor->advance(1000ms);
	for (int32_t part = 0; part < 5; part++)
		EXPECT_EQ(player().getInventory().getItemCountByItemId(ASSEMBLY_PART_1 + part), 0) << "part " << part;
	const std::vector<std::vector<uint8_t>> packets = sent();
	EXPECT_EQ(count(packets, serialized(SM_SYSTEM_MESSAGE::STR_ASSEMBLY_ITEM_SUCCEEDED())), 1);
	EXPECT_EQ(count(packets, serialized(SM_SYSTEM_MESSAGE::STR_USE_ITEM(first.getL10n()))), 1);
}

TEST_F(ItemActionsBatchBTest, APartThatLeftDuringTheAssemblyBarFailsIt) {
	const auto& assemble = actionOf<model::templates::item::actions::AssemblyItemAction>(ASSEMBLY_PART_1);
	Item& first = stored(ITEM, ASSEMBLY_PART_1, 1);
	for (int32_t part = 1; part < 5; part++)
		stored(ITEM + part, ASSEMBLY_PART_1 + part, 1);
	assemble.act(player(), Ptr<Item>(first), nullptr);
	player().getInventory().decreaseByItemId(ASSEMBLY_PART_1 + 4, 1);
	clearSent();
	executor->advance(1000ms);
	EXPECT_EQ(sent(), cp::exactly({serialized(SM_ITEM_USAGE_ANIMATION(player().getObjectId(), ITEM, ASSEMBLY_PART_1, 0, 2, 0))}));
	EXPECT_EQ(player().getInventory().getItemCountByItemId(ASSEMBLY_PART_1), 1) << "nothing is spent";
}

// ---- ChargeAction (ChargeAction.java:32-117) and ItemChargeService ------------------------------------------------------------------------------

TEST_F(ItemActionsBatchBTest, AConditionerRefusesAnItemTheRankDoesNotAllowAndFindsNothingEquipped) {
	const auto& charge = actionOf<model::templates::item::actions::ChargeAction>(CONDITIONER_1);
	Item& conditioner = stored(ITEM, CONDITIONER_1, 1);
	Item& ranked = stored(ITEM + 1, RANKED_SWORD, 1);
	EXPECT_EQ(ranked.calculateAvailableChargeLevel(player()), 0) << "improvement level 1 - (recommend_rank 4 - rank 1)";
	clearSent();
	EXPECT_FALSE(charge.canAct(player(), Ptr<Item>(conditioner), Ptr<Item>(ranked)));
	EXPECT_EQ(sent(), cp::exactly({serialized(SM_SYSTEM_MESSAGE::STR_MSG_ITEM_CHARGE_FAIL_NOT_CHARGEABLE(ranked.getL10n()))}));
	clearSent();
	EXPECT_FALSE(charge.canAct(player(), Ptr<Item>(conditioner), nullptr));
	EXPECT_EQ(sent(), cp::exactly({serialized(SM_SYSTEM_MESSAGE::STR_MSG_ITEM_CHARGE_ALL_FAIL_NO_CHARGEABLE_EQUIPMENT())})) << "way 1, nothing equipped";
}

TEST_F(ItemActionsBatchBTest, AConditionerChargesTheTargetToItsCapacityAfterTheBar) {
	const auto& charge = actionOf<model::templates::item::actions::ChargeAction>(CONDITIONER_1);
	Item& conditioner = stored(ITEM, CONDITIONER_1, 2);
	Item& sword = stored(ITEM + 1, CHARGEABLE_SWORD, 1);
	EXPECT_TRUE(charge.canAct(player(), Ptr<Item>(conditioner), Ptr<Item>(sword)));
	clearSent();
	charge.act(player(), Ptr<Item>(conditioner), Ptr<Item>(sword));
	EXPECT_EQ(sent(), cp::exactly({serialized(SM_ITEM_USAGE_ANIMATION(player().getObjectId(), ITEM, CONDITIONER_1, 3000, 0, 0))}));
	executor->advance(3000ms);
	EXPECT_EQ(sword.getChargePoints(), model::items::ChargeInfo::LEVEL1) << "capacity 1";
	EXPECT_EQ(conditioner.getItemCount(), 1);
	const std::vector<std::vector<uint8_t>> packets = sent();
	EXPECT_EQ(count(packets, serialized(SM_SYSTEM_MESSAGE::STR_MSG_ITEM_CHARGE_SUCCESS(sword.getL10n(), 1))), 1);
	EXPECT_EQ(count(packets, serialized(SM_SYSTEM_MESSAGE::STR_MSG_ITEM_CHARGE_ALL_COMPLETE())), 0) << "a single target: no bulk summary";
	EXPECT_EQ(count(packets, serialized(SM_SYSTEM_MESSAGE::STR_USE_ITEM(conditioner.getL10n()))), 1);

	clearSent();
	EXPECT_FALSE(charge.canAct(player(), Ptr<Item>(conditioner), Ptr<Item>(sword)));
	EXPECT_EQ(sent(), cp::exactly({serialized(SM_SYSTEM_MESSAGE::STR_MSG_ITEM_CHARGE_FAIL_ALREADY_CHARGED(sword.getL10n(), "1"))}));
}

TEST_F(ItemActionsBatchBTest, TheChargePriceFollowsTheImprovementsTwoPrices) {
	// ItemChargeService.java:136-163 with price1 10000, price2 200000: firstLevel 5000, updateLevel round(5000 + 95000) = 100000
	Item& sword = stored(ITEM, CHARGEABLE_SWORD, 1);
	EXPECT_EQ(ItemChargeService::getPayAmountForService(sword, 1), 5000);
	EXPECT_EQ(ItemChargeService::getPayAmountForService(sword, 2), 105000) << "the full level 1 plus the update";
	sword.getConditioningInfo()->updateChargePoints(250000);
	EXPECT_EQ(ItemChargeService::getPayAmountForService(sword, 1), 2500) << "half charged";
	sword.getConditioningInfo()->updateChargePoints(250000);
	EXPECT_EQ(ItemChargeService::getPayAmountForService(sword, 2), 100000) << "level 1 full: the update alone";
	EXPECT_EQ(ItemChargeService::getPayAmountForService(sword, 3), 0) << "no such level";
	EXPECT_FALSE(ItemChargeService::processPayment(player(), 1, 1)) << "no kinah";
	EXPECT_FALSE(ItemChargeService::processPayment(player(), 3, 0)) << "an unknown way";
	EXPECT_EQ(ItemChargeService::calculateMaxChargeLevelBasedOnRank(player(), sword, 1), 1);
}

TEST_F(ItemActionsBatchBTest, ChargingIgnoringTheRankFillsTheLevelAndAnnouncesIt) {
	Item& ranked = stored(ITEM, RANKED_SWORD, 1);
	clearSent();
	EXPECT_FALSE(ItemChargeService::chargeItem(player(), ranked, 1, false, false)) << "the rank allows level 0";
	EXPECT_TRUE(ItemChargeService::chargeItem(player(), ranked, 1, true, false));
	EXPECT_EQ(ranked.getChargePoints(), model::items::ChargeInfo::LEVEL1);
	EXPECT_EQ(count(sent(), serialized(SM_SYSTEM_MESSAGE::STR_MSG_ITEM_CHARGE_SUCCESS(ranked.getL10n(), 1))), 1);
	EXPECT_FALSE(ItemChargeService::chargeItem(player(), ranked, 1, true, false)) << "already full";
}

// ---- CompositionAction (CompositionAction.java:18-61) -----------------------------------------------------------------------------------------

TEST_F(ItemActionsBatchBTest, ACompositionNeedsTheToolAndTwoEnchantmentStones) {
	const model::templates::item::actions::CompositionAction composition;
	Item& tool = stored(ITEM, COMBINATION_TOOL, 1);
	Item& first = stored(ITEM + 1, L1_ENCHANTMENT_STONE, 2);
	Item& potion = stored(ITEM + 2, MINOR_LIFE_POTION, 1);
	EXPECT_TRUE(composition.canAct(player(), tool, first, first));
	EXPECT_FALSE(composition.canAct(player(), first, first, first)) << "no combination tool";
	EXPECT_FALSE(composition.canAct(player(), tool, potion, first)) << "the first is no stone";
	EXPECT_FALSE(composition.canAct(player(), tool, first, potion)) << "the second is no stone";
	EXPECT_EQ(composition.getItemId(7), 166000007);
}

// ---- DyeAction (DyeAction.java:30-112) ----------------------------------------------------------------------------------------------------------

TEST_F(ItemActionsBatchBTest, APaintDyesAnItemAndTheRemoverTakesTheColourOff) {
	const auto& paint = actionOf<model::templates::item::actions::DyeAction>(PAINT_RED);
	EXPECT_EQ(paint.getColor(), 0xc22626);
	Item& red = stored(ITEM, PAINT_RED, 2);
	Item& jacket = stored(ITEM + 1, DYEABLE_JACKET, 1);
	clearSent();
	EXPECT_FALSE(paint.canAct(player(), Ptr<Item>(red), nullptr, {noHouseObject()}));
	EXPECT_EQ(sent(), cp::exactly({serialized(SM_SYSTEM_MESSAGE::STR_ITEM_COLOR_ERROR())})) << "nothing to dye";
	EXPECT_TRUE(paint.canAct(player(), Ptr<Item>(red), Ptr<Item>(jacket), {noHouseObject()}));
	clearSent();
	paint.act(player(), Ptr<Item>(red), Ptr<Item>(jacket), {noHouseObject()});
	EXPECT_EQ(jacket.getItemColor(), 0xc22626);
	EXPECT_EQ(jacket.getColorExpireTime(), 0) << "no minutes";
	EXPECT_EQ(red.getItemCount(), 1);
	EXPECT_EQ(count(sent(), serialized(SM_SYSTEM_MESSAGE::STR_ITEM_COLOR_CHANGE_SUCCEED(jacket.getL10n(), red.getL10n()))), 1);

	const auto& remover = actionOf<model::templates::item::actions::DyeAction>(DYE_REMOVER);
	EXPECT_EQ(remover.getColor(), std::nullopt);
	Item& removerItem = stored(ITEM + 2, DYE_REMOVER, 1);
	clearSent();
	remover.act(player(), Ptr<Item>(removerItem), Ptr<Item>(jacket), {noHouseObject()});
	EXPECT_EQ(jacket.getItemColor(), std::nullopt);
	EXPECT_EQ(count(sent(), serialized(SM_SYSTEM_MESSAGE::STR_ITEM_COLOR_REMOVE_SUCCEED(jacket.getL10n()))), 1);
}

TEST_F(ItemActionsBatchBTest, APaintLeavesAnUndyeableItemAndTheStackAlone) {
	const auto& paint = actionOf<model::templates::item::actions::DyeAction>(PAINT_RED);
	Item& red = stored(ITEM, PAINT_RED, 2);
	Item& sword = stored(ITEM + 1, TRAINING_SWORD, 1); // mask 138366: no ITEM_MASK_DYEABLE
	clearSent();
	paint.act(player(), Ptr<Item>(red), Ptr<Item>(sword), {noHouseObject()});
	EXPECT_EQ(red.getItemCount(), 2);
	EXPECT_EQ(sword.getItemColor(), std::nullopt);
	EXPECT_TRUE(sent().empty());
}

// ---- InstanceTimeClear (InstanceTimeClear.java:36-97) -----------------------------------------------------------------------------------------

TEST_F(ItemActionsBatchBTest, AnInstanceScrollAcceptsOnlyItsSyncIds) {
	const auto& clear = actionOf<model::templates::item::actions::InstanceTimeClear>(DRAUPNIR_SCROLL);
	Item& scroll = stored(ITEM, DRAUPNIR_SCROLL, 1);
	EXPECT_TRUE(clear.canAct(player(), Ptr<Item>(scroll), nullptr, {std::any(int32_t{10})}));
	EXPECT_TRUE(clear.canAct(player(), Ptr<Item>(scroll), nullptr, {std::any(int32_t{2})}));
	clearSent();
	EXPECT_FALSE(clear.canAct(player(), Ptr<Item>(scroll), nullptr, {std::any(int32_t{3})}));
	EXPECT_EQ(sent(), cp::exactly({serialized(SM_SYSTEM_MESSAGE::STR_MSG_CANT_INSTANCE_COOL_TIME_INIT())}));
}

// ---- PackAction (PackAction.java:30-126) ------------------------------------------------------------------------------------------------------

TEST_F(ItemActionsBatchBTest, AWrappingScrollWrapsAnUntradeableWeaponOnce) {
	const auto& pack = actionOf<model::templates::item::actions::PackAction>(WEAPON_WRAPPING);
	EXPECT_EQ(pack.getTarget(), UseTarget::WEAPON);
	Item& scroll = stored(ITEM, WEAPON_WRAPPING, 2);
	Item& sword = stored(ITEM + 1, TAHABATA_SWORD, 1); // EPIC, level 50, pack_count 3, mask 138316: not tradeable
	EXPECT_TRUE(pack.canAct(player(), Ptr<Item>(scroll), Ptr<Item>(sword)));
	clearSent();
	pack.act(player(), Ptr<Item>(scroll), Ptr<Item>(sword));
	EXPECT_EQ(sword.getPackCount(), 1);
	EXPECT_EQ(scroll.getItemCount(), 1);
	const std::vector<std::vector<uint8_t>> packets = sent();
	EXPECT_EQ(count(packets, serialized(SM_ITEM_USAGE_ANIMATION(player().getObjectId(), ITEM, WEAPON_WRAPPING, 0, 1, 1))), 1);
	EXPECT_EQ(count(packets, serialized(SM_SYSTEM_MESSAGE::STR_MSG_PACK_ITEM_SUCCEED(sword.getL10n()))), 1);
	EXPECT_FALSE(pack.canAct(player(), Ptr<Item>(scroll), Ptr<Item>(sword))) << "only an unpacked (negative) item may be packed again";
	sword.setPackCount(-3);
	clearSent();
	EXPECT_FALSE(pack.canAct(player(), Ptr<Item>(scroll), Ptr<Item>(sword)));
	EXPECT_EQ(sent(), cp::exactly({serialized(SM_SYSTEM_MESSAGE::STR_MSG_PACK_ITEM_CANNOT(sword.getL10n()))})) << "3 of pack_count 3 used";
}

TEST_F(ItemActionsBatchBTest, AWrappingScrollRefusesInJavasOrder) {
	const auto& pack = actionOf<model::templates::item::actions::PackAction>(WEAPON_WRAPPING);
	Item& scroll = stored(ITEM, WEAPON_WRAPPING, 1);
	clearSent();
	EXPECT_FALSE(pack.canAct(player(), Ptr<Item>(scroll), nullptr));
	EXPECT_EQ(sent(), cp::exactly({serialized(SM_SYSTEM_MESSAGE::STR_MSG_PACK_ITEM_NO_TARGET_ITEM())}));
	Item& sword = stored(ITEM + 1, TAHABATA_SWORD, 1);
	{
		AtomicConfigScope<int32_t> limit(configs::main::GSConfig::ITEM_WRAP_LIMIT, 128);
		clearSent();
		EXPECT_FALSE(pack.canAct(player(), Ptr<Item>(scroll), Ptr<Item>(sword)));
		EXPECT_EQ(sent(), cp::exactly({serialized(SM_SYSTEM_MESSAGE::STR_MSG_PACK_ITEM_CANNOT(sword.getL10n()))})) << "128 is neither <= 127 nor 255";
	}
	sword.setEquipped(true);
	clearSent();
	EXPECT_FALSE(pack.canAct(player(), Ptr<Item>(scroll), Ptr<Item>(sword)));
	EXPECT_EQ(sent(), cp::exactly({serialized(SM_SYSTEM_MESSAGE::STR_MSG_PACK_ITEM_WRONG_EQUIPED())}));
	Item& training = stored(ITEM + 2, TRAINING_SWORD, 1); // mask 138366: tradeable
	clearSent();
	EXPECT_FALSE(pack.canAct(player(), Ptr<Item>(scroll), Ptr<Item>(training)));
	EXPECT_EQ(sent(), cp::exactly({serialized(SM_SYSTEM_MESSAGE::STR_MSG_PACK_ITEM_WRONG_EXCHANGE())}));
	Item& other = stored(ITEM + 3, HOUSE_OBJECT_ITEM, 1); // mask 4168: not tradeable; item group NONE is in none of Java's groups
	clearSent();
	EXPECT_FALSE(pack.canAct(player(), Ptr<Item>(scroll), Ptr<Item>(other)));
	EXPECT_EQ(sent(), cp::exactly({serialized(SM_SYSTEM_MESSAGE::STR_MSG_PACK_ITEM_WRONG_TARGET_ITEM_CATEGORY(scroll.getL10n(), other.getL10n()))}));
}

// ---- RideAction (RideAction.java:44-149) ------------------------------------------------------------------------------------------------------

TEST_F(ItemActionsBatchBTest, ACirruspeedMountsAfterTheBarAndASecondUseDismounts) {
	const auto& ride = actionOf<model::templates::item::actions::RideAction>(CIRRUSPEED);
	ASSERT_NE(ride.getRideInfo(), nullptr);
	Item& cloud = stored(ITEM, CIRRUSPEED, 1);
	EXPECT_TRUE(ride.canAct(player(), Ptr<Item>(cloud), nullptr));
	clearSent();
	ride.act(player(), Ptr<Item>(cloud), nullptr);
	EXPECT_EQ(sent(), cp::exactly({serialized(SM_ITEM_USAGE_ANIMATION(player().getObjectId(), ITEM, CIRRUSPEED, 3000, 0, 0))}));
	executor->advance(3000ms);
	EXPECT_TRUE(player().isInPlayerMode(model::actions::PlayerMode::RIDE));
	EXPECT_TRUE(player().isInState(model::gameobjects::state::CreatureState::RESTING));
	ASSERT_TRUE(player().getRideObservers());
	EXPECT_EQ(player().getRideObservers()->size(), 3u) << "abnormal, attacked and dot-attacked";
	EXPECT_EQ(count(sent(), serialized(SM_SYSTEM_MESSAGE::STR_USE_ITEM(cloud.getL10n()))), 1);

	ride.act(player(), Ptr<Item>(cloud), nullptr);
	EXPECT_FALSE(player().isInPlayerMode(model::actions::PlayerMode::RIDE)) << "a second use dismounts at once";
	EXPECT_EQ(player().getRideObservers()->size(), 0u);
}

TEST_F(ItemActionsBatchBTest, ARestingPlayerCannotMount) {
	const auto& ride = actionOf<model::templates::item::actions::RideAction>(CIRRUSPEED);
	Item& cloud = stored(ITEM, CIRRUSPEED, 1);
	EXPECT_FALSE(ride.canAct(player(), nullptr, nullptr)) << "no item to mount with";
	player().setState(model::gameobjects::state::CreatureState::RESTING);
	clearSent();
	EXPECT_FALSE(ride.canAct(player(), Ptr<Item>(cloud), nullptr));
	EXPECT_EQ(sent(), cp::exactly({serialized(
						  SM_SYSTEM_MESSAGE::STR_MSG_CANT_RIDE(utils::ChatUtil::l10n(getL10nId(model::ActionState::RESTING))))}));
}

// ---- TamperingAction (TamperingAction.java:37-166) ---------------------------------------------------------------------------------------------

TEST_F(ItemActionsBatchBTest, TheFirstTamperingIsSafeAndRaisesTheLevel) {
	const auto& tampering = actionOf<model::templates::item::actions::TamperingAction>(TAMPERING_TOOL);
	Item& tool = stored(ITEM, TAMPERING_TOOL, 2);
	Item& sword = stored(ITEM + 1, TRAINING_SWORD, 1);
	EXPECT_FALSE(tampering.canAct(player(), Ptr<Item>(tool), Ptr<Item>(sword))) << "max_tampering 0";
	Item& earring = stored(ITEM + 2, PLUME_EARRING, 1);
	EXPECT_TRUE(tampering.canAct(player(), Ptr<Item>(tool), Ptr<Item>(earring)));
	clearSent();
	tampering.act(player(), Ptr<Item>(tool), Ptr<Item>(earring));
	EXPECT_EQ(sent(), cp::exactly({serialized(SM_ITEM_USAGE_ANIMATION(player().getObjectId(), ITEM, TAMPERING_TOOL, 5000, 0, 0))}));
	executor->advance(5000ms);
	EXPECT_EQ(earring.getTempering(), 1) << "+0 -> +1 is always safe";
	EXPECT_EQ(tool.getItemCount(), 1);
	const std::vector<std::vector<uint8_t>> packets = sent();
	EXPECT_EQ(count(packets, serialized(SM_SYSTEM_MESSAGE::STR_MSG_ITEM_AUTHORIZE_SUCCEEDED(earring.getL10n(), 1))), 1);
	EXPECT_EQ(count(packets, serialized(SM_ITEM_USAGE_ANIMATION(player().getObjectId(), ITEM, TAMPERING_TOOL, 0, 1, 0))), 1);
	earring.setTempering(5);
	EXPECT_FALSE(tampering.canAct(player(), Ptr<Item>(tool), Ptr<Item>(earring))) << "at max_tampering 5";
}

// correction of the Java code (owner's decision 2026-10-05, both branches): Java's canAct threw a NullPointerException for a use without a
// target (TamperingAction.java:34); it cannot act, as ApExtract and Pack
TEST_F(ItemActionsBatchBTest, ATamperingWithoutATargetCannotAct) {
	const auto& tampering = actionOf<model::templates::item::actions::TamperingAction>(TAMPERING_TOOL);
	Item& tool = stored(ITEM, TAMPERING_TOOL, 1);
	clearSent();
	EXPECT_FALSE(tampering.canAct(player(), Ptr<Item>(tool), nullptr));
	EXPECT_TRUE(sent().empty());
}

TEST_F(ItemActionsBatchBTest, APlumeAboveFourGainsARandomBonusPerLevelAndLosesItBelow) {
	Item& earring = stored(ITEM, PLUME_EARRING, 1);
	earring.setTempering(4);
	model::templates::item::actions::TamperingAction::setTemperingLevel(earring, player(), 7);
	EXPECT_EQ(earring.getTempering(), 7);
	EXPECT_GE(earring.getRndPlumeBonusValue(), 0);
	EXPECT_LE(earring.getRndPlumeBonusValue(), 3 * 12) << "levels 4, 5, 6: Rnd.get(0, 12) each (no TSHIRT_PHYSICAL name)";
	earring.setRndPlumeBonusValue(9);
	model::templates::item::actions::TamperingAction::setTemperingLevel(earring, player(), 0);
	EXPECT_EQ(earring.getRndPlumeBonusValue(), 0) << "at 4 or below the bonus is cleared";
}

// ---- ToyPetSpawnAction, SummonHouseObjectAction ----------------------------------------------------------------------------------------------

TEST_F(ItemActionsBatchBTest, AKiskCannotBePlacedWhileFlying) {
	const auto& spawn = actionOf<model::templates::item::actions::ToyPetSpawnAction>(KISK);
	EXPECT_EQ(spawn.getNpcId(), 700118);
	Item& kisk = stored(ITEM, KISK, 1);
	player().setFlyState(model::gameobjects::state::FlyState::FLYING);
	clearSent();
	EXPECT_FALSE(spawn.canAct(player(), Ptr<Item>(kisk), nullptr));
	EXPECT_EQ(sent(), cp::exactly({serialized(SM_SYSTEM_MESSAGE::STR_CANNOT_USE_BINDSTONE_ITEM_WHILE_FLYING())}));
}

TEST_F(ItemActionsBatchBTest, ASummonHouseObjectActionNeverActs) {
	const auto& summon = actionOf<model::templates::item::actions::SummonHouseObjectAction>(HOUSE_OBJECT_ITEM);
	EXPECT_EQ(summon.getTemplateId(), 3000001);
	Item& item = stored(ITEM, HOUSE_OBJECT_ITEM, 1);
	EXPECT_FALSE(summon.canAct(player(), Ptr<Item>(item), nullptr));
	clearSent();
	summon.act(player(), Ptr<Item>(item), nullptr);
	EXPECT_TRUE(sent().empty());
}

// ---- UseTarget (UseTarget.java:13-19) ----------------------------------------------------------------------------------------------------------

TEST(UseTargetInfoTest, ValueIsTheNameAndFromValueIsValueOf) {
	using model::templates::item::actions::fromValue;
	EXPECT_EQ(value(UseTarget::EQUIPMENT), "EQUIPMENT");
	EXPECT_EQ(fromValue<UseTarget>("WING"), UseTarget::WING);
	EXPECT_THROW(fromValue<UseTarget>("wing"), runtime::IllegalArgumentException);
}

} // namespace
} // namespace aion::gameserver::services::item::test
