#pragma once

// Test support of the craft-task lane (P5-02a, M5c stage 2, m5c-plan.md C-02 and its C-06 cases): CraftingTask driven against a real Player whose
// packets a real AionConnection captures, with the thread's Rnd seeded so every roll of analyzeInteraction and calculateCrit is known.
//
// - The Player, its connection and the packet capture are tests/cm_ak/InWorldPacketRunSupport.h's (makePlayer, TestClient), included by
//   relative path as CastTestSupport.h does. The crafting station is a real StaticObject (StaticObjectsTest.cpp builds it the same way): its
//   spawn is a fixture of this file, not shipped data - CraftingTask reads nothing of it but its object id.
// - CraftingTaskProbe derives the class under test and only widens access: the template methods are protected (AbstractCraftTask.java,
//   CraftingTask.java) and the bars live in AbstractCraftTask's protected fields. Nothing is overridden.
// - Every item template, recipe template, housing land and building below is a row of the shipped data copied verbatim
//   (game-server/data/static_data/items/item_templates.xml, recipe/recipe_templates.xml, housing/houses.xml, housing/house_buildings.xml; the
//   line of each row beside it).
// - Expected values come from the Java arithmetic of CraftingTask.java:120-173, restated below in float arithmetic, and are anchored to
//   `tools/oracle/oracle.py m5c-craft` (CraftingTaskArithmeticTest pins the restated functions to the oracle's numbers).
// - CraftingTask.onSuccessFinish's no-proc arm calls CraftService.finishCrafting (CraftingTask.java:55), which is C-01's (P5-09c), ported by
//   the craft lane beside this one and merged before this lane (m5c-plan.md §20.4: C-01, then C-02). These cases run against that body:
//   `successFinish` records what onSuccessFinish answered and what the cube gained, and `expectFinishCrafting` asserts that the product of the
//   crit count reached it (CraftService.java:70-72); TheEnhancementStoneBonus... reads the xp finishCrafting grants. An UnportedException (the
//   I-02 stub) is no accepted outcome any more: it fails the case.
// - calculateCrit reads `requester.getActiveHouse()` (CraftingTask.java:73), which loads the character's houses through HousingService, whose
//   constructor reads the database (HousingService.cpp:66-72). Its cases are database cases: a fresh database `aion_gs_test_crafting_task` on
//   the server of AION_TEST_GS_DATABASE_URL, created from game-server/sql/aion_gs.sql of the Java tree like tests/economy/P5-09a's (whose rules
//   this copy follows; a chunk's tests include no other chunk's database support). ctest runs every test case in its own process, so the
//   HousingService singleton sees exactly the rows its case wrote: run the database case alone (`--gtest_filter` one test).

#include "../../cm_ak/InWorldPacketRunSupport.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <format>
#include <fstream>
#include <memory>
#include <mutex>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "aion/commons/configuration/ConfigValue.h"
#include "aion/commons/database/Connection.h"
#include "aion/commons/database/ConnectionProperties.h"
#include "aion/commons/database/DatabaseFactory.h"
#include "aion/commons/database/PreparedStatement.h"
#include "aion/commons/database/ResultSet.h"
#include "aion/commons/utils/Exception.h"
#include "aion/commons/utils/Rnd.h"
#include "aion/gameserver/configs/main/CraftConfig.h"
#include "aion/gameserver/configs/main/RatesConfig.h"
#include "aion/gameserver/controllers/StaticObjectController.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/HouseBuildingData.bind.h"
#include "aion/gameserver/dataholders/HouseBuildingData.h"
#include "aion/gameserver/dataholders/HouseData.bind.h"
#include "aion/gameserver/dataholders/HouseData.h"
#include "aion/gameserver/dataholders/ItemData.bind.h"
#include "aion/gameserver/dataholders/ItemData.h"
#include "aion/gameserver/dataholders/ItemRestrictionCleanupData.h"
#include "aion/gameserver/dataholders/RecipeData.bind.h"
#include "aion/gameserver/dataholders/RecipeData.h"
#include "aion/gameserver/dataholders/loadingutils/LoadContext.h"
#include "aion/gameserver/dataholders/loadingutils/StaticDataLoader.h"
#include "aion/gameserver/model/gameobjects/Persistable_PersistentState.h"
#include "aion/gameserver/model/gameobjects/StaticObject.h"
#include "aion/gameserver/model/gameobjects/VisibleObject.h"
#include "aion/gameserver/model/gameobjects/player/PetCommonData.h"
#include "aion/gameserver/model/gameobjects/player/PetList.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/gameobjects/player/PlayerCommonData.h"
#include "aion/gameserver/model/house/House.h"
#include "aion/gameserver/model/items/storage/Storage.h"
#include "aion/gameserver/model/skill/PlayerSkillEntry.h"
#include "aion/gameserver/model/skill/PlayerSkillList.h"
#include "aion/gameserver/model/templates/housing/HouseType.h"
#include "aion/gameserver/model/templates/recipe/RecipeTemplate.h"
#include "aion/gameserver/model/templates/spawns/SpawnGroup.h"
#include "aion/gameserver/model/templates/spawns/SpawnTemplate.h"
#include "aion/gameserver/network/aion/SerializedBody.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/sched/DeterministicExecutor.h"
#include "aion/gameserver/skillengine/task/AbstractCraftTask.h"
#include "aion/gameserver/skillengine/task/AbstractInteractionTask.h"
#include "aion/gameserver/skillengine/task/CraftingTask.h"
#include "aion/gameserver/utils/JavaMath.h"
#include "aion/gameserver/utils/ThreadPoolManager.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::skillengine::task::crafttest {

namespace cp = network::aion::clientpackets::testing;
namespace Rnd = commons::utils::Rnd;

using gameserver::model::gameobjects::StaticObject;
using gameserver::model::gameobjects::player::Player;
using gameserver::model::templates::recipe::RecipeTemplate;
using runtime::Ptr;
using runtime::Ref;

// ------------------------------------------------------------------------------------------------------------------------- shipped rows

// the recipes below (recipe_templates.xml) and their products
inline constexpr int32_t ROAST_ININA_RECIPE = 155001381;  // cooking 40001, skillpoint 1, 2 x 160001001, one combo product 160001051
inline constexpr int32_t SOBI_TEXTILE_RECIPE = 155000832; // tailoring 40004, skillpoint 210, 152020059's two combo products RARE, LEGEND
inline constexpr int32_t LEATHER_PAD_RECIPE = 155002856;  // tailoring 40004, a LEGEND product
inline constexpr int32_t HELIOTROPE_RECIPE = 155001752;   // weaponsmithing 40002, a UNIQUE product
inline constexpr int32_t RARIFIED_ROD_RECIPE = 155101302; // weaponsmithing 40002, an EPIC product
inline constexpr int32_t HAIRPIN_RECIPE = 155101322;      // handicrafting 40008, a MYTHIC product
inline constexpr int32_t OFFERING_RECIPE = 155003492;     // weaponsmithing 40002, max_production_count="1", a RARE product
inline constexpr int32_t ARIA_RECIPE = 155000001;         // morph 40009, 3 x 152000401, no combo product
inline constexpr int32_t STEEL_NAIL_RECIPE = 155000076;   // weaponsmithing 40002, skillpoint 5, 1 x 152020002, no combo product

inline constexpr int32_t COOKING = 40001;
inline constexpr int32_t WEAPONSMITHING = 40002;
inline constexpr int32_t TAILORING = 40004;
inline constexpr int32_t MORPH = 40009;

inline constexpr int32_t ROAST_ININA = 160001001;
inline constexpr int32_t TASTY_ROAST_ININA = 160001051;
inline constexpr int32_t SOBI_TEXTILE = 152020059;
inline constexpr int32_t FINE_SOBI_TEXTILE = 152020060;
inline constexpr int32_t SUPERB_SOBI_TEXTILE = 152020061;
inline constexpr int32_t ARIA = 152000401;
inline constexpr int32_t STEEL_NAIL = 152020002;
inline constexpr int32_t LEATHER_PAD = 152020044;
inline constexpr int32_t HELIOTROPE_CRYSTAL = 152020181;
inline constexpr int32_t RARIFIED_ROD = 169405250;
inline constexpr int32_t HAIRPIN = 125003670;
inline constexpr int32_t OFFERING = 182215095;

/** item_templates.xml, verbatim: the products and combo products of the recipes above */
inline constexpr std::string_view CRAFT_ITEM_TEMPLATES_XML = R"xml(<item_templates>
	<!-- :821940 -->
	<item_template id="160001001" name="Roast Inina" level="10" cName="food_phyattack_20a" mask="12414" max_stack_count="1000" quality="COMMON" price="300" desc="729414" activate_target="STANDALONE" activate_count="1">
		<actions>
			<skilluse level="2" skillid="10054"/>
		</actions>
		<uselimits usedelay="5000" usedelayid="22"/>
	</item_template>
	<!-- :822180 -->
	<item_template id="160001051" name="Tasty Roast Inina" level="10" cName="food_r_phyattack_20a" mask="12414" max_stack_count="1000" quality="COMMON" price="300" desc="729464" activate_target="STANDALONE" activate_count="1">
		<actions>
			<skilluse level="2" skillid="10086"/>
		</actions>
		<uselimits usedelay="5000" usedelayid="22"/>
	</item_template>
	<!-- :744946 -->
	<item_template id="152020059" name="Sobi Textile" level="32" cName="ta_part_cloth3_03a" mask="12414" max_stack_count="1000" quality="COMMON" price="1080" race="ELYOS" desc="707497"/>
	<!-- :744947 -->
	<item_template id="152020060" name="Fine Sobi Textile" level="32" cName="ta_part_cloth3_03b" mask="12414" max_stack_count="1000" quality="RARE" price="2160" race="ELYOS" desc="707498"/>
	<!-- :744948 -->
	<item_template id="152020061" name="Superb Sobi Textile" level="32" cName="ta_part_cloth3_03c" mask="12414" max_stack_count="1000" quality="LEGEND" price="4320" race="ELYOS" desc="707499"/>
	<!-- :744931 -->
	<item_template id="152020044" name="Strong Superb Leather Pad" level="32" cName="ta_part_lea3_03c" mask="12414" max_stack_count="1000" quality="LEGEND" price="4920" race="ELYOS" desc="707487"/>
	<!-- :745068 -->
	<item_template id="152020181" name="Red Heliotrope Crystal" level="55" cName="combineskill_all_01a" mask="12414" max_stack_count="1000" quality="UNIQUE" price="300000" race="ELYOS" desc="759267"/>
	<!-- :850529 -->
	<item_template id="169405250" name="Reliable Rarified Rod" level="61" cName="ws_we_long_parts_N_E_61a" mask="12414" max_stack_count="1000" quality="EPIC" price="785659" desc="822987"/>
	<!-- :720800 -->
	<item_template id="125003670" name="Exalted Ceramium Hairpin" level="65" cName="ac_head_PvE_M_p_65a" mask="4862" item_group="HEAD" quality="MYTHIC" price="13436515" restrict="65 65 65 65 65 65 65 65 65 65 65 65 65 65 65 65 65" desc="823017" max_enchant_bonus="0" max_tampering="5">
		<modifiers>
			<add name="EVASION" value="115"/>
			<add name="MAGICAL_RESIST" value="65"/>
			<add name="PHYSICAL_DEFENSE" value="167"/>
			<add name="MAGIC_SKILL_BOOST_RESIST" value="49"/>
			<add name="MAXHP" value="482" bonus="true"/>
			<add name="MAGICAL_RESIST" value="62" bonus="true"/>
			<add name="MAGIC_SKILL_BOOST_RESIST" value="61" bonus="true"/>
			<add name="PHYSICAL_DEFENSE" value="165" bonus="true"/>
			<add name="PHYSICAL_CRITICAL_DAMAGE_REDUCE" value="94" bonus="true"/>
			<add name="MAGICAL_CRITICAL_DAMAGE_REDUCE" value="47" bonus="true"/>
			<rate name="FLY_SPEED" value="10" bonus="true"/>
		</modifiers>
		<actions>
			<remodel type="2"/>
		</actions>
		<disposition id="188950019" count="4"/>
	</item_template>
	<!-- :888192 -->
	<item_template id="182215095" name="Supreme Seraphim Offering" level="1" cName="quest_37503a" mask="20544" item_group="QUEST" quality="RARE" price="5" race="ELYOS" desc="1134096">
		<inventory id="2"/>
	</item_template>
	<!-- :743911 -->
	<item_template id="152000401" name="Aria" level="10" cName="herb_n_c_10a" mask="4222" max_stack_count="10000" item_group="GATHERABLE" quality="COMMON" price="5" race="ELYOS" desc="702016"/>
	<!-- :744889 -->
	<item_template id="152020002" name="Steel Nail" level="11" cName="ws_part_steel_02a" mask="12414" max_stack_count="1000" quality="COMMON" price="120" race="ELYOS" desc="707453"/>
</item_templates>)xml";

/** recipe_templates.xml, verbatim */
inline constexpr std::string_view CRAFT_RECIPE_TEMPLATES_XML = R"xml(<recipe_templates>
	<!-- :12124 -->
	<recipe_template id="155001381" nameid="731656" skillid="40001" race="ELYOS" skillpoint="1" autolearn="1" productid="160001001" quantity="2">
		<components_data>
			<component quantity="1" itemid="152001001"/>
			<component quantity="2" itemid="169400096"/>
		</components_data>
		<comboproduct itemid="160001051"/>
	</recipe_template>
	<!-- :7466 -->
	<recipe_template id="155000832" nameid="731109" skillid="40004" race="ELYOS" skillpoint="210" productid="152020059" quantity="1">
		<components_data>
			<component quantity="2" itemid="152020057"/>
		</components_data>
		<comboproduct itemid="152020060"/>
		<comboproduct itemid="152020061"/>
	</recipe_template>
	<!-- :24274 -->
	<recipe_template id="155002856" nameid="775281" skillid="40004" race="ELYOS" skillpoint="210" productid="152020044" quantity="1">
		<components_data>
			<component quantity="3" itemid="152020043"/>
			<component quantity="50" itemid="169400037"/>
		</components_data>
	</recipe_template>
	<!-- :15101 -->
	<recipe_template id="155001752" nameid="754522" skillid="40002" race="ELYOS" skillpoint="450" autolearn="6" productid="152020181" quantity="1" craft_delay_id="915501" craft_delay_time="79200">
		<components_data>
			<component quantity="1" itemid="169405023"/>
			<component quantity="1" itemid="169400132"/>
		</components_data>
	</recipe_template>
	<!-- :84584 -->
	<recipe_template id="155101302" nameid="822825" skillid="40002" race="ELYOS" skillpoint="500" productid="169405250" quantity="1">
		<components_data>
			<component quantity="20" itemid="152029289"/>
			<component quantity="45" itemid="169405232"/>
			<component quantity="14" itemid="152000918"/>
			<component quantity="14" itemid="152011102"/>
			<component quantity="29" itemid="152000222"/>
			<component quantity="16" itemid="152029272"/>
		</components_data>
	</recipe_template>
	<!-- :84784 -->
	<recipe_template id="155101322" nameid="822845" skillid="40008" race="ELYOS" skillpoint="500" productid="125003670" quantity="1">
		<components_data>
			<component quantity="1" itemid="125003439"/>
			<component quantity="29" itemid="169405237"/>
			<component quantity="16" itemid="169405256"/>
			<component quantity="8" itemid="169405255"/>
		</components_data>
	</recipe_template>
	<!-- :29505 -->
	<recipe_template id="155003492" nameid="779410" skillid="40002" race="ELYOS" skillpoint="550" productid="182215095" quantity="1" max_production_count="1">
		<components_data>
			<component quantity="3" itemid="169401022"/>
		</components_data>
	</recipe_template>
	<!-- :379 -->
	<recipe_template id="155000076" nameid="730353" skillid="40002" race="ELYOS" skillpoint="5" productid="152020002" quantity="1">
		<components_data>
			<component quantity="2" itemid="152020001"/>
		</components_data>
	</recipe_template>
	<!-- :3 -->
	<recipe_template id="155000001" nameid="730278" skillid="40009" race="ELYOS" skillpoint="1" dp="200" autolearn="1" productid="152000401" quantity="3">
		<components_data>
			<component quantity="1" itemid="152000901"/>
		</components_data>
	</recipe_template>
</recipe_templates>)xml";

/** housing/houses.xml, verbatim: a PALACE land (:3), an ESTATE land (:153), a MANSION land (:315), a HOUSE land (:591) and the STUDIO land (:617) */
inline constexpr std::string_view CRAFT_HOUSE_LANDS_XML = R"xml(<house_lands>
	<land id="325001" teleport_npc="810003" manager_npc="810017" sign_home="810007" sign_waiting="810006" sign_sale="810005" sign_nosale="810004">
		<addresses>
			<address id="10001" map="700010000" town="1001" x="696.159973" y="1999.969971" z="174.42577"/>
			<address id="10002" map="700010000" town="1001" x="609.904907" y="2057.186279" z="174.90712"/>
			<address id="10003" map="700010000" town="1001" x="779.445618" y="2061.070068" z="174.19347"/>
			<address id="10019" map="700010000" town="1002" x="80.480995" y="2055.561035" z="131.51261"/>
		</addresses>
		<buildings>
			<building id="350000" default="true"/>
			<building id="350001"/>
		</buildings>
		<sale level="50" gold_price="1000000000" point_price="0"/>
		<fee>20000000</fee>
		<caps room="false" floor="false" emblemId="2" addon="true"/>
	</land>
	<land id="326002" teleport_npc="810003" manager_npc="810018" sign_home="810007" sign_waiting="810006" sign_sale="810005" sign_nosale="810004">
		<addresses>
			<address id="6101" map="210050000" town="0" x="1132.396240" y="813.401550" z="527.33487"/>
		</addresses>
		<buildings>
			<building id="351000" default="true"/>
			<building id="351001"/>
		</buildings>
		<sale level="50" gold_price="335000000" point_price="0"/>
		<fee>4000000</fee>
		<caps room="true" floor="true" emblemId="2" addon="true"/>
	</land>
	<land id="327002" teleport_npc="810003" manager_npc="810019" sign_home="810007" sign_waiting="810006" sign_sale="810005" sign_nosale="810004">
		<addresses>
			<address id="6002" map="210040000" town="0" x="2710.913330" y="615.251404" z="355.92179"/>
			<address id="6003" map="210040000" town="0" x="2741.268555" y="575.825500" z="356.02247"/>
			<address id="6004" map="210040000" town="0" x="1769.933228" y="1632.419800" z="132.66587"/>
			<address id="6007" map="210040000" town="0" x="219.799728" y="1788.178833" z="135.10115"/>
			<address id="6008" map="210040000" town="0" x="181.902634" y="1741.180664" z="131.11818"/>
		</addresses>
		<buildings>
			<building id="352000" default="true"/>
			<building id="352001"/>
		</buildings>
		<sale level="40" gold_price="112000000" point_price="0"/>
		<fee>2000000</fee>
		<caps room="true" floor="true" emblemId="2" addon="true"/>
	</land>
	<land id="328002" teleport_npc="810003" manager_npc="810020" sign_home="810007" sign_waiting="810006" sign_sale="810005" sign_nosale="810004">
		<addresses>
			<address id="6005" map="210040000" town="0" x="1804.022461" y="1648.438599" z="129.4495"/>
			<address id="6006" map="210040000" town="0" x="1746.503296" y="1593.924927" z="129.47357"/>
			<address id="6009" map="210040000" town="0" x="263.699554" y="1765.013672" z="130.16687"/>
		</addresses>
		<buildings>
			<building id="353000" default="true"/>
			<building id="353001"/>
		</buildings>
		<sale level="40" gold_price="12000000" point_price="0"/>
		<fee>400000</fee>
		<caps room="true" floor="true" emblemId="2" addon="true"/>
	</land>
	<land id="329001" teleport_npc="810003" manager_npc="810021" sign_home="810007" sign_waiting="810006" sign_sale="810005" sign_nosale="810004">
		<addresses>
			<address id="2001" map="720010000" town="0" x="366.242615" y="295.776093" z="222.3536" exit_map="700010000" exit_x="2573.0" exit_y="1961.0" exit_z="185.0"/>
		</addresses>
		<buildings>
			<building id="355000" default="true"/>
		</buildings>
		<sale level="21" gold_price="4000000" point_price="0"/>
		<fee>0</fee>
		<caps room="true" floor="true" emblemId="0" addon="false"/>
	</land>
</house_lands>)xml";

/** housing/house_buildings.xml, verbatim: the default buildings of the five lands (the size is only here, Building.getSize) - :3, :27, :51, :75, :99 */
inline constexpr std::string_view CRAFT_HOUSE_BUILDINGS_XML = R"xml(<buildings>
    <building id="350000" type="PERSONAL_FIELD" size="PALACE" parts_match="CP_S">
        <parts>
            <roof>3550000</roof>
            <outwall>3551000</outwall>
            <frame>3552000</frame>
            <door>3553000</door>
            <garden>3556000</garden>
            <fence>3557000</fence>
            <inwall>3554000</inwall>
            <infloor>3555000</infloor>
        </parts>
    </building>
    <building id="351000" type="PERSONAL_FIELD" size="ESTATE" parts_match="CP_A">
        <parts>
            <roof>3500000</roof>
            <outwall>3501000</outwall>
            <frame>3502000</frame>
            <door>3503000</door>
            <garden>3506000</garden>
            <fence>3507000</fence>
            <inwall>3504000</inwall>
            <infloor>3505000</infloor>
        </parts>
    </building>
    <building id="352000" type="PERSONAL_FIELD" size="MANSION" parts_match="CP_B">
        <parts>
            <roof>3510000</roof>
            <outwall>3511000</outwall>
            <frame>3512000</frame>
            <door>3513000</door>
            <garden>3516000</garden>
            <fence>3517000</fence>
            <inwall>3514000</inwall>
            <infloor>3515000</infloor>
        </parts>
    </building>
    <building id="353000" type="PERSONAL_FIELD" size="HOUSE" parts_match="CP_C">
        <parts>
            <roof>3520000</roof>
            <outwall>3521000</outwall>
            <frame>3522001</frame>
            <door>3523000</door>
            <garden>3526000</garden>
            <fence>3527000</fence>
            <inwall>3524000</inwall>
            <infloor>3525000</infloor>
        </parts>
    </building>
    <building id="355000" type="PERSONAL_INS" size="STUDIO" parts_match="CP_D">
        <parts>
            <door>3533000</door>
            <inwall>3534000</inwall>
            <infloor>3535000</infloor>
        </parts>
    </building>
</buildings>)xml";

// ------------------------------------------------------------------------------------------------------------------------- the Java arithmetic

/** The largest multi `Rnd.nextFloat(1f, 2f)` answers (CraftingTask.java:131), the oracle's `next_down(2.0)` */
inline float topMulti() {
	return std::nextafter(2.0f, 0.0f);
}

/** CraftingTask.java:132-133: CraftConfig.MAX_CRAFT_FAILURE_CHANCE * Math.max(1 - skillLvlDiff * 0.015f, 0.25f), float arithmetic */
inline float failureThreshold(int32_t failureChance, int32_t diff) {
	return static_cast<float>(failureChance) * std::max(1.0f - static_cast<float>(diff) * 0.015f, 0.25f);
}

/** CraftingTask.java:152: 15 + skillLvlDiff / 3f */
inline float critBlueThreshold(int32_t diff) {
	return 15.0f + static_cast<float>(diff) / 3.0f;
}

/** CraftingTask.java:155-158: Math.round(70 + ((int) (((blue ? 100 : 0) + (((diff + 1) / 2f) + lvlBoni) * 10) * multi)) * bonusModifier) */
inline int32_t successStep(int32_t diff, bool blue, float multi, float modifier) {
	const int32_t lvlBoni = diff > 10 ? (diff - 10) * 2 : 0;
	const float base = static_cast<float>(blue ? 100 : 0) + ((static_cast<float>(diff + 1) / 2.0f) + static_cast<float>(lvlBoni)) * 10.0f;
	const auto stepBonus = static_cast<int32_t>(base * multi); // far inside the int range for every level difference of the game
	return utils::JavaMath::round(70.0f + static_cast<float>(stepBonus) * modifier);
}

/** CraftingTask.java:160-162: Math.round((limited ? 70 : 120) + ((int) (((diff + 1) / 1.5f * 10) * multi)) * bonusModifier) */
inline int32_t failureStep(int32_t diff, bool limited, float multi, float modifier) {
	const auto stepBonus = static_cast<int32_t>((static_cast<float>(diff + 1) / 1.5f * 10.0f) * multi);
	return utils::JavaMath::round(static_cast<float>(limited ? 70 : 120) + static_cast<float>(stepBonus) * modifier);
}

/** CraftingTask.java:170-171: the execution speed of the update packet */
inline int32_t executionSpeedOf(int32_t diff, float modifier) {
	const int32_t speed = modifier < 1.0f ? utils::JavaMath::round(900.0f * (2.0f - modifier)) : 900 - diff * 30;
	return std::max(speed, 300);
}

/** CraftingTask.java:172: the bar delay of the update packet */
inline int32_t barDelayOf(int32_t diff, float modifier) {
	return modifier < 1.0f ? 1200 : std::max(500, 1200 - diff * 30);
}

/** CraftingTask.java:136-149: the bonus modifier of the current item template's quality (in float, as Java's literals) */
inline constexpr float LEGEND_MODIFIER = 0.9f;
inline constexpr float UNIQUE_MODIFIER = 0.7f;
inline constexpr float EPIC_MODIFIER = 0.5f;
inline constexpr float MYTHIC_MODIFIER = 0.3f;

/** The first draws of the calling thread's generator after `Rnd::seedCurrentThreadForTests(seed)`, in the order analyzeInteraction makes them */
struct Draws {
	float multi = 0;  // Rnd.nextFloat(1f, 2f)
	float first = 0;  // the failure roll, or the CRIT_BLUE roll when no failure roll is made (skillLvlDiff >= 41)
	float second = 0; // the CRIT_BLUE roll after a failure roll
	float third = 0;  // the draw after both
};

inline Draws drawsOf(uint64_t seed) {
	Rnd::seedCurrentThreadForTests(seed);
	Draws draws;
	draws.multi = Rnd::nextFloat(1.0f, 2.0f);
	draws.first = Rnd::chance();
	draws.second = Rnd::chance();
	draws.third = Rnd::chance();
	return draws;
}

/** The chance() draws of a seeded generator (calculateCrit rolls one per proc) */
inline std::vector<float> chancesOf(uint64_t seed, size_t count) {
	Rnd::seedCurrentThreadForTests(seed);
	std::vector<float> chances;
	for (size_t i = 0; i < count; i++)
		chances.push_back(Rnd::chance());
	return chances;
}

// ------------------------------------------------------------------------------------------------------------------------- the probe

/** CraftingTask with its protected members made visible; no body is overridden */
class CraftingTaskProbe final : public CraftingTask {
	AION_MAKE_REF_FRIEND
public:
	static Ref<CraftingTaskProbe> create(Player& requester, Ptr<StaticObject> responder, const RecipeTemplate* recipeTemplate, int32_t skillLvlDiff,
		int32_t bonus) {
		return runtime::makeRef<CraftingTaskProbe>(requester, responder, recipeTemplate, skillLvlDiff, bonus);
	}

	using CraftingTask::analyzeInteraction;
	using CraftingTask::onFailureFinish;
	using CraftingTask::onInteractionAbort;
	using CraftingTask::onInteractionFinish;
	using CraftingTask::onInteractionStart;
	using CraftingTask::onSuccessFinish;
	using CraftingTask::sendInteractionUpdate;

	/** AbstractCraftTask.onInteraction, the body of every scheduled run */
	bool interact() { return onInteraction(); }

	int32_t successValue() const { return currentSuccessValue.get(); }
	int32_t failureValue() const { return currentFailureValue.get(); }
	void setBars(int32_t success, int32_t failure) {
		currentSuccessValue.set(success);
		currentFailureValue.set(failure);
	}
	CraftType type() const { return craftType.get(); }
	void setType(CraftType type) { craftType.set(type); }
	int32_t firstDelay() const { return delay.get(); }
	int32_t period() const { return interval.get(); }

	static constexpr int32_t fullBar() { return fullBarValue; }

protected:
	CraftingTaskProbe(Player& requester, Ptr<StaticObject> responder, const RecipeTemplate* recipeTemplate, int32_t skillLvlDiff, int32_t bonus)
		: CraftingTask(requester, responder, recipeTemplate, skillLvlDiff, bonus) {}
	~CraftingTaskProbe() override = default;
};

// ------------------------------------------------------------------------------------------------------------------------- the packets

/** ServerPacketsOpcodes.java:198-199: 180 SM_CRAFT_ANIMATION [S_COMBINE_OTHER], 181 SM_CRAFT_UPDATE [S_COMBINE] */
inline constexpr int32_t SM_CRAFT_ANIMATION_OPCODE = 180;
inline constexpr int32_t SM_CRAFT_UPDATE_OPCODE = 181;

/**
 * Every SM_CRAFT_UPDATE and SM_CRAFT_ANIMATION the connection queued, decoded from the bytes Java writes (SM_CRAFT_UPDATE.java:37-45: H skillId,
 * C action, D itemId, D success, D failure, D executionSpeed, D delay, then the message; SM_CRAFT_ANIMATION.java:24-29: D playerObjId,
 * D targetObjectId, H skillId, C action) into one line each, so a sequence reads like the Java body that sent it:
 * `update 40001/0 item 160001001 bar 1000/1000 speed 0 delay 0` and `animation 5001->7 skill 40001 action 0`.
 */
inline std::vector<std::string> craftPackets(const std::vector<network::aion::SerializedBody>& sent) {
	std::vector<std::string> lines;
	for (const network::aion::SerializedBody& body : sent) {
		if (body.opCode == SM_CRAFT_UPDATE_OPCODE) {
			network::test::PacketReader reader(cp::bodyOf(*body.bytes));
			const auto skillId = static_cast<uint16_t>(reader.H());
			const int32_t action = reader.C();
			const int32_t itemId = reader.D();
			const int32_t success = reader.D();
			const int32_t failure = reader.D();
			const int32_t speed = reader.D();
			const int32_t delay = reader.D();
			lines.push_back(std::format("update {}/{} item {} bar {}/{} speed {} delay {}", skillId, action, itemId, success, failure, speed, delay));
		} else if (body.opCode == SM_CRAFT_ANIMATION_OPCODE) {
			network::test::PacketReader reader(cp::bodyOf(*body.bytes));
			const int32_t player = reader.D();
			const int32_t target = reader.D();
			const auto skillId = static_cast<uint16_t>(reader.H());
			const int32_t action = reader.C();
			lines.push_back(std::format("animation {}->{} skill {} action {}", player, target, skillId, action));
		}
	}
	return lines;
}

inline std::string update(int32_t skillId, int32_t action, int32_t itemId, int32_t success, int32_t failure, int32_t speed = 0, int32_t delay = 0) {
	return std::format("update {}/{} item {} bar {}/{} speed {} delay {}", skillId, action, itemId, success, failure, speed, delay);
}

inline std::string animation(int32_t playerObjId, int32_t targetObjectId, int32_t skillId, int32_t action) {
	return std::format("animation {}->{} skill {} action {}", playerObjId, targetObjectId, skillId, action);
}

// ------------------------------------------------------------------------------------------------------------------------- configuration

/** Sets an atomic configuration field for the scope and restores the previous value */
template <class T>
class AtomicConfigScope {
public:
	AtomicConfigScope(std::atomic<T>& configValue, T value) : config(configValue), previous(configValue.load()) { config.store(value); }
	~AtomicConfigScope() { config.store(previous); }
	AtomicConfigScope(const AtomicConfigScope&) = delete;
	AtomicConfigScope& operator=(const AtomicConfigScope&) = delete;

private:
	std::atomic<T>& config;
	const T previous;
};

/** Sets a rate list (RatesConfig, one entry per membership level) for the scope and restores the previous one */
class RatesScope {
public:
	RatesScope(commons::configuration::ConfigValue<std::vector<float>>& configValue, std::vector<float> value)
		: config(configValue), previous(configValue.get()) {
		config.set(std::move(value));
	}
	~RatesScope() { config.set(previous ? *previous : std::vector<float>()); }
	RatesScope(const RatesScope&) = delete;
	RatesScope& operator=(const RatesScope&) = delete;

private:
	commons::configuration::ConfigValue<std::vector<float>>& config;
	const std::shared_ptr<const std::vector<float>> previous;
};

// ------------------------------------------------------------------------------------------------------------------------- the fixture

/** Player.postConstruct loads the pets through PlayerPetsDAO (a database); the crafters have none */
inline std::vector<Ref<gameserver::model::gameobjects::player::PetCommonData>> craftTestNoPets(Player&) {
	return {};
}

/** What CraftingTask.onSuccessFinish did (see the header comment) */
struct SuccessFinish {
	bool proc = false;           // it answered false: calculateCrit rolled a proc and onInteractionStart restarted the bar
	bool finishCrafting = false; // it answered true, after CraftService.finishCrafting returned
	int64_t productAdded = 0;    // what the requester's cube gained of the product the case names
};

class CraftingTaskTest : public cp::InWorldPacketTest {
protected:
	void SetUp() override {
		InWorldPacketTest::SetUp();
		// the base fixture's DeterministicExecutor again, with a handle the scheduled cases advance (nothing is scheduled yet)
		utils::ThreadPoolManager::installBackend(nullptr);
		auto backend = std::make_unique<runtime::DeterministicExecutor>(clock, 29);
		executor = backend.get(); // owned by ThreadPoolManager until the base TearDown installs no backend
		utils::ThreadPoolManager::installBackend(std::move(backend));
		xml::LoadContext itemContext;
		dataholders::DataManager::ITEM_DATA.publish(xml::bindString<dataholders::ItemData>(itemContext, CRAFT_ITEM_TEMPLATES_XML));
		xml::LoadContext recipeContext;
		dataholders::DataManager::RECIPE_DATA.publish(xml::bindString<dataholders::RecipeData>(recipeContext, CRAFT_RECIPE_TEMPLATES_XML));
		// the item info blob of the product's SM_INVENTORY_ADD_ITEM asks it once C-01 adds the product (GeneralInfoBlobEntry); no entries
		dataholders::DataManager::ITEM_CLEAN_UP.publish(std::make_unique<dataholders::ItemRestrictionCleanupData>());
		gameserver::model::gameobjects::player::PetList::setPlayerPetsLoaderForTests(&craftTestNoPets);
		// the crafting station: a real StaticObject with its own object id (CraftService.startCrafting hands one to the task)
		stationGroup = gameserver::model::templates::spawns::SpawnGroup::create(210010000, 700001, 0, nullptr);
		stationSpawn = gameserver::model::templates::spawns::SpawnTemplate::create(*stationGroup, 101.0f, 100.0f, 50.0f, int8_t{0}, 0, std::nullopt, 0);
		station = gameserver::model::gameobjects::VisibleObject::create<StaticObject>(std::make_unique<controllers::StaticObjectController>(),
			*stationSpawn, nullptr);
		crafter = cp::makePlayer(5301, 9531, "Crafter");
		// the crafting skills at a level that finishCrafting's skill xp does not level up (C-01 in the tree; PlayerSkillList.java:84-120)
		learn(crafter, 20);
		client = std::make_unique<cp::TestClient>();
		client->enterWorld(crafter);
		(*client)->clearSent();
	}

	void TearDown() override {
		if (crafter.player) {
			if (runtime::Ptr<AbstractInteractionTask> pending = crafter.player->getInteractionTask())
				pending->stop(); // a started task and the player hold each other (cycles.toml: Player.interactionTask)
			crafter.player->setClientConnection(nullptr);
		}
		client.reset();
		crafter = {};
		station = nullptr;
		stationSpawn = nullptr;
		stationGroup = nullptr;
		skillEntries.clear();
		gameserver::model::gameobjects::player::PetList::setPlayerPetsLoaderForTests(nullptr);
		InWorldPacketTest::TearDown(); // retires the executor, which restores the thread's Rnd generator (DeterministicExecutor.h:28-30)
		dataholders::DataManager::ITEM_CLEAN_UP.resetForTests();
		dataholders::DataManager::RECIPE_DATA.resetForTests();
		dataholders::DataManager::ITEM_DATA.resetForTests();
	}

	/** The crafting skills and the morph skill at the given level (Java PlayerSkillListDAO rows) */
	void learn(cp::PlayerFixture& fixture, int32_t level) {
		std::vector<Ptr<gameserver::model::skill::PlayerSkillEntry>> entries;
		for (int32_t skillId : {COOKING, 40002, 40003, TAILORING, 40007, 40008, 40010, MORPH}) {
			Ref<gameserver::model::skill::PlayerSkillEntry> entry = gameserver::model::skill::PlayerSkillEntry::create(skillId, level, 0,
				gameserver::model::gameobjects::Persistable_PersistentState::NOACTION);
			entries.push_back(Ptr<gameserver::model::skill::PlayerSkillEntry>(entry));
			skillEntries.push_back(std::move(entry));
		}
		fixture.player->setSkillList(gameserver::model::skill::PlayerSkillList::create(entries));
	}

	static const RecipeTemplate* recipe(int32_t recipeId) {
		const RecipeTemplate* recipeTemplate = dataholders::DataManager::RECIPE_DATA->getRecipeTemplateById(recipeId);
		EXPECT_NE(recipeTemplate, nullptr) << recipeId;
		return recipeTemplate;
	}

	/** A task of the crafter at the station, as CraftService.startCrafting creates it (CraftService.java:123) */
	Ref<CraftingTaskProbe> newTask(int32_t recipeId, int32_t skillLvlDiff, int32_t bonus = 0) {
		return newTaskOf(*crafter.player, recipeId, skillLvlDiff, bonus);
	}

	Ref<CraftingTaskProbe> newTaskOf(Player& player, int32_t recipeId, int32_t skillLvlDiff, int32_t bonus = 0) {
		return CraftingTaskProbe::create(player, Ptr<StaticObject>(station), recipe(recipeId), skillLvlDiff, bonus);
	}

	/** The morph task, which has no station (CraftService.java:123 casts the missing target: null) */
	Ref<CraftingTaskProbe> newMorphTask(int32_t skillLvlDiff = 0) {
		return CraftingTaskProbe::create(*crafter.player, nullptr, recipe(ARIA_RECIPE), skillLvlDiff, 0);
	}

	std::vector<std::string> sentCraftPackets() { return craftPackets((*client)->sent()); }

	void clearSent() { (*client)->clearSent(); }

	int32_t crafterId() const { return crafter.player->getObjectId(); }
	int32_t stationId() const { return station->getObjectId(); }

	/** Runs onSuccessFinish and records what it answered, with the count of `productId` the requester's cube gained meanwhile */
	static SuccessFinish successFinish(CraftingTaskProbe& craftingTask, Player& requester, int32_t productId) {
		SuccessFinish finish;
		const int64_t before = requester.getInventory().getItemCountByItemId(productId);
		const bool finished = craftingTask.onSuccessFinish();
		finish.proc = !finished;
		finish.finishCrafting = finished;
		finish.productAdded = requester.getInventory().getItemCountByItemId(productId) - before;
		return finish;
	}

	SuccessFinish successFinish(CraftingTaskProbe& craftingTask, int32_t productId) { return successFinish(craftingTask, *crafter.player, productId); }

	/**
	 * onSuccessFinish ended in CraftService.finishCrafting(requester, recipeTemplate, critCount, bonus) (CraftingTask.java:55): the product of
	 * that crit count reached the cube (CraftService.java:70-72: the combo product after a proc, else the product, recipe quantity)
	 */
	static void expectFinishCrafting(const SuccessFinish& finish, int64_t quantity) {
		EXPECT_FALSE(finish.proc);
		EXPECT_TRUE(finish.finishCrafting);
		EXPECT_EQ(finish.productAdded, quantity) << "CraftService.finishCrafting adds the product of the crit count";
	}

	/** onSuccessFinish rolled a proc: it restarted the bar, answered false and never reached finishCrafting */
	static void expectProc(const SuccessFinish& finish) {
		EXPECT_TRUE(finish.proc);
		EXPECT_FALSE(finish.finishCrafting);
		EXPECT_EQ(finish.productAdded, 0);
	}

	/** The xp of the crafter's skill (PlayerSkillEntry.currentXp, which CraftService.finishCrafting raises through addSkillXp) */
	int32_t skillXp(int32_t skillId) const { return crafter.player->getSkillList()->getSkillEntry(skillId)->getCurrentXp(); }

	/** The crafter's character exp (PlayerCommonData.exp, which finishCrafting raises by the xp reward when the skill xp was granted) */
	int64_t characterExp() const { return crafter.player->getCommonData()->getExp(); }

	runtime::DeterministicExecutor* executor = nullptr;
	Ref<gameserver::model::templates::spawns::SpawnGroup> stationGroup;
	Ref<gameserver::model::templates::spawns::SpawnTemplate> stationSpawn;
	Ref<StaticObject> station;
	cp::PlayerFixture crafter;
	std::vector<Ref<gameserver::model::skill::PlayerSkillEntry>> skillEntries;
	std::unique_ptr<cp::TestClient> client;
};

// ------------------------------------------------------------------------------------------------------------------------- the database

inline constexpr std::string_view TEST_DATABASE = "aion_gs_test_crafting_task";

inline std::string env(const char* name) {
	const char* value = std::getenv(name); // NOLINT(concurrency-mt-unsafe): read by the test process before threads start
	return value ? value : "";
}

/** @return true if the database tests are enabled (AION_TEST_GS_DATABASE_URL is set) */
inline bool isDatabaseEnabled() {
	return !env("AION_TEST_GS_DATABASE_URL").empty();
}

inline std::string databaseUser() {
	std::string value = env("AION_TEST_GS_DATABASE_USER");
	return value.empty() ? "root" : value;
}

inline std::string databasePassword() {
	return env("AION_TEST_GS_DATABASE_PASSWORD");
}

/** @return the JDBC URL of the environment with its database replaced by `database` (the query string is kept) */
inline std::string urlWithDatabase(std::string_view database) {
	std::string url = env("AION_TEST_GS_DATABASE_URL");
	const size_t hostStart = url.find("//");
	const size_t query = url.find('?', hostStart == std::string::npos ? 0 : hostStart + 2);
	const size_t pathStart = url.find('/', hostStart == std::string::npos ? 0 : hostStart + 2);
	const size_t hostEnd = query == std::string::npos ? url.size() : query;
	std::string base = url.substr(0, pathStart != std::string::npos && pathStart < hostEnd ? pathStart : hostEnd);
	std::string suffix = query == std::string::npos ? "" : url.substr(query);
	return base + "/" + std::string(database) + suffix;
}

/** Splits an SQL script into statements (quotes, "-- " / "#" / block comments; the rules of tests/dao/DaoTestDatabase.h) */
inline std::vector<std::string> splitSqlStatements(std::string_view script) {
	std::vector<std::string> statements;
	std::string current;
	auto flush = [&] {
		size_t begin = current.find_first_not_of(" \t\r\n");
		if (begin != std::string::npos) {
			size_t end = current.find_last_not_of(" \t\r\n");
			statements.push_back(current.substr(begin, end - begin + 1));
		}
		current.clear();
	};
	for (size_t i = 0; i < script.size(); i++) {
		char c = script[i];
		if (c == '\'' || c == '"' || c == '`') {
			size_t end = i + 1;
			while (end < script.size() && script[end] != c) {
				if (script[end] == '\\' && c != '`')
					end++;
				end++;
			}
			end = std::min(end, script.size() - 1);
			current.append(script.substr(i, end - i + 1));
			i = end;
		} else if (c == '-' && i + 1 < script.size() && script[i + 1] == '-' &&
			(i + 2 == script.size() || script[i + 2] == ' ' || script[i + 2] == '\t' || script[i + 2] == '\r' || script[i + 2] == '\n')) {
			while (i < script.size() && script[i] != '\n')
				i++;
			current += '\n';
		} else if (c == '#') {
			while (i < script.size() && script[i] != '\n')
				i++;
			current += '\n';
		} else if (c == '/' && i + 1 < script.size() && script[i + 1] == '*') {
			size_t end = script.find("*/", i + 2);
			i = end == std::string_view::npos ? script.size() : end + 1;
			current += ' ';
		} else if (c == ';') {
			flush();
		} else {
			current += c;
		}
	}
	flush();
	return statements;
}

/** Takes the process lock, recreates the test database from aion_gs.sql and initializes DatabaseFactory with it. Runs once per process. */
inline void setUpDatabaseOnce() {
	static std::once_flag once;
	std::call_once(once, [] {
		using commons::database::Connection;
		using commons::database::ConnectionProperties;
		// the lock connection stays open until the process exits: the server releases the named lock when it closes
		static Connection* lockConnection =
			Connection::open(ConnectionProperties::parse(env("AION_TEST_GS_DATABASE_URL"), databaseUser(), databasePassword())).release();
		auto lock = lockConnection->prepareStatement("SELECT GET_LOCK(?, 900)");
		lock->setString(1, std::string(TEST_DATABASE));
		auto locked = lock->executeQuery();
		if (!locked->next() || locked->getInt(1) != 1)
			throw commons::utils::IllegalStateException("Could not acquire the database lock " + std::string(TEST_DATABASE));
		lockConnection->executeSimple("DROP DATABASE IF EXISTS `" + std::string(TEST_DATABASE) + "`");
		lockConnection->executeSimple("CREATE DATABASE `" + std::string(TEST_DATABASE) + "` CHARACTER SET utf8mb4");

		std::unique_ptr<Connection> schema =
			Connection::open(ConnectionProperties::parse(urlWithDatabase(TEST_DATABASE), databaseUser(), databasePassword()));
		std::ifstream in(std::filesystem::path(AION_GAMESERVER_JAVA_DIR) / "sql" / "aion_gs.sql", std::ios::binary);
		if (!in)
			throw commons::utils::IOException("Cannot read aion_gs.sql");
		std::stringstream script;
		script << in.rdbuf();
		for (const std::string& statement : splitSqlStatements(script.str()))
			schema->executeSimple(statement);
		commons::database::DatabaseFactory::init(urlWithDatabase(TEST_DATABASE), databaseUser(), databasePassword(), 10, 5000);
	});
}

/** Executes a statement without parameters on a pooled connection */
inline void execute(std::string_view sql) {
	auto con = commons::database::DatabaseFactory::getConnection();
	con->executeSimple(sql);
}

/** A minimal players row: HousingService revokes the houses of owners missing from `players` (HousingService.java:63-72) */
inline void insertPlayer(int32_t id, std::string_view name, int32_t accountId) {
	execute(std::format("INSERT INTO players (id, name, account_id, account_name, x, y, z, heading, world_id, gender, race, player_class, exp) "
						"VALUES ({}, '{}', {}, 'account{}', 1, 2, 3, 4, 210010000, 'MALE', 'ELYOS', 'WARRIOR', 0)",
		id, name, accountId, accountId));
}

/** A houses row as HousesDAO reads it: the building at the address, owned since the acquire time (settings 257: owner name shown, door open) */
inline void insertHouse(int32_t id, int32_t playerId, int32_t buildingId, int32_t address) {
	execute(std::format("INSERT INTO houses (id, player_id, building_id, address, acquire_time, settings) VALUES ({}, {}, {}, {}, "
						"'2025-01-01 10:00:00', 257)",
		id, playerId, buildingId, address));
}

} // namespace aion::gameserver::skillengine::task::crafttest
