// M5d E-09 (m5d-plan.md §7, §18.3; the dialog-and-rewards lane's file lease on services/reward/BonusService.* of P5-09a): BonusService, all
// three bodies, against BonusService.java:27-81. QuestService.getRewardItems asks getQuestBonus for every quest with a <bonus>
// (QuestService.java:200-203), which is 760 XML quests, the 574 work orders among them (m5d-plan.md §18.2).
// - getBonusGroups: the six implemented types (EVENTS, FOOD, MANASTONE, MEDICINE, MEDAL, TASK) name their groups of ItemGroupsData (one quest
//   of each type below), MOVIE has none, and every other type the data names has none and is logged as not implemented (BOSS below). NONE's
//   arm (none, silently) is not driven: <bonus>'s type is a required attribute (QuestBonuses.java), no quest of quest_data.xml names NONE, and
//   getBonusGroups is private.
// - getMatchingItemsOfRandomGroup: a group chosen by chance and removed (Chance.selectElement(groups, true)), its entries filtered by
//   ItemRaceEntry.matches (the race of the player and of the item, the bonus level, a craft entry's skill and skill points), and the next group
//   tried while the filter leaves nothing.
// - getQuestBonus: nothing without a <bonus>, else one entry of the matching ones chosen by chance, as a new QuestItems of its id and count.
// Chance.selectElement is random (Rnd), so the cases draw over fixed seeds and assert membership and the whole set of outcomes, not an identity.
//
// Every row is the shipped data's, verbatim (file:line beside each): the quests of quest_data/quest_data.xml, the item groups of
// items/item_groups.xml and the item templates of items/item_templates.xml. The medal group is complete. The four craft groups are cut down to
// the rows of skill 40007 (the craft of 5400 and 6400) around skill point 1: every craft_bundles and craft_recipes row of skill 40007 that
// covers skill point 1 (lines 423-424, 553-554, 685-686, 2516-2517; four each) and the first two rows of craft_materials and craft_shop,
// neither of which covers it. That no craft_materials or craft_shop row of skill 40007 covers skill point 1 is a property of the whole file
// (checked with a throw-away ElementTree script over items/item_groups.xml: 5400 and 6400 are the only two of the 574 TASK quests whose skill
// has matches in craft_bundles and craft_recipes only), so the cut keeps the case the shipped data has. The eighteen pet food groups are cut to
// their element: ItemGroupsData.afterUnmarshal reads each of them (Java's NullPointerException for a missing one), and no case reads a row.
// The groups of the other four implemented types are cut to the rows their case needs, each kept in data order: of manastones_common and
// manastones_rare the first two rows of item level 20 and a row of another level (the row before them, 10, and the first of level 30); of food
// the first two Elyos rows of level 20, the first Elyos row of level 30 (an item of template level 20) and the first Asmodian row of level 20;
// of the three medicine groups every row of level 20 and the first row of level 30 of each (in medicine_rare the one of template level 20);
// of events every row of level 1 and the first of level 2. The gathering, enchant and boss groups are left out: getBonusGroups never returns
// them (their arms are commented out, BonusService.java:56-61).

#include <gtest/gtest.h>

#include "EconomyTestSupport.h"
#include "../../support/NetworkTestSupport.h"

#include <cstdint>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "aion/commons/utils/Rnd.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/ItemData.bind.h"
#include "aion/gameserver/dataholders/ItemData.h"
#include "aion/gameserver/dataholders/ItemGroupsData.bind.h"
#include "aion/gameserver/dataholders/ItemGroupsData.h"
#include "aion/gameserver/dataholders/QuestsData.bind.h"
#include "aion/gameserver/dataholders/QuestsData.h"
#include "aion/gameserver/model/Race.h"
#include "aion/gameserver/model/templates/QuestTemplate.h"
#include "aion/gameserver/model/templates/itemgroups/ItemRaceEntry.h"
#include "aion/gameserver/model/templates/quest/QuestItems.h"
#include "aion/gameserver/services/reward/BonusService.h"

namespace aion::gameserver::economy::test {
namespace {

namespace Rnd = commons::utils::Rnd;
using model::Race;
using model::templates::itemgroups::ItemRaceEntry;
using model::templates::quest::QuestItems;
using services::reward::BonusService;

const char* BONUS_SERVICE_LOGGER = "com.aionemu.gameserver.services.reward.BonusService";

constexpr int32_t SLEEPING_ON_THE_JOB = 1101; // no <bonus>
constexpr int32_t NEW_MANASTONES_FOR_OLD = 1161; // MANASTONE, level 20
constexpr int32_t MORE_MANDURI_FRILLNECKS = 1305; // FOOD, level 20
constexpr int32_t KERUBIAN_HORNS = 1318; // MEDICINE, level 20, PC_ALL
constexpr int32_t SILVER_FOR_THE_FOUNTAIN_ELYOS = 1717; // MEDAL, level 1
constexpr int32_t DEFEAT_LOWER_GENERALS = 1737; // BOSS, level 40
constexpr int32_t SILVER_FOR_THE_FOUNTAIN_ASMODIANS = 2717; // MEDAL, level 1
constexpr int32_t LAMPLIGHT_SUPPLIES_ELYOS = 5400; // TASK, skill 40007, skill point 1
constexpr int32_t LAMPLIGHT_SUPPLIES_ASMODIANS = 6400; // TASK, skill 40007, skill point 1
constexpr int32_t GRITTY_ININA_SUPPLIES = 5500; // TASK, skill 40001 (cooking), skill point 1
constexpr int32_t CHASING_THE_GRANKERS = 9548; // MOVIE
constexpr int32_t CYGNEA_FOUNTAIN_OF_LUCK = 15205; // MEDAL, level 2, PC_ALL
constexpr int32_t SYMPHONY_OF_LEGEND_FIRST_MOVEMENT = 80673; // EVENTS, level 1, PC_ALL

/** quest_data/quest_data.xml, verbatim rows */
constexpr std::string_view QUEST_DATA_XML = R"xml(<quests>
	<!-- :895-897 -->
	<quest id="1101" name="Sleeping on the Job" nameId="1102201" quest_zone="Poeta" minlevel_permitted="1" max_repeat_count="1" can_report="true" race_permitted="ELYOS" category="IMPORTANT">
		<rewards gold="120" exp="130"/>
	</quest>
	<!-- :1381-1389 -->
	<quest id="1161" name="[Manastone] New Manastones for Old" nameId="1102331" quest_zone="Verteron" minlevel_permitted="10" max_repeat_count="255" race_permitted="ELYOS" category="QUEST">
		<collect_items>
			<collect_item item_id="167000258" count="1"/>
			<collect_item item_id="167000260" count="1"/>
			<collect_item item_id="167000265" count="1"/>
		</collect_items>
		<rewards exp="450"/>
		<bonus level="20" type="MANASTONE"/>
	</quest>
	<!-- :1960-1970 -->
	<quest id="1305" name="More Manduri Frillnecks" nameId="1102405" quest_zone="Eltnen" minlevel_permitted="20" max_repeat_count="10" reward_repeat_count="10" can_report="true" cannot_share="true" race_permitted="ELYOS" category="QUEST">
		<rewards gold="2800" exp="66900"/>
		<bonus level="20" type="FOOD"/>
		<extended_rewards gold="5000">
			<reward_item item_id="188050588" count="1"/>
		</extended_rewards>
		<quest_kill step="0" var="0" count="10" npc_ids="211687 211688" seq="0"/>
		<start_conditions>
			<finished quest_id="1421"/>
		</start_conditions>
	</quest>
	<!-- :2081-2094 -->
	<quest id="1318" name="Kerubian Horns" nameId="1102418" quest_zone="Eltnen" minlevel_permitted="22" max_repeat_count="1" cannot_share="true" race_permitted="PC_ALL" category="QUEST">
		<collect_items>
			<collect_item item_id="182201352" count="10"/>
		</collect_items>
		<rewards gold="19500" exp="70068">
			<reward_item item_id="188050584" count="3"/>
		</rewards>
		<bonus level="20" type="MEDICINE"/>
		<quest_drop npc_id="210774" item_id="182201352" chance="80"/>
		<quest_drop npc_id="210783" item_id="182201352" chance="80"/>
		<start_conditions>
			<finished quest_id="1317"/>
		</start_conditions>
	</quest>
	<!-- :6332-6338 -->
	<quest id="1717" name="Silver for the Fountain" nameId="1104517" quest_zone="Reshanta" minlevel_permitted="25" max_repeat_count="255" cannot_share="true" race_permitted="ELYOS" extra_category="COIN_QUEST" category="QUEST" restricted="true">
		<inventory_items>
			<inventory_item item_id="186000031" count="1"/>
		</inventory_items>
		<rewards exp="1500"/>
		<bonus level="1" type="MEDAL"/>
	</quest>
	<!-- :6495-6504 -->
	<quest id="1737" name="[Alliance] Defeat Lower Generals" nameId="1104537" quest_zone="Reshanta" minlevel_permitted="37" max_repeat_count="255" cannot_share="true" race_permitted="ELYOS" category="QUEST" target="ALLIANCE" restricted="true">
		<rewards exp="1692000" ap="400">
			<reward_item item_id="188051194" count="1"/>
		</rewards>
		<bonus level="40" type="BOSS"/>
		<quest_kill step="0" var="0" count="7" npc_ids="263006 263007 263008 263009 263010 263011 263012 263013 263014 263015 263306 263307 263308 263309 263310 263311 263312 263313 263314 263315 264506 264507 264508 264509 264510 264511 264512 264513 264514 264515" seq="0"/>
		<start_conditions>
			<finished quest_id="1736"/>
		</start_conditions>
	</quest>
	<!-- :15818-15824 -->
	<quest id="2717" name="Silver for the Fountain" nameId="1104817" quest_zone="Reshanta" minlevel_permitted="25" max_repeat_count="255" cannot_share="true" race_permitted="ASMODIANS" extra_category="COIN_QUEST" category="QUEST" restricted="true">
		<inventory_items>
			<inventory_item item_id="186000031" count="1"/>
		</inventory_items>
		<rewards exp="1500"/>
		<bonus level="1" type="MEDAL"/>
	</quest>
	<!-- :28423-28432 -->
	<quest id="5400" name="Lamplight Supplies" nameId="1190164" minlevel_permitted="9" maxlevel_permitted="65" max_repeat_count="255" cannot_share="true" race_permitted="ELYOS" combineskill="40007" combine_skillpoint="1" category="TASK">
		<collect_items>
			<collect_item item_id="182290481" count="3"/>
		</collect_items>
		<rewards/>
		<bonus type="TASK"/>
		<quest_work_items>
			<quest_work_item item_id="182290164" count="4"/>
		</quest_work_items>
	</quest>
	<!-- :28855-28864 -->
	<quest id="5500" name="Gritty Inina Supplies" nameId="1190205" minlevel_permitted="9" maxlevel_permitted="65" max_repeat_count="255" cannot_share="true" race_permitted="ELYOS" combineskill="40001" combine_skillpoint="1" category="TASK">
		<collect_items>
			<collect_item item_id="182290522" count="3"/>
		</collect_items>
		<rewards/>
		<bonus type="TASK"/>
		<quest_work_items>
			<quest_work_item item_id="182290205" count="4"/>
		</quest_work_items>
	</quest>
	<!-- :31369-31378 -->
	<quest id="6400" name="Lamplight Supplies" nameId="1195165" minlevel_permitted="9" maxlevel_permitted="65" max_repeat_count="255" cannot_share="true" race_permitted="ASMODIANS" combineskill="40007" combine_skillpoint="1" category="TASK">
		<collect_items>
			<collect_item item_id="182291481" count="3"/>
		</collect_items>
		<rewards/>
		<bonus type="TASK"/>
		<quest_work_items>
			<quest_work_item item_id="182291164" count="4"/>
		</quest_work_items>
	</quest>
	<!-- :32645-32652 -->
	<quest id="9548" name="[Event] Chasing the Grankers" nameId="1112019" quest_zone="Event" minlevel_permitted="10" max_repeat_count="255" cannot_share="true" race_permitted="ELYOS" category="EVENT">
		<collect_items>
			<collect_item item_id="182206048" count="5"/>
		</collect_items>
		<rewards exp="1225"/>
		<bonus type="MOVIE"/>
		<quest_drop npc_id="214532" item_id="182206048" chance="50"/>
	</quest>
	<!-- :43577-43586 -->
	<quest id="15205" name="Cygnea Fountain of Luck" nameId="1801259" quest_zone="Cygnea" minlevel_permitted="55" max_repeat_count="255" cannot_share="true" race_permitted="PC_ALL" extra_category="COIN_QUEST" category="QUEST">
		<collect_items>
			<collect_item item_id="186000096" count="1"/>
		</collect_items>
		<inventory_items>
			<inventory_item item_id="186000096" count="1"/>
		</inventory_items>
		<rewards/>
		<bonus level="2" type="MEDAL"/>
	</quest>
	<!-- :81913-81922 -->
	<quest id="80673" name="[Event] Verify Symphony of Legend - The First Movement" nameId="1800533" quest_zone="Event" minlevel_permitted="10" max_repeat_count="255" cannot_share="true" race_permitted="PC_ALL" category="EVENT">
		<collect_items>
			<collect_item item_id="188100252" count="10"/>
		</collect_items>
		<rewards/>
		<bonus level="1" type="EVENTS"/>
		<start_conditions>
			<finished quest_id="80677"/>
		</start_conditions>
	</quest>
</quests>)xml";

/** items/item_templates.xml, verbatim rows: the items of the group rows below */
constexpr std::string_view MEDAL_ITEM_TEMPLATES_XML = R"xml(
	<!-- :896295 -->
	<item_template id="186000031" name="Silver Medal" level="40" cName="medal_02" mask="12414" max_stack_count="1000" item_group="MEDALS" quality="RARE" price="5000" desc="739362">
		<inventory id="1"/>
	</item_template>
	<!-- :896292 -->
	<item_template id="186000030" name="Gold Medal" level="50" cName="medal_01" mask="12414" max_stack_count="1000" item_group="MEDALS" quality="RARE" price="5000" desc="739361">
		<inventory id="1"/>
	</item_template>
	<!-- :878581 -->
	<item_template id="182202156" name="Quartz of Virtue" level="1" cName="quest_1718a" mask="20545" item_group="QUEST" quality="COMMON" price="1" desc="1107511" activate_target="STANDALONE" activate_count="1000">
		<uselimits usedelay="2000" usedelayid="41"/>
		<inventory id="2"/>
	</item_template>
	<!-- :880711 -->
	<item_template id="182205668" name="Rusted Spear" level="1" cName="quest_2718a" mask="20545" item_group="QUEST" quality="COMMON" price="1" desc="1107835" activate_target="STANDALONE" activate_count="1000">
		<actions>
			<queststart questid="2718"/>
		</actions>
		<uselimits usedelay="2000" usedelayid="41"/>
		<inventory id="2"/>
	</item_template>
	<!-- :874550 -->
	<item_template id="182005205" name="Rusted Medal" level="40" cName="junk_bronze_medal_01" mask="12414" max_stack_count="1000" quality="JUNK" price="500" desc="741640"/>
	<!-- :874551 -->
	<item_template id="182005206" name="Rusted Medal" level="40" cName="junk_gathering_test_01" mask="12414" max_stack_count="1000" quality="JUNK" price="500" desc="741640"/>
	<!-- :896428 -->
	<item_template id="186000096" name="Platinum Medal" level="60" cName="medal_03" mask="12364" max_stack_count="1000" item_group="MEDALS" quality="RARE" price="15000" desc="752849">
		<inventory id="1"/>
	</item_template>
	<!-- :896559 -->
	<item_template id="186000147" name="Mithril Medal" level="60" cName="medal_04" mask="12364" max_stack_count="1000" item_group="MEDALS" quality="RARE" price="15000" desc="792098">
		<inventory id="1"/>
	</item_template>
	<!-- :896799 -->
	<item_template id="186000242" name="Ceramium Medal" level="65" cName="medal_05" mask="12364" max_stack_count="10000" item_group="MEDALS" quality="RARE" price="25000" desc="812760">
		<inventory id="1"/>
	</item_template>
	<!-- :896802 -->
	<item_template id="186000243" name="Fragmented Ceramium" level="65" cName="coin_piece_01" mask="12360" max_stack_count="10000" item_group="COINS" quality="RARE" price="100" desc="812761">
		<inventory id="1"/>
	</item_template>
)xml";

constexpr std::string_view CRAFT_ITEM_TEMPLATES_XML = R"xml(
	<!-- :744999 -->
	<item_template id="152020112" name="Lesser Elemental Water" level="11" cName="al_part_stone1_02a" mask="12414" max_stack_count="1000" quality="COMMON" price="10" race="ELYOS" desc="707534"/>
	<!-- :744998 -->
	<item_template id="152020111" name="Lesser Elemental Stone Powder" level="10" cName="al_part_stone1_01a" mask="12414" max_stack_count="1000" quality="COMMON" price="20" race="ELYOS" desc="707533"/>
	<!-- :850115 -->
	<item_template id="169400086" name="Glass Bottle" level="10" cName="shopmaterial_al_09a" mask="12414" max_stack_count="1000" quality="COMMON" price="50" desc="703764"/>
	<!-- :850103 -->
	<item_template id="169400074" name="Lesser Catalyst" level="10" cName="shopmaterial_al_01a" mask="12414" max_stack_count="1000" quality="COMMON" price="400" desc="703752"/>
	<!-- :760777 -->
	<item_template id="152202476" name="Design: Lesser Elemental Stone Powder Bundle" level="10" cName="rec_l_al_al_part_mass_stone1_01a" mask="12414" max_stack_count="20" item_group="RECIPE" quality="COMMON" price="1200" race="ELYOS" desc="767215" activate_target="STANDALONE" activate_count="1">
		<actions>
			<craftlearn recipeid="155002476"/>
		</actions>
		<uselimits usedelayid="52"/>
	</item_template>
	<!-- :760783 -->
	<item_template id="152202477" name="Design: Lesser Elemental Water Bundle" level="11" cName="rec_l_al_al_part_mass_stone1_02a" mask="12414" max_stack_count="20" item_group="RECIPE" quality="COMMON" price="1200" race="ELYOS" desc="767216" activate_target="STANDALONE" activate_count="1">
		<actions>
			<craftlearn recipeid="155002477"/>
		</actions>
		<uselimits usedelayid="52"/>
	</item_template>
	<!-- :786623 -->
	<item_template id="152207478" name="Design: Lesser Elemental Stone Powder Bundle" level="10" cName="rec_d_al_al_part_mass_d_stone1_01a" mask="12414" max_stack_count="20" item_group="RECIPE" quality="COMMON" price="1200" race="ASMODIANS" desc="768850" activate_target="STANDALONE" activate_count="1">
		<actions>
			<craftlearn recipeid="155007478"/>
		</actions>
		<uselimits usedelayid="52"/>
	</item_template>
	<!-- :786629 -->
	<item_template id="152207479" name="Design: Lesser Elemental Water Bundle" level="11" cName="rec_d_al_al_part_mass_d_stone1_02a" mask="12414" max_stack_count="20" item_group="RECIPE" quality="COMMON" price="1200" race="ASMODIANS" desc="768851" activate_target="STANDALONE" activate_count="1">
		<actions>
			<craftlearn recipeid="155007479"/>
		</actions>
		<uselimits usedelayid="52"/>
	</item_template>
	<!-- :753271 -->
	<item_template id="152201261" name="Design: Lesser Elemental Stone Powder" level="10" cName="rec_l_al_al_part_stone1_01a" mask="12414" max_stack_count="20" item_group="RECIPE" quality="COMMON" price="100" race="ELYOS" desc="733012" activate_target="STANDALONE" activate_count="1">
		<actions>
			<craftlearn recipeid="155001261"/>
		</actions>
		<uselimits usedelayid="52"/>
	</item_template>
	<!-- :753277 -->
	<item_template id="152201262" name="Design: Lesser Elemental Water" level="11" cName="rec_l_al_al_part_stone1_02a" mask="12414" max_stack_count="20" item_group="RECIPE" quality="COMMON" price="100" race="ELYOS" desc="733013" activate_target="STANDALONE" activate_count="1">
		<actions>
			<craftlearn recipeid="155001262"/>
		</actions>
		<uselimits usedelayid="52"/>
	</item_template>
	<!-- :779141 -->
	<item_template id="152206267" name="Design: Lesser Elemental Stone Powder" level="10" cName="rec_d_al_al_part_d_stone1_01a" mask="12414" max_stack_count="20" item_group="RECIPE" quality="COMMON" price="100" race="ASMODIANS" desc="737460" activate_target="STANDALONE" activate_count="1">
		<actions>
			<craftlearn recipeid="155006267"/>
		</actions>
		<uselimits usedelayid="52"/>
	</item_template>
	<!-- :779147 -->
	<item_template id="152206268" name="Design: Lesser Elemental Water" level="11" cName="rec_d_al_al_part_d_stone1_02a" mask="12414" max_stack_count="20" item_group="RECIPE" quality="COMMON" price="100" race="ASMODIANS" desc="737461" activate_target="STANDALONE" activate_count="1">
		<actions>
			<craftlearn recipeid="155006268"/>
		</actions>
		<uselimits usedelayid="52"/>
	</item_template>
)xml";

/** items/item_templates.xml, verbatim rows: the items of the food, medicine, manastone and event group rows below */
constexpr std::string_view OTHER_BONUS_ITEM_TEMPLATES_XML = R"xml(
	<!-- :821940 -->
	<item_template id="160001001" name="Roast Inina" level="10" cName="food_phyattack_20a" mask="12414" max_stack_count="1000" quality="COMMON" price="300" desc="729414" activate_target="STANDALONE" activate_count="1">
		<actions>
			<skilluse level="2" skillid="10054"/>
		</actions>
		<uselimits usedelay="5000" usedelayid="22"/>
	</item_template>
	<!-- :821946 -->
	<item_template id="160001002" name="Savory Liguri" level="10" cName="food_msboost_20a" mask="12414" max_stack_count="1000" quality="COMMON" price="300" desc="729415" activate_target="STANDALONE" activate_count="1">
		<actions>
			<skilluse level="2" skillid="10058"/>
		</actions>
		<uselimits usedelay="5000" usedelayid="22"/>
	</item_template>
	<!-- :822000 -->
	<item_template id="160001011" name="Roast Kurin with Butter" level="20" cName="food_phyattack_30a" mask="12414" max_stack_count="1000" quality="COMMON" price="600" desc="729424" activate_target="STANDALONE" activate_count="1">
		<actions>
			<skilluse level="3" skillid="10054"/>
		</actions>
		<uselimits usedelay="5000" usedelayid="22"/>
	</item_template>
	<!-- :824316 -->
	<item_template id="160002001" name="Roast Conide" level="10" cName="food_d_phyattack_20a" mask="12414" max_stack_count="1000" quality="COMMON" price="300" desc="729605" activate_target="STANDALONE" activate_count="1">
		<actions>
			<skilluse level="2" skillid="10054"/>
		</actions>
		<uselimits usedelay="5000" usedelayid="22"/>
	</item_template>
	<!-- :830730 -->
	<item_template id="162000003" name="Lesser Life Potion" level="20" cName="remedy_hp_20a" mask="12414" max_stack_count="1000" quality="COMMON" price="450" desc="702585" activate_target="STANDALONE" activate_count="1">
		<actions>
			<skilluse level="1" skillid="9890"/>
		</actions>
		<uselimits usedelay="30000" usedelayid="11"/>
	</item_template>
	<!-- :830736 -->
	<item_template id="162000004" name="Life Potion" level="30" cName="remedy_hp_30a" mask="12414" max_stack_count="1000" quality="COMMON" price="850" desc="702587" activate_target="STANDALONE" activate_count="1">
		<actions>
			<skilluse level="1" skillid="9891"/>
		</actions>
		<uselimits usedelay="30000" usedelayid="11"/>
	</item_template>
	<!-- :830760 -->
	<item_template id="162000008" name="Lesser Mana Potion" level="20" cName="remedy_mp_20a" mask="12414" max_stack_count="1000" quality="COMMON" price="450" desc="702595" activate_target="STANDALONE" activate_count="1">
		<actions>
			<skilluse level="1" skillid="9895"/>
		</actions>
		<uselimits usedelay="30000" usedelayid="11"/>
	</item_template>
	<!-- :830790 -->
	<item_template id="162000013" name="Lesser Life Serum" level="20" cName="potion_hp_20a" mask="12414" max_stack_count="1000" quality="RARE" price="600" desc="702603" activate_target="STANDALONE" activate_count="1">
		<actions>
			<skilluse level="1" skillid="9900"/>
		</actions>
		<uselimits usedelay="30000" usedelayid="11"/>
	</item_template>
	<!-- :830820 -->
	<item_template id="162000018" name="Lesser Mana Serum" level="20" cName="potion_mp_20a" mask="12414" max_stack_count="1000" quality="RARE" price="600" desc="702608" activate_target="STANDALONE" activate_count="1">
		<actions>
			<skilluse level="1" skillid="9905"/>
		</actions>
		<uselimits usedelay="30000" usedelayid="11"/>
	</item_template>
	<!-- :830856 -->
	<item_template id="162000024" name="Lesser Wind Serum" level="10" cName="potion_flytime_20a" mask="12414" max_stack_count="1000" quality="RARE" price="600" desc="702616" activate_target="STANDALONE" activate_count="1">
		<actions>
			<skilluse level="1" skillid="9911"/>
		</actions>
		<uselimits usedelay="24000" usedelayid="13" ride_usable="true"/>
	</item_template>
	<!-- :830862 -->
	<item_template id="162000025" name="Wind Serum" level="20" cName="potion_flytime_30A" mask="12414" max_stack_count="1000" quality="RARE" price="1200" desc="702617" activate_target="STANDALONE" activate_count="1">
		<actions>
			<skilluse level="2" skillid="9911"/>
		</actions>
		<uselimits usedelay="42000" usedelayid="13" ride_usable="true"/>
	</item_template>
	<!-- :830973 -->
	<item_template id="162000042" name="Lesser Recovery Potion" level="20" cName="remedy_hp_mp_20a" mask="12414" max_stack_count="1000" quality="RARE" price="450" desc="729395" activate_target="STANDALONE" activate_count="1">
		<actions>
			<skilluse level="1" skillid="10182"/>
		</actions>
		<uselimits usedelay="30000" usedelayid="11"/>
	</item_template>
	<!-- :831003 -->
	<item_template id="162000047" name="Lesser Recovery Serum" level="20" cName="potion_hp_mp_20a" mask="12414" max_stack_count="1000" quality="LEGEND" price="600" desc="729400" activate_target="STANDALONE" activate_count="1">
		<actions>
			<skilluse level="1" skillid="10187"/>
		</actions>
		<uselimits usedelay="30000" usedelayid="11"/>
	</item_template>
	<!-- :831009 -->
	<item_template id="162000048" name="Recovery Serum" level="30" cName="potion_hp_mp_30a" mask="12414" max_stack_count="1000" quality="LEGEND" price="1200" desc="729401" activate_target="STANDALONE" activate_count="1">
		<actions>
			<skilluse level="1" skillid="10188"/>
		</actions>
		<uselimits usedelay="30000" usedelayid="11"/>
	</item_template>
	<!-- :839346 -->
	<item_template id="167000235" name="Manastone: Crit Strike +4" level="10" cName="matter_option_c_physicalcritical_10" mask="12414" max_stack_count="10000" item_group="MANASTONE" quality="COMMON" price="10" desc="719046" activate_count="1">
		<modifiers>
			<add name="PHYSICAL_CRITICAL" value="4" bonus="true"/>
		</modifiers>
		<actions>
			<enchant count="1"/>
		</actions>
	</item_template>
	<!-- :839354 -->
	<item_template id="167000258" name="Manastone: HP +30" level="20" cName="matter_option_c_hp_20" mask="12414" max_stack_count="10000" item_group="MANASTONE" quality="COMMON" price="10" desc="719069" activate_count="1">
		<modifiers>
			<add name="MAXHP" value="30" bonus="true"/>
		</modifiers>
		<actions>
			<enchant count="1"/>
		</actions>
	</item_template>
	<!-- :839362 -->
	<item_template id="167000259" name="Manastone: MP +30" level="20" cName="matter_option_c_mp_20" mask="12414" max_stack_count="10000" item_group="MANASTONE" quality="COMMON" price="10" desc="719070" activate_count="1">
		<modifiers>
			<add name="MAXMP" value="30" bonus="true"/>
		</modifiers>
		<actions>
			<enchant count="1"/>
		</actions>
	</item_template>
	<!-- :839418 -->
	<item_template id="167000290" name="Manastone: HP +40" level="30" cName="matter_option_c_hp_30" mask="12414" max_stack_count="10000" item_group="MANASTONE" quality="COMMON" price="10" desc="719101" activate_count="1">
		<modifiers>
			<add name="MAXHP" value="40" bonus="true"/>
		</modifiers>
		<actions>
			<enchant count="1"/>
		</actions>
	</item_template>
	<!-- :839626 -->
	<item_template id="167000418" name="Manastone: HP +55" level="20" cName="matter_option_r_hp_20" mask="12414" max_stack_count="10000" item_group="MANASTONE" quality="RARE" price="100" desc="719229" activate_count="1">
		<modifiers>
			<add name="MAXHP" value="55" bonus="true"/>
		</modifiers>
		<actions>
			<enchant count="5"/>
		</actions>
	</item_template>
	<!-- :839634 -->
	<item_template id="167000419" name="Manastone: MP +55" level="20" cName="matter_option_r_mp_20" mask="12414" max_stack_count="10000" item_group="MANASTONE" quality="RARE" price="100" desc="719230" activate_count="1">
		<modifiers>
			<add name="MAXMP" value="55" bonus="true"/>
		</modifiers>
		<actions>
			<enchant count="5"/>
		</actions>
	</item_template>
	<!-- :839692 -->
	<item_template id="167000450" name="Manastone: HP +65" level="30" cName="matter_option_r_hp_30" mask="12414" max_stack_count="10000" item_group="MANASTONE" quality="RARE" price="100" desc="719261" activate_count="1">
		<modifiers>
			<add name="MAXHP" value="65" bonus="true"/>
		</modifiers>
		<actions>
			<enchant count="5"/>
		</actions>
	</item_template>
	<!-- :876608 -->
	<item_template id="182007170" name="Symphony of Legend Copy" level="1" cName="world_event_movement_fake" mask="28736" max_stack_count="10000" quality="RARE" price="5" desc="842582">
		<inventory id="2"/>
	</item_template>
	<!-- :927729 -->
	<item_template id="188100253" name="Symphony of Legend - The Second Movement" level="1" cName="assembly_world_event_movement_02" casting_delay="1000" mask="28794" max_stack_count="1000" quality="COMMON" price="5" desc="842584" activate_target="STANDALONE" activate_count="1">
		<actions>
			<assemble item="182007170"/>
		</actions>
		<uselimits usedelay="1000" usedelayid="80"/>
	</item_template>
	<!-- :927735 -->
	<item_template id="188100254" name="Symphony of Legend - The Third Movement" level="1" cName="assembly_world_event_movement_03" casting_delay="1000" mask="28794" max_stack_count="1000" quality="COMMON" price="5" desc="842585" activate_target="STANDALONE" activate_count="1">
		<actions>
			<assemble item="182007170"/>
		</actions>
		<uselimits usedelay="1000" usedelayid="80"/>
	</item_template>
	<!-- :927741 -->
	<item_template id="188100255" name="Symphony of Legend - The Fourth Movement" level="1" cName="assembly_world_event_movement_04" casting_delay="1000" mask="28794" max_stack_count="1000" quality="COMMON" price="5" desc="842586" activate_target="STANDALONE" activate_count="1">
		<actions>
			<assemble item="182007170"/>
		</actions>
		<uselimits usedelay="1000" usedelayid="80"/>
	</item_template>
	<!-- :927747 -->
	<item_template id="188100256" name="Symphony of Legend - Epilogue" level="1" cName="assembly_world_event_movement_05" casting_delay="1000" mask="28794" max_stack_count="1000" quality="COMMON" price="5" desc="842587" activate_target="STANDALONE" activate_count="1">
		<actions>
			<assemble item="182007170"/>
		</actions>
		<uselimits usedelay="1000" usedelayid="80"/>
	</item_template>
)xml";

/** items/item_groups.xml, verbatim rows (see the file comment for the cut of the groups) */
constexpr std::string_view ITEM_GROUPS_XML = R"xml(<item_groups>
	<!-- :3-5 -->
	<craft_materials bonusType="TASK" chance="47">
		<item id="152020112" skill="40007" minLevel="5" maxLevel="40"/>
		<item id="152020111" skill="40007" minLevel="10" maxLevel="60"/>
	</craft_materials>
	<!-- :267-269 -->
	<craft_shop bonusType="TASK" chance="47">
		<item id="169400086" skill="40007" minLevel="5" maxLevel="460"/>
		<item id="169400074" skill="40007" minLevel="10" maxLevel="90"/>
	</craft_shop>
	<!-- :422-424, :553-554 -->
	<craft_bundles bonusType="TASK" chance="2">
		<item id="152202476" skill="40007" level="1"/>
		<item id="152202477" skill="40007" level="1"/>
		<item id="152207478" skill="40007" level="1"/>
		<item id="152207479" skill="40007" level="1"/>
	</craft_bundles>
	<!-- :684-686, :2516-2517 -->
	<craft_recipes bonusType="TASK" chance="4">
		<item id="152201261" skill="40007" level="1"/>
		<item id="152201262" skill="40007" level="1"/>
		<item id="152206267" skill="40007" level="1"/>
		<item id="152206268" skill="40007" level="1"/>
	</craft_recipes>
	<!-- :4346, :4355-4357, :4364, :4416 -->
	<manastones_common bonusType="MANASTONE" chance="95">
		<item id="167000235"/>
		<item id="167000258"/>
		<item id="167000259"/>
		<item id="167000290"/>
	</manastones_common>
	<!-- :4417-4419, :4426, :4507 -->
	<manastones_rare bonusType="MANASTONE" chance="5">
		<item id="167000418"/>
		<item id="167000419"/>
		<item id="167000450"/>
	</manastones_rare>
	<!-- :4508-4539 -->
	<medals bonusType="MEDAL" chance="100">
		<item id="186000031" level="1" count="2" chance="22"/>
		<item id="186000030" level="1" count="1" chance="19"/>
		<item id="182202156" level="1" count="1" chance="2" race="ELYOS"/>
		<item id="182205668" level="1" count="1" chance="2" race="ASMODIANS"/>
		<item id="182005205" level="1" count="1" chance="57"/>
		<item id="182005206" level="2" count="1" chance="67"/>
		<item id="186000096" level="2" count="1" chance="13.5"/>
		<item id="186000096" level="2" count="2" chance="6.5"/>
		<item id="186000147" level="2" count="1" chance="10"/>
		<item id="186000147" level="2" count="2" chance="3"/>
		<item id="182005206" level="3" count="1" chance="67"/>
		<item id="186000096" level="3" count="1" chance="8.5"/>
		<item id="186000096" level="3" count="2" chance="1.5"/>
		<item id="186000147" level="3" count="1" chance="13"/>
		<item id="186000147" level="3" count="2" chance="9"/>
		<item id="186000147" level="3" count="3" chance="0.9"/>
		<item id="186000147" level="3" count="4" chance="0.1"/>
		<item id="182005206" level="4" count="1" chance="67"/>
		<item id="186000147" level="4" count="1" chance="22"/>
		<item id="186000147" level="4" count="2" chance="10"/>
		<item id="186000147" level="4" count="3" chance="0.9"/>
		<item id="186000147" level="4" count="4" chance="0.1"/>
		<item id="182005206" level="5" count="1" chance="67"/>
		<item id="186000242" level="5" count="1" chance="12.3"/>
		<item id="186000242" level="5" count="2" chance="1.7"/>
		<item id="186000242" level="5" count="3" chance="0.5"/>
		<item id="186000242" level="5" count="4" chance="0.1"/>
		<item id="186000243" level="5" count="1" chance="18.4"/>
		<item id="182005206" level="6" count="1" chance="67"/>
		<item id="186000242" level="6" count="1" chance="33"/>
	</medals>
	<!-- :4540, :4549-4550, :4559, :4599, :4658 -->
	<food bonusType="FOOD" chance="100">
		<item id="160001001" race="ELYOS" level="20"/>
		<item id="160001002" race="ELYOS" level="20"/>
		<item id="160001011" race="ELYOS" level="30"/>
		<item id="160002001" race="ASMODIANS" level="20"/>
	</food>
	<!-- :4659, :4661-4662, :4666, :4676 -->
	<medicine_common bonusType="MEDICINE" chance="60">
		<item id="162000003" level="20"/>
		<item id="162000004" level="30"/>
		<item id="162000008" level="20"/>
	</medicine_common>
	<!-- :4677, :4679, :4684, :4688-4689, :4693, :4705 -->
	<medicine_rare bonusType="MEDICINE" chance="20">
		<item id="162000013" level="20"/>
		<item id="162000018" level="20"/>
		<item id="162000024" level="20"/>
		<item id="162000025" level="30"/>
		<item id="162000042" level="20"/>
	</medicine_rare>
	<!-- :4706, :4708-4709, :4715 -->
	<medicine_legendary bonusType="MEDICINE" chance="20">
		<item id="162000047" level="20"/>
		<item id="162000048" level="30"/>
	</medicine_legendary>
	<!-- :4969, :4971-4976, :4978, :5025 -->
	<events bonusType="EVENTS">
		<item id="188100253" level="1" count="1" chance="16.66"/>
		<item id="188100254" level="1" count="1" chance="16.66"/>
		<item id="188100255" level="1" count="1" chance="16.66"/>
		<item id="188100256" level="1" count="1" chance="16.66"/>
		<item id="182007170" level="1" count="1" chance="16.66"/>
		<item id="182007170" level="1" count="5" chance="16.7"/>
		<item id="188100254" level="2" count="1" chance="20"/>
	</events>
	<!-- :5257-6124, the pet food groups with their rows cut (ItemGroupsData.afterUnmarshal reads every one of them) -->
	<feed_fluid group="JUNK">
	</feed_fluid>
	<feed_armor group="JUNK">
	</feed_armor>
	<feed_thorn group="JUNK">
	</feed_thorn>
	<feed_bone group="JUNK">
	</feed_bone>
	<feed_balaur_material group="JUNK">
	</feed_balaur_material>
	<feed_soul group="JUNK">
	</feed_soul>
	<feed_exclude group="JUNK">
	</feed_exclude>
	<stinking_junk group="JUNK">
	</stinking_junk>
	<feed_healthy_all group="NONE">
	</feed_healthy_all>
	<feed_healthy_spicy group="NONE">
	</feed_healthy_spicy>
	<feed_powder_biscuit group="NONE">
	</feed_powder_biscuit>
	<feed_crystal_biscuit group="NONE">
	</feed_crystal_biscuit>
	<feed_gem_biscuit group="NONE">
	</feed_gem_biscuit>
	<poppy_snack group="NONE">
	</poppy_snack>
	<tasty_poppy_snack group="NONE">
	</tasty_poppy_snack>
	<nutritious_poppy_snack group="NONE">
	</nutritious_poppy_snack>
	<feed_shugo_event_coin group="NONE">
	</feed_shugo_event_coin>
	<feed_aether_cherry group="NONE">
	</feed_aether_cherry>
</item_groups>)xml";

/** The seeds each random case draws over (the outcomes are fixed by them; the assertions are the sets, not the order) */
constexpr uint64_t SEEDS = 400;

using Outcome = std::pair<int32_t, int64_t>; // item id, count

class BonusServiceTest : public EconomyTest {
protected:
	void SetUp() override {
		EconomyTest::SetUp();
		// one load: the group entries find their item templates through the XmlIDs the item templates registered (ItemRaceEntry::afterUnmarshal)
		xml::LoadContext context;
		std::string items = "<item_templates>";
		items.append(MEDAL_ITEM_TEMPLATES_XML).append(CRAFT_ITEM_TEMPLATES_XML).append(OTHER_BONUS_ITEM_TEMPLATES_XML).append("</item_templates>");
		dataholders::DataManager::ITEM_DATA.publish(xml::bindString<dataholders::ItemData>(context, items));
		dataholders::DataManager::ITEM_GROUPS_DATA.publish(xml::bindString<dataholders::ItemGroupsData>(context, ITEM_GROUPS_XML));
		dataholders::DataManager::QUEST_DATA.publish(xml::bindString<dataholders::QuestsData>(context, QUEST_DATA_XML));
	}

	void TearDown() override {
		dataholders::DataManager::QUEST_DATA.resetForTests();
		dataholders::DataManager::ITEM_GROUPS_DATA.resetForTests();
		dataholders::DataManager::ITEM_DATA.resetForTests();
		Rnd::generator() = savedGenerator;
		EconomyTest::TearDown();
	}

	static const model::templates::QuestTemplate* quest(int32_t questId) {
		const model::templates::QuestTemplate* questTemplate = dataholders::DataManager::QUEST_DATA->getQuestById(questId);
		EXPECT_NE(questTemplate, nullptr) << questId;
		return questTemplate;
	}

	/** Every (item id, count) getQuestBonus answers over the seeds; `empty` counts the answers without an item */
	static std::set<Outcome> bonusOutcomes(model::gameobjects::player::Player& player, int32_t questId, int& empty) {
		std::set<Outcome> outcomes;
		empty = 0;
		for (uint64_t seed = 1; seed <= SEEDS; seed++) {
			Rnd::seedCurrentThreadForTests(seed);
			std::optional<QuestItems> bonus = BonusService::getQuestBonus(player, quest(questId));
			if (bonus)
				outcomes.insert({bonus->getItemId(), bonus->getCount()});
			else
				empty++;
		}
		return outcomes;
	}

	static std::vector<int32_t> idsOf(const std::vector<const ItemRaceEntry*>& entries) {
		std::vector<int32_t> ids;
		for (const ItemRaceEntry* entry : entries)
			ids.push_back(entry->getId());
		return ids;
	}

	/** the thread's random state before the case, restored after it (the seeded draws must not leak into the next case of a process) */
	Rnd::Xoshiro256PlusPlus savedGenerator = Rnd::generator();
};

TEST_F(BonusServiceTest, AQuestWithoutABonusGetsNone) {
	PlayerFixture elyos = makePlayer(100201, 1201, "Elyos", Race::ELYOS);
	network::test::LogCapture capture({BONUS_SERVICE_LOGGER});

	// BonusService.java:28-29
	EXPECT_FALSE(BonusService::getQuestBonus(*elyos.player, quest(SLEEPING_ON_THE_JOB)).has_value());
	EXPECT_EQ(capture.count("Bonus of type"), 0) << capture.dump();
}

TEST_F(BonusServiceTest, AMovieBonusHasNoGroupAndAnUnimplementedTypeIsLogged) {
	PlayerFixture elyos = makePlayer(100201, 1201, "Elyos", Race::ELYOS);
	network::test::LogCapture capture({BONUS_SERVICE_LOGGER});

	// :74-76: MOVIE (and NONE) break out of the switch without a word, and the empty group list chooses nothing (:43, :51-52 null; :32-33)
	EXPECT_TRUE(BonusService::getMatchingItemsOfRandomGroup(*elyos.player, quest(CHASING_THE_GRANKERS)).empty());
	EXPECT_FALSE(BonusService::getQuestBonus(*elyos.player, quest(CHASING_THE_GRANKERS)).has_value());
	EXPECT_EQ(capture.count("Bonus of type"), 0) << capture.dump();

	// :56-61 are commented out, so BOSS (and GATHER, ENCHANT and the other types the data names) takes the default arm (:77-78)
	EXPECT_TRUE(BonusService::getMatchingItemsOfRandomGroup(*elyos.player, quest(DEFEAT_LOWER_GENERALS)).empty());
	EXPECT_FALSE(BonusService::getQuestBonus(*elyos.player, quest(DEFEAT_LOWER_GENERALS)).has_value());
	EXPECT_EQ(capture.count("Bonus of type BOSS is not implemented"), 2) << capture.dump();
}

TEST_F(BonusServiceTest, AMedalBonusIsARowOfTheMedalGroupAtTheBonusLevelForThePlayersRace) {
	PlayerFixture elyos = makePlayer(100201, 1201, "Elyos", Race::ELYOS);
	PlayerFixture asmodian = makePlayer(100202, 1202, "Asmodian", Race::ASMODIANS);

	// the one medal group (chance 100) is always the one chosen; its level-1 rows in data order, without the other race's row
	EXPECT_EQ(idsOf(BonusService::getMatchingItemsOfRandomGroup(*elyos.player, quest(SILVER_FOR_THE_FOUNTAIN_ELYOS))),
		(std::vector<int32_t>{186000031, 186000030, 182202156, 182005205}));
	EXPECT_EQ(idsOf(BonusService::getMatchingItemsOfRandomGroup(*asmodian.player, quest(SILVER_FOR_THE_FOUNTAIN_ASMODIANS))),
		(std::vector<int32_t>{186000031, 186000030, 182205668, 182005205}));

	// getQuestBonus: one of them by its chance (22, 19, 2, 57), with the row's count (FullRewardItem.getCount)
	int empty = -1;
	EXPECT_EQ(bonusOutcomes(*elyos.player, SILVER_FOR_THE_FOUNTAIN_ELYOS, empty),
		(std::set<Outcome>{{186000031, 2}, {186000030, 1}, {182202156, 1}, {182005205, 1}}));
	EXPECT_EQ(empty, 0);
	EXPECT_EQ(bonusOutcomes(*asmodian.player, SILVER_FOR_THE_FOUNTAIN_ASMODIANS, empty),
		(std::set<Outcome>{{186000031, 2}, {186000030, 1}, {182205668, 1}, {182005205, 1}}));
	EXPECT_EQ(empty, 0);

	// level 2 of a quest for both races: the five level-2 rows, two of them the same item with another count
	EXPECT_EQ(bonusOutcomes(*elyos.player, CYGNEA_FOUNTAIN_OF_LUCK, empty),
		(std::set<Outcome>{{182005206, 1}, {186000096, 1}, {186000096, 2}, {186000147, 1}, {186000147, 2}}));
	EXPECT_EQ(empty, 0);
}

TEST_F(BonusServiceTest, AWorkOrderBonusFallsThroughTheCraftGroupsThatHaveNoMatch) {
	PlayerFixture elyos = makePlayer(100201, 1201, "Elyos", Race::ELYOS);
	PlayerFixture asmodian = makePlayer(100202, 1202, "Asmodian", Race::ASMODIANS);

	// TASK names the four craft groups (:72-73). For skill 40007 at skill point 1 craft_materials (47) and craft_shop (47) have no row
	// (CraftItem.matchesQuest: minLevel 5 and 10), so a draw that picks one of them removes it (Chance.selectElement(remainingGroups, true)) and
	// draws again (:43-50) until craft_bundles (2) or craft_recipes (4) answers with the two rows of the player's race (the item's race,
	// ItemRaceEntry.matchesRace); CraftRecipe.getCount is ItemRaceEntry's 1
	for (uint64_t seed = 1; seed <= SEEDS; seed++) {
		Rnd::seedCurrentThreadForTests(seed);
		std::vector<int32_t> ids = idsOf(BonusService::getMatchingItemsOfRandomGroup(*elyos.player, quest(LAMPLIGHT_SUPPLIES_ELYOS)));
		ASSERT_TRUE(ids == (std::vector<int32_t>{152202476, 152202477}) || ids == (std::vector<int32_t>{152201261, 152201262}))
			<< "seed " << seed << ": " << ::testing::PrintToString(ids);
	}

	int empty = -1;
	EXPECT_EQ(bonusOutcomes(*elyos.player, LAMPLIGHT_SUPPLIES_ELYOS, empty),
		(std::set<Outcome>{{152202476, 1}, {152202477, 1}, {152201261, 1}, {152201262, 1}}));
	EXPECT_EQ(empty, 0) << "every draw ends in a group with rows";
	EXPECT_EQ(bonusOutcomes(*asmodian.player, LAMPLIGHT_SUPPLIES_ASMODIANS, empty),
		(std::set<Outcome>{{152207478, 1}, {152207479, 1}, {152206267, 1}, {152206268, 1}}));
	EXPECT_EQ(empty, 0);
}

TEST_F(BonusServiceTest, TheItemsFollowThePlayersRaceNotTheQuests) {
	// ItemRaceEntry.matches filters by the player's race (and the item's), never by the quest's race_permitted: an Elyos who holds the Asmodian
	// work order 6400 (as a GM could give it) gets the Elyos rows of the same skill and skill point
	PlayerFixture elyos = makePlayer(100201, 1201, "Elyos", Race::ELYOS);
	int empty = -1;
	EXPECT_EQ(bonusOutcomes(*elyos.player, LAMPLIGHT_SUPPLIES_ASMODIANS, empty),
		(std::set<Outcome>{{152202476, 1}, {152202477, 1}, {152201261, 1}, {152201262, 1}}));
	EXPECT_EQ(empty, 0);
}

TEST_F(BonusServiceTest, WhenNoGroupHasARowTheLastEmptyListGivesNoBonus) {
	// A property of this file's cut, not of the shipped groups (where cooking, skill 40001, has rows at skill point 1 in craft_shop and
	// craft_recipes): the cut groups hold rows of skill 40007 only, so for 5500 (cooking) every group is drawn, filtered empty and removed
	// (:43-50), the loop ends with the groups, and the last, empty list gives no item (:32-36; Java's Chance.selectElement over no element)
	PlayerFixture elyos = makePlayer(100201, 1201, "Elyos", Race::ELYOS);
	int empty = -1;
	EXPECT_TRUE(bonusOutcomes(*elyos.player, GRITTY_ININA_SUPPLIES, empty).empty());
	EXPECT_EQ(empty, static_cast<int>(SEEDS));
	EXPECT_TRUE(BonusService::getMatchingItemsOfRandomGroup(*elyos.player, quest(GRITTY_ININA_SUPPLIES)).empty());
}

TEST_F(BonusServiceTest, AManastoneBonusIsTheLevelRowsOfTheCommonOrTheRareManastoneGroup) {
	PlayerFixture elyos = makePlayer(100201, 1201, "Elyos", Race::ELYOS);
	network::test::LogCapture capture({BONUS_SERVICE_LOGGER});
	const std::vector<int32_t> common = {167000258, 167000259};
	const std::vector<int32_t> rare = {167000418, 167000419};

	// MANASTONE names manastones_common (95) and manastones_rare (5) (:66-67, ItemGroupsData.afterUnmarshal). Their rows are plain
	// ItemRaceEntry: ItemRaceEntry.matchesLevel compares the item template's level with the bonus level 20, so the rows of item level 10 and 30
	// never match. Both groups have rows of level 20, so the first group drawn answers with its two
	int commonDraws = 0;
	int rareDraws = 0;
	for (uint64_t seed = 1; seed <= SEEDS; seed++) {
		Rnd::seedCurrentThreadForTests(seed);
		std::vector<int32_t> ids = idsOf(BonusService::getMatchingItemsOfRandomGroup(*elyos.player, quest(NEW_MANASTONES_FOR_OLD)));
		ASSERT_TRUE(ids == common || ids == rare) << "seed " << seed << ": " << ::testing::PrintToString(ids);
		(ids == common ? commonDraws : rareDraws)++;
	}
	EXPECT_GT(commonDraws, rareDraws) << "95 against 5";
	EXPECT_GT(rareDraws, 0);

	// getQuestBonus: one of the four, ItemRaceEntry.getCount's 1
	int empty = -1;
	EXPECT_EQ(bonusOutcomes(*elyos.player, NEW_MANASTONES_FOR_OLD, empty),
		(std::set<Outcome>{{167000258, 1}, {167000259, 1}, {167000418, 1}, {167000419, 1}}));
	EXPECT_EQ(empty, 0);
	EXPECT_EQ(capture.count("Bonus of type"), 0) << capture.dump();
}

TEST_F(BonusServiceTest, AMedicineBonusIsTheLevelRowsOfOneOfTheThreeMedicineGroups) {
	PlayerFixture elyos = makePlayer(100201, 1201, "Elyos", Race::ELYOS);
	network::test::LogCapture capture({BONUS_SERVICE_LOGGER});
	const std::vector<int32_t> common = {162000003, 162000008};
	const std::vector<int32_t> rare = {162000013, 162000018, 162000024, 162000042};
	const std::vector<int32_t> legendary = {162000047};

	// MEDICINE names medicine_common (60), medicine_rare (20) and medicine_legendary (20) (:68-69). Their rows are MedicineItem, an
	// IdLevelReward: the row's level is compared, not the template's (IdLevelReward.matchesLevel), so 162000024 (template level 10, row 20)
	// matches and 162000025 (template level 20, row 30) does not
	std::set<std::vector<int32_t>> drawn;
	for (uint64_t seed = 1; seed <= SEEDS; seed++) {
		Rnd::seedCurrentThreadForTests(seed);
		std::vector<int32_t> ids = idsOf(BonusService::getMatchingItemsOfRandomGroup(*elyos.player, quest(KERUBIAN_HORNS)));
		ASSERT_TRUE(ids == common || ids == rare || ids == legendary) << "seed " << seed << ": " << ::testing::PrintToString(ids);
		drawn.insert(ids);
	}
	EXPECT_EQ(drawn, (std::set<std::vector<int32_t>>{common, rare, legendary}));

	// getQuestBonus: one of the seven, with MedicineItem.getCount's Rnd.get(1, 3)
	int empty = -1;
	std::set<int32_t> ids;
	std::set<int64_t> counts;
	for (const Outcome& outcome : bonusOutcomes(*elyos.player, KERUBIAN_HORNS, empty)) {
		ids.insert(outcome.first);
		counts.insert(outcome.second);
	}
	EXPECT_EQ(ids, (std::set<int32_t>{162000003, 162000008, 162000013, 162000018, 162000024, 162000042, 162000047}));
	EXPECT_EQ(counts, (std::set<int64_t>{1, 2, 3}));
	EXPECT_EQ(empty, 0);
	EXPECT_EQ(capture.count("Bonus of type"), 0) << capture.dump();
}

TEST_F(BonusServiceTest, AFoodBonusIsTheLevelRowsOfThePlayersRace) {
	PlayerFixture elyos = makePlayer(100201, 1201, "Elyos", Race::ELYOS);
	PlayerFixture asmodian = makePlayer(100202, 1202, "Asmodian", Race::ASMODIANS);
	network::test::LogCapture capture({BONUS_SERVICE_LOGGER});

	// FOOD names the one food group (:64-65). Its rows are FoodItem, an IdLevelReward (the row's level: 160001011 of template level 20 is a
	// row of level 30) with the row's race (ItemRaceEntry.matchesRace): the Elyos gets the two Elyos rows of level 20, the Asmodian who holds
	// the Elyos quest (as a GM could give it) the Asmodian one
	EXPECT_EQ(idsOf(BonusService::getMatchingItemsOfRandomGroup(*elyos.player, quest(MORE_MANDURI_FRILLNECKS))),
		(std::vector<int32_t>{160001001, 160001002}));
	EXPECT_EQ(idsOf(BonusService::getMatchingItemsOfRandomGroup(*asmodian.player, quest(MORE_MANDURI_FRILLNECKS))),
		(std::vector<int32_t>{160002001}));

	// getQuestBonus: with FoodItem.getCount's Rnd.nextBoolean() ? 5 : 10
	int empty = -1;
	EXPECT_EQ(bonusOutcomes(*elyos.player, MORE_MANDURI_FRILLNECKS, empty),
		(std::set<Outcome>{{160001001, 5}, {160001001, 10}, {160001002, 5}, {160001002, 10}}));
	EXPECT_EQ(empty, 0);
	EXPECT_EQ(bonusOutcomes(*asmodian.player, MORE_MANDURI_FRILLNECKS, empty), (std::set<Outcome>{{160002001, 5}, {160002001, 10}}));
	EXPECT_EQ(empty, 0);
	EXPECT_EQ(capture.count("Bonus of type"), 0) << capture.dump();
}

TEST_F(BonusServiceTest, AnEventsBonusIsARowOfTheEventGroupAtTheBonusLevel) {
	PlayerFixture elyos = makePlayer(100201, 1201, "Elyos", Race::ELYOS);
	network::test::LogCapture capture({BONUS_SERVICE_LOGGER});

	// EVENTS names the one events group (:62-63, chance 100 by default). Its rows are FullRewardItem, an IdLevelReward with its own count and
	// chance: the six rows of level 1 in data order, one item twice with another count, and not the row of level 2
	EXPECT_EQ(idsOf(BonusService::getMatchingItemsOfRandomGroup(*elyos.player, quest(SYMPHONY_OF_LEGEND_FIRST_MOVEMENT))),
		(std::vector<int32_t>{188100253, 188100254, 188100255, 188100256, 182007170, 182007170}));

	int empty = -1;
	EXPECT_EQ(bonusOutcomes(*elyos.player, SYMPHONY_OF_LEGEND_FIRST_MOVEMENT, empty),
		(std::set<Outcome>{{188100253, 1}, {188100254, 1}, {188100255, 1}, {188100256, 1}, {182007170, 1}, {182007170, 5}}));
	EXPECT_EQ(empty, 0);
	EXPECT_EQ(capture.count("Bonus of type"), 0) << capture.dump();
}

} // namespace
} // namespace aion::gameserver::economy::test
