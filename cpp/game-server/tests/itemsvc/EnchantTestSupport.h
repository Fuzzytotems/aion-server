#pragma once

// Shared fixture of the M5c enhance-lane tests (m5c-plan.md E-05): EnchantService, the ItemSocketService manastone bodies, EnchantItemAction,
// ExtractAction, DecomposeAction and RemodelAction, driven against the real Player and AionConnection of ItemServicesTestSupport.h.
//
// Every item template and decomposable row below is copied verbatim from the shipped data (game-server/data/static_data/items/
// item_templates.xml and decomposable_items/decomposable_items.xml, the line of each row in the comment above it), and so is the SWORD list of
// enchants/enchant_templates.xml the equipped-item cases publish. The configuration values are the shipped profile's (config/main/
// rates.properties, prices.properties, config/administration/admin.properties), set explicitly because the unit tests load no properties.

#include "ItemServicesTestSupport.h"

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "aion/commons/utils/Rnd.h"
#include "aion/gameserver/configs/administration/AdminConfig.h"
#include "aion/gameserver/configs/main/PricesConfig.h"
#include "aion/gameserver/configs/main/RatesConfig.h"
#include "aion/gameserver/dataholders/DecomposableItemsData.bind.h"
#include "aion/gameserver/dataholders/DecomposableItemsData.h"
#include "aion/gameserver/dataholders/EnchantData.bind.h"
#include "aion/gameserver/dataholders/EnchantData.h"
#include "aion/gameserver/dataholders/ItemSetData.h"
#include "aion/gameserver/model/ChatType.h"
#include "aion/gameserver/model/gameobjects/Persistable_PersistentState.h"
#include "aion/gameserver/model/gameobjects/player/Equipment.h"
#include "aion/gameserver/model/skill/PlayerSkillEntry.h"
#include "aion/gameserver/model/skill/PlayerSkillList.h"
#include "aion/gameserver/network/aion/serverpackets/SM_MESSAGE.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::services::item::test {

// the item ids of the rows below (the ids the base fixture names are reused from ItemServicesTestSupport.h)
inline constexpr int32_t CIRCULUS_SWORD = 100000001;
inline constexpr int32_t PLAINSMANS_SWORD = 100000133;
inline constexpr int32_t SET_TEST_SWORD_01 = 100000714;
inline constexpr int32_t ANCIENT_MANASTONE_TEST_SPELLBOOK = 100601101;
inline constexpr int32_t PLAINSMANS_TUNIC = 110100355;
inline constexpr int32_t RATTAN_HAT = 125045164;
inline constexpr int32_t AQUABLUE_SUNHAT = 125045242;
inline constexpr int32_t SPORTS_CAP = 125040180;
inline constexpr int32_t LAWFUL_SHADES = 125045297;
inline constexpr int32_t DAPPER_FEDORA = 125045346;
inline constexpr int32_t PEPENTO = 152000064;
inline constexpr int32_t JUICY_PEPENTO = 152000065;
inline constexpr int32_t ENAMORED_TIGER_FORM_CANDY_ELYOS = 160010109;
inline constexpr int32_t ENAMORED_TIGER_FORM_CANDY_ASMODIANS = 160010110;
inline constexpr int32_t EXTRACTION_TOOLS = 165000001;
inline constexpr int32_t ALPHA_ENCHANTMENT_STONE = 166000191;
inline constexpr int32_t BETA_ENCHANTMENT_STONE = 166000192;
inline constexpr int32_t GAMMA_ENCHANTMENT_STONE = 166000193;
inline constexpr int32_t DELTA_ENCHANTMENT_STONE = 166000194;
inline constexpr int32_t EPSILON_ENCHANTMENT_STONE = 166000195;
inline constexpr int32_t LESSER_SUPPLEMENTS = 166100000;
inline constexpr int32_t MANASTONE_ONLY_SAND = 166150000;
inline constexpr int32_t MANASTONE_SOCKETING_SUPPLEMENTS_31_50 = 166150006;
inline constexpr int32_t AMPLIFICATION_STONE = 166500002;
inline constexpr int32_t MANASTONE_HP_20 = 167000226;
inline constexpr int32_t MANASTONE_HP_60 = 167000354;
inline constexpr int32_t MANASTONE_HP_55 = 167000418;
inline constexpr int32_t ANCIENT_MANASTONE_HP_105 = 167020024;
inline constexpr int32_t SUSPICIOUS_OLD_SACK = 188050584;
inline constexpr int32_t SUSPICIOUS_RED_SACK = 188050586;
inline constexpr int32_t CHOCOLATE_CANDY_BOX = 188050714;
inline constexpr int32_t EVENT_COLD_BOX = 188051090;
inline constexpr int32_t EVENT_MANASTONE_BUNDLE = 188053609;

/** item_templates.xml, verbatim rows (the line of each <item_template> in the comment above it) */
inline constexpr std::string_view ENHANCE_ITEM_TEMPLATES_XML = R"xml(<item_templates>
	<!-- :3 -->
	<item_template id="100000001" name="Circulus' Sword" level="1" cName="sword_circulous" mask="138878" item_group="SWORD" quality="UNIQUE" price="5" desc="700558" attack_type="PHYSICAL" max_enchant="15">
		<weapon_stats hit_count="2" attack_range="1500" parry="173" physical_accuracy="52" critical="50" attack_speed="1400" max_damage="20" min_damage="16"/>
		<idian burn_attack="29" burn_defend="12"/>
	</item_template>
	<!-- :375 -->
	<item_template id="100000094" name="Training Sword" level="1" cName="sword_n_c_01a" mask="138366" item_group="SWORD" quality="COMMON" price="5" desc="700775" attack_type="PHYSICAL" max_enchant="10" m_slots="1">
		<weapon_stats hit_count="2" attack_range="1500" parry="173" physical_accuracy="52" critical="50" attack_speed="1400" max_damage="20" min_damage="16"/>
		<idian burn_attack="29" burn_defend="12"/>
	</item_template>
	<!-- :478 -->
	<item_template id="100000133" name="Plainsman's Sword" level="2" cName="sword_n_c2_02a" mask="138366" item_group="SWORD" quality="COMMON" price="200" option_slot_bonus="1" restrict="2 2 2 2 2 2 2 2 2 2 2 2 2 2 2 2 2" desc="707715" attack_type="PHYSICAL" max_enchant="10" m_slots="1">
		<modifiers>
			<add name="MAXHP" value="33" bonus="true"/>
		</modifiers>
		<weapon_stats hit_count="2" attack_range="1500" parry="199" physical_accuracy="82" critical="50" attack_speed="1400" max_damage="28" min_damage="22"/>
		<idian burn_attack="29" burn_defend="12"/>
	</item_template>
	<!-- :4443 -->
	<item_template id="100000714" name="Set Test Sword 01" level="30" cName="test_set_sword_01" mask="138366" item_group="SWORD" quality="COMMON" price="5" restrict="30 30 30 30 30 30 30 30 30 30 30 30 30 30 30 30 30" desc="743693" attack_type="PHYSICAL" can_exceed_enchant="true" enchant_type="1" max_enchant_bonus="5">
		<modifiers>
			<add name="MAXHP" value="135" bonus="true"/>
			<add name="PHYSICAL_ACCURACY" value="60" bonus="true"/>
		</modifiers>
		<weapon_stats hit_count="2" attack_range="1500" parry="173" physical_accuracy="52" critical="50" attack_speed="1400" max_damage="20" min_damage="16"/>
		<idian burn_attack="29" burn_defend="12"/>
	</item_template>
	<!-- :4807 -->
	<item_template id="100000768" name="Tahabata's Sword" level="50" cName="sword_n_e1_50a" mask="138316" pack_count="3" item_group="SWORD" quality="EPIC" price="1361300" restrict="50 50 50 50 50 50 50 50 50 50 50 50 50 50 50 50 50" desc="746164" attack_type="PHYSICAL" exceed_enchant_skill="RANK2_SET1_PHYSICAL_WEAPON" can_exceed_enchant="true" max_enchant="15" m_slots="5" temp_exchange_time="10">
		<modifiers>
			<rate name="ATTACK_SPEED" value="-19" bonus="true"/>
			<add name="PHYSICAL_ACCURACY" value="140" bonus="true"/>
			<add name="PHYSICAL_CRITICAL" value="78" bonus="true"/>
			<add name="PHYSICAL_ATTACK" value="36" bonus="true"/>
			<add name="MAXHP" value="317" bonus="true"/>
		</modifiers>
		<actions>
			<remodel type="2"/>
		</actions>
		<weapon_stats hit_count="2" attack_range="1500" magical_accuracy="285" parry="780" physical_accuracy="870" critical="50" attack_speed="1400" max_damage="197" min_damage="161"/>
		<disposition id="188950005" count="6"/>
		<uselimits pack_count="3"/>
		<idian burn_attack="29" burn_defend="12"/>
	</item_template>
	<!-- :9614 -->
	<item_template id="100001276" name="Tune, Retune_Test_Option" level="55" cName="test_sword_n_e1_55a" mask="134220" item_group="SWORD" quality="EPIC" price="2499500" rnd_count="1" rnd_bonus="1" restrict="55 55 55 55 55 55 55 55 55 55 55 55 55 55 55 55 55" desc="783931" attack_type="PHYSICAL" can_exceed_enchant="true" max_enchant="15" m_slots="5" temp_exchange_time="10">
		<modifiers>
			<add name="MAXHP" value="500" bonus="true"/>
			<add name="PHYSICAL_ATTACK" value="30" bonus="true"/>
			<add name="MAXMP" value="150" bonus="true"/>
		</modifiers>
		<weapon_stats hit_count="2" attack_range="1500" magical_accuracy="311" parry="974" physical_accuracy="936" critical="50" attack_speed="1400" max_damage="211" min_damage="171"/>
		<disposition id="188950006" count="6"/>
		<idian burn_attack="29" burn_defend="12"/>
	</item_template>
	<!-- :72382 -->
	<item_template id="100601101" name="Ancient Manastone Test Spellbook_Unique_5_0_2" level="60" cName="test_special_slot_book_01" mask="138316" item_group="SPELLBOOK" quality="UNIQUE" price="2932300" desc="797836" attack_type="MAGICAL_WATER" max_enchant="15" m_slots="6" s_slots="2">
		<modifiers>
			<add name="MAXHP" value="303" bonus="true"/>
			<add name="BOOST_MAGICAL_SKILL" value="49" bonus="true"/>
			<add name="MAGICAL_ACCURACY" value="35" bonus="true"/>
			<rate name="BOOST_CASTING_TIME" value="15" bonus="true"/>
		</modifiers>
		<weapon_stats hit_count="1" attack_range="15000" boost_magical_skill="825" magical_accuracy="453" attack_speed="2200" max_damage="253" min_damage="228"/>
		<idian burn_attack="46" burn_defend="15"/>
	</item_template>
	<!-- :189490 -->
	<item_template id="110100355" name="Plainsman's Tunic" level="4" cName="rb_torso_n_c2_04a" mask="36990" item_group="RB_TORSO" quality="COMMON" price="500" option_slot_bonus="1" restrict="4 4 4 4 4 4 4 4 4 4 4 4 4 4 4 4 4" desc="708448" max_enchant="10" m_slots="1">
		<modifiers>
			<add name="EVASION" value="45"/>
			<add name="MAGICAL_RESIST" value="31"/>
			<add name="PHYSICAL_DEFENSE" value="19"/>
			<add name="MAXHP" value="29" bonus="true"/>
			<add name="MAXMP" value="35" bonus="true"/>
		</modifiers>
	</item_template>
	<!-- :733907 -->
	<item_template id="125040180" name="Sports Cap" level="1" cName="cash_head_olimpics_01" mask="37502" item_group="HEAD" quality="COMMON" price="5" desc="780395">
		<actions>
			<remodel type="2"/>
		</actions>
	</item_template>
	<!-- :735075 -->
	<item_template id="125045164" name="Rattan Hat" level="1" cName="world_cash_head_beachhat_03a" mask="37498" item_group="HEAD" quality="COMMON" price="5" desc="825513">
		<actions>
			<remodel type="2"/>
		</actions>
	</item_template>
	<!-- :735465 -->
	<item_template id="125045242" name="Aquablue Sunhat" level="1" cName="Cash_Head_Swimsuit_01b" mask="37498" item_group="HEAD" quality="COMMON" price="5" desc="831521">
		<actions>
			<remodel type="2"/>
		</actions>
	</item_template>
	<!-- :735755 -->
	<item_template id="125045297" name="Lawful Shades" level="1" cName="world_cash_head_cop_03" mask="4730" item_group="HEAD" quality="COMMON" price="5" desc="833129">
		<actions>
			<remodel type="2"/>
		</actions>
	</item_template>
	<!-- :736000 -->
	<item_template id="125045346" name="Dapper Fedora" level="1" cName="world_cash_head_LuxurySuit_01" mask="37498" item_group="HEAD" quality="COMMON" price="5" desc="833580">
		<actions>
			<remodel type="2"/>
		</actions>
	</item_template>
	<!-- :743602 -->
	<item_template id="152000064" name="Pepento" level="60" cName="vegetable_C_60a" mask="4222" max_stack_count="10000" item_group="GATHERABLE" quality="COMMON" price="420" desc="811631"/>
	<!-- :743603 -->
	<item_template id="152000065" name="Juicy Pepento" level="60" cName="vegetable_R_60a" casting_delay="3000" mask="4222" max_stack_count="10000" item_group="GATHERABLE" quality="RARE" price="1260" desc="811632" activate_target="STANDALONE" activate_count="1">
		<actions>
			<decompose/>
		</actions>
		<uselimits usedelayid="85"/>
	</item_template>
	<!-- :828801 -->
	<item_template id="160010109" name="Enamored Tiger Form Candy" level="50" cName="cash_food_l_pink_tiger_01" mask="12410" max_stack_count="1000" quality="LEGEND" price="5" race="ELYOS" desc="769983" activate_target="STANDALONE" activate_count="1">
		<actions>
			<skilluse level="1" skillid="10275"/>
		</actions>
		<uselimits usedelay="3600000" usedelayid="24"/>
	</item_template>
	<!-- :828807 -->
	<item_template id="160010110" name="Enamored Tiger Form Candy" level="50" cName="cash_food_d_pink_tiger_01" mask="12410" max_stack_count="1000" quality="LEGEND" price="5" race="ASMODIANS" desc="769984" activate_target="STANDALONE" activate_count="1">
		<actions>
			<skilluse level="1" skillid="10276"/>
		</actions>
		<uselimits usedelay="3600000" usedelayid="24"/>
	</item_template>
	<!-- :830724 -->
	<item_template id="162000002" name="Minor Life Potion" level="10" cName="remedy_hp_10a" mask="12414" max_stack_count="1000" quality="COMMON" price="250" desc="702583" activate_target="STANDALONE" activate_count="1">
		<actions>
			<skilluse level="1" skillid="9889"/>
		</actions>
		<uselimits usedelay="30000" usedelayid="11"/>
	</item_template>
	<!-- :836407 -->
	<item_template id="165000001" name="Extraction Tools" level="1" cName="matter_extraction_01" mask="12414" max_stack_count="100" quality="COMMON" price="1000" desc="701680" activate_count="1">
		<actions>
			<extract/>
		</actions>
	</item_template>
	<!-- :837022 -->
	<item_template id="166000001" name="L1 Enchantment Stone" level="1" cName="matter_enchant_01" mask="12414" max_stack_count="100" item_group="ENCHANTMENT" quality="COMMON" price="5" desc="701681" activate_count="1">
		<actions>
			<enchant count="1"/>
		</actions>
	</item_template>
	<!-- :837972 -->
	<item_template id="166000191" name="Alpha Enchantment Stone" level="1" cName="matter_enchant_g1" mask="12414" max_stack_count="100" item_group="ENCHANTMENT" quality="COMMON" price="950" desc="842371" activate_count="1">
		<actions>
			<enchant count="1"/>
		</actions>
	</item_template>
	<!-- :837977 -->
	<item_template id="166000192" name="Beta Enchantment Stone" level="1" cName="matter_enchant_g2" mask="12414" max_stack_count="100" item_group="ENCHANTMENT" quality="COMMON" price="950" desc="842372" activate_count="1">
		<actions>
			<enchant count="10"/>
		</actions>
	</item_template>
	<!-- :837982 -->
	<item_template id="166000193" name="Gamma Enchantment Stone" level="1" cName="matter_enchant_g3" mask="12414" max_stack_count="100" item_group="ENCHANTMENT" quality="COMMON" price="950" desc="842373" activate_count="1">
		<actions>
			<enchant count="50"/>
		</actions>
	</item_template>
	<!-- :837987 -->
	<item_template id="166000194" name="Delta Enchantment Stone" level="1" cName="matter_enchant_g4" mask="12414" max_stack_count="100" item_group="ENCHANTMENT" quality="COMMON" price="950" desc="842374" activate_count="1">
		<actions>
			<enchant count="100"/>
		</actions>
	</item_template>
	<!-- :837992 -->
	<item_template id="166000195" name="Epsilon Enchantment Stone" level="1" cName="matter_enchant_g5" mask="12414" max_stack_count="100" item_group="ENCHANTMENT" quality="COMMON" price="950" desc="842375" activate_count="1">
		<actions>
			<enchant count="150"/>
		</actions>
	</item_template>
	<!-- :838880 -->
	<item_template id="166100000" name="Lesser Supplements (Heroic or Less)" level="30" cName="sub_matter_l_05" mask="12414" max_stack_count="1000" quality="LEGEND" price="5000" desc="754436">
		<actions>
			<enchant chance="5.0"/>
		</actions>
	</item_template>
	<!-- :839000 -->
	<item_template id="166150000" name="Test Manastone Only Heroic Enchantment Sand" level="50" cName="test_sub_optionprob_l_100" mask="12360" max_stack_count="1000" quality="LEGEND" price="5" desc="765843">
		<actions>
			<enchant chance="100.0" manastone_only="true"/>
		</actions>
	</item_template>
	<!-- :839031 -->
	<item_template id="166150006" name="Test Manastone Socketing Supplements [Heroic and below]" level="50" cName="test_sub_optionprob_l_20" mask="12360" max_stack_count="1000" quality="LEGEND" price="5" desc="780190">
		<actions>
			<enchant chance="100.0" min_level="31" max_level="50" manastone_only="true"/>
		</actions>
	</item_template>
	<!-- :839275 -->
	<item_template id="166500002" name="Amplification Stone" level="65" cName="exceed_enchant_key_01" mask="12414" max_stack_count="100" quality="MYTHIC" price="5" desc="841652"/>
	<!-- :839282 -->
	<item_template id="167000226" name="Manastone: HP +20" level="10" cName="matter_option_c_hp_10" mask="12414" max_stack_count="10000" item_group="MANASTONE" quality="COMMON" price="10" desc="719037" activate_count="1">
		<modifiers>
			<add name="MAXHP" value="20" bonus="true"/>
		</modifiers>
		<actions>
			<enchant count="1"/>
		</actions>
	</item_template>
	<!-- :839554 -->
	<item_template id="167000354" name="Manastone: HP +60" level="50" cName="matter_option_c_hp_50" mask="12414" max_stack_count="10000" item_group="MANASTONE" quality="COMMON" price="10" desc="719165" activate_count="1">
		<modifiers>
			<add name="MAXHP" value="60" bonus="true"/>
		</modifiers>
		<actions>
			<enchant count="3"/>
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
	<!-- :846255 -->
	<item_template id="167020024" name="Ancient Manastone: HP +105" level="70" cName="special_matter_option_hp_70" mask="12414" max_stack_count="10000" item_group="SPECIAL_MANASTONE" quality="LEGEND" price="100" desc="819827" activate_count="1">
		<modifiers>
			<add name="MAXHP" value="105" bonus="true"/>
		</modifiers>
		<actions>
			<enchant count="25"/>
		</actions>
	</item_template>
	<!-- :894666 -->
	<item_template id="182400001" name="Kinah" level="1" cName="gold" mask="12350" quality="COMMON" price="0" desc="701677"/>
	<!-- :904910 -->
	<item_template id="188050584" name="Suspicious Old Sack" level="30" cName="wrap_q_matter_enchant_50a" casting_delay="1500" mask="12364" max_stack_count="1000" quality="COMMON" price="1000" desc="764353" activate_target="STANDALONE" activate_count="1">
		<actions>
			<decompose/>
		</actions>
		<uselimits usedelay="5000" usedelayid="85"/>
	</item_template>
	<!-- :904922 -->
	<item_template id="188050586" name="Suspicious Red Sack" level="50" cName="wrap_q_matter_enchant_70a" casting_delay="1500" mask="12364" max_stack_count="1000" quality="COMMON" price="1000" desc="764355" activate_target="STANDALONE" activate_count="1">
		<actions>
			<decompose/>
		</actions>
		<uselimits usedelay="5000" usedelayid="85"/>
	</item_template>
	<!-- :905076 -->
	<item_template id="188050714" name="Chocolate Candy Box" level="1" cName="wrap_cash_candybox05" mask="12360" max_stack_count="100" quality="COMMON" price="5" desc="764228" activate_target="STANDALONE" activate_count="1">
		<actions>
			<decompose/>
		</actions>
		<uselimits usedelayid="88"/>
	</item_template>
	<!-- :907333 -->
	<item_template id="188051090" name="[Event] Cold Box" level="1" cName="wrap_event_hotbox_01a" casting_delay="1000" mask="12360" max_stack_count="100" quality="COMMON" price="5" race="ELYOS" desc="769178" activate_target="STANDALONE" activate_count="1">
		<actions>
			<decompose/>
		</actions>
		<acquisition type="REWARD" item="186000111" count="20"/>
		<uselimits usedelay="5000" usedelayid="88"/>
	</item_template>
	<!-- :922972 -->
	<item_template id="188053609" name="[Event] Level 60 Composite Manastone Bundle" level="1" cName="wrap_world_cash_matter_option_60a_china" casting_delay="1000" mask="12360" max_stack_count="1000" quality="LEGEND" price="5" desc="842323" activate_target="STANDALONE" activate_count="1">
		<actions>
			<decompose/>
		</actions>
		<uselimits usedelay="1000" usedelayid="85"/>
	</item_template>
</item_templates>)xml";

/** decomposable_items.xml, verbatim rows (the line of each <decomposable> in the comment above it) */
inline constexpr std::string_view ENHANCE_DECOMPOSABLE_ITEMS_XML = R"xml(<decomposable_items>
	<!-- :3 -->
	<decomposable item_id="152000065"><!-- Juicy Pepento -->
		<items>
			<item id="152000064" min_count="2" /><!-- Pepento -->
		</items>
	</decomposable>
	<!-- :4014 -->
	<decomposable item_id="188050584"><!-- Suspicious Old Sack -->
		<items>
			<random_item type="ENCHANTMENT" max_count="3" />
		</items>
	</decomposable>
	<!-- :4024 -->
	<decomposable item_id="188050586"><!-- Suspicious Red Sack -->
		<items>
			<random_item type="ENCHANTMENT" max_count="5" />
		</items>
	</decomposable>
	<!-- :4213 -->
	<decomposable item_id="188050714"><!-- Chocolate Candy Box -->
		<items>
			<item id="160010109" min_count="3" race="ELYOS" /><!-- Enamored Tiger Form Candy -->
			<item id="160010110" min_count="3" race="ASMODIANS" /><!-- Enamored Tiger Form Candy -->
		</items>
	</decomposable>
	<!-- :7309 -->
	<decomposable item_id="188051090" selectable="true"><!-- [Event] Cold Box -->
		<items>
			<item id="125045164" /><!-- Rattan Hat -->
			<item id="125045242" /><!-- Aquablue Sunhat -->
			<item id="125040180" /><!-- Sports Cap -->
			<item id="125045297" /><!-- Lawful Shades -->
			<item id="125045346" /><!-- Dapper Fedora -->
			<item id="188053609" min_count="3" /><!-- [Event] Level 60 Composite Manastone Bundle -->
		</items>
	</decomposable>
</decomposable_items>)xml";

/** enchant_templates.xml (game-server/data/static_data/enchants), the verbatim SWORD list (:261-325) */
inline constexpr std::string_view SWORD_ENCHANT_TEMPLATES_XML = R"xml(<enchant_templates>
	<enchant_list item_group="SWORD">
		<enchant_data level="1">
			<enchant_stat stat="PHYSICAL_ATTACK" value="2"/>
		</enchant_data>
		<enchant_data level="2">
			<enchant_stat stat="PHYSICAL_ATTACK" value="4"/>
		</enchant_data>
		<enchant_data level="3">
			<enchant_stat stat="PHYSICAL_ATTACK" value="6"/>
		</enchant_data>
		<enchant_data level="4">
			<enchant_stat stat="PHYSICAL_ATTACK" value="8"/>
		</enchant_data>
		<enchant_data level="5">
			<enchant_stat stat="PHYSICAL_ATTACK" value="10"/>
		</enchant_data>
		<enchant_data level="6">
			<enchant_stat stat="PHYSICAL_ATTACK" value="12"/>
		</enchant_data>
		<enchant_data level="7">
			<enchant_stat stat="PHYSICAL_ATTACK" value="14"/>
		</enchant_data>
		<enchant_data level="8">
			<enchant_stat stat="PHYSICAL_ATTACK" value="16"/>
		</enchant_data>
		<enchant_data level="9">
			<enchant_stat stat="PHYSICAL_ATTACK" value="18"/>
		</enchant_data>
		<enchant_data level="10">
			<enchant_stat stat="PHYSICAL_ATTACK" value="20"/>
		</enchant_data>
		<enchant_data level="11">
			<enchant_stat stat="PHYSICAL_ATTACK" value="22"/>
		</enchant_data>
		<enchant_data level="12">
			<enchant_stat stat="PHYSICAL_ATTACK" value="24"/>
		</enchant_data>
		<enchant_data level="13">
			<enchant_stat stat="PHYSICAL_ATTACK" value="26"/>
		</enchant_data>
		<enchant_data level="14">
			<enchant_stat stat="PHYSICAL_ATTACK" value="28"/>
		</enchant_data>
		<enchant_data level="15">
			<enchant_stat stat="PHYSICAL_ATTACK" value="30"/>
		</enchant_data>
		<enchant_data level="16">
			<enchant_stat stat="PHYSICAL_ATTACK" value="32"/>
		</enchant_data>
		<enchant_data level="17">
			<enchant_stat stat="PHYSICAL_ATTACK" value="34"/>
		</enchant_data>
		<enchant_data level="18">
			<enchant_stat stat="PHYSICAL_ATTACK" value="36"/>
		</enchant_data>
		<enchant_data level="19">
			<enchant_stat stat="PHYSICAL_ATTACK" value="38"/>
		</enchant_data>
		<enchant_data level="20">
			<enchant_stat stat="PHYSICAL_ATTACK" value="40"/>
		</enchant_data>
		<enchant_data level="21">
			<enchant_stat stat="PHYSICAL_ATTACK" value="2"/>
		</enchant_data>
	</enchant_list>
</enchant_templates>)xml";

/**
 * Sets a reloadable configuration value for the scope and restores the previous snapshot. ConfigValue.get() is never null (a value that was
 * never set is the value-initialized T, ConfigValue.h:45-55), so the value before the scope, the empty default included, is always restored
 */
template <class T>
class ConfigValueScope {
public:
	ConfigValueScope(commons::configuration::ConfigValue<T>& configValue, T value) : config(configValue), previous(configValue.get()) {
		config.set(std::move(value));
	}
	~ConfigValueScope() { config.set(*previous); }
	ConfigValueScope(const ConfigValueScope&) = delete;
	ConfigValueScope& operator=(const ConfigValueScope&) = delete;

private:
	commons::configuration::ConfigValue<T>& config;
	const std::shared_ptr<const T> previous;
};

class EnhanceTest : public ItemServicesTest {
protected:
	void SetUp() override {
		ItemServicesTest::SetUp();
		// the base fixture's item rows, replaced by this lane's, bound in one load context with the decomposables (ResultedItem finds its item
		// through the context's XmlIDs, ResultedItem.cpp)
		dataholders::DataManager::ITEM_DATA.resetForTests();
		xml::LoadContext context;
		dataholders::DataManager::ITEM_DATA.publish(xml::bindString<dataholders::ItemData>(context, ENHANCE_ITEM_TEMPLATES_XML));
		dataholders::DataManager::DECOMPOSABLE_ITEMS_DATA.publish(
			xml::bindString<dataholders::DecomposableItemsData>(context, ENHANCE_DECOMPOSABLE_ITEMS_XML));
		// the shipped profile: rates.properties:32, :36, :40; prices.properties:8, :12, :16; admin.properties:79
		manastoneChances = std::make_unique<ConfigValueScope<std::vector<float>>>(configs::main::RatesConfig::MANASTONE_CHANCES, std::vector<float>{75.0f, 75.0f});
		baseChances = std::make_unique<ConfigValueScope<std::vector<float>>>(configs::main::RatesConfig::ENCHANTMENT_STONE_BASE_CHANCES,
			std::vector<float>{65.0f, 65.0f});
		amplifiedChances = std::make_unique<ConfigValueScope<std::vector<float>>>(configs::main::RatesConfig::ENCHANTMENT_STONE_AMPLIFIED_CHANCES,
			std::vector<float>{61.0f, 61.0f});
		prices = std::make_unique<AtomicConfigScope<int32_t>>(configs::main::PricesConfig::DEFAULT_PRICES, 100);
		modifier = std::make_unique<AtomicConfigScope<int32_t>>(configs::main::PricesConfig::DEFAULT_MODIFIER, 100);
		taxes = std::make_unique<AtomicConfigScope<int32_t>>(configs::main::PricesConfig::DEFAULT_TAXES, 100);
		enchantInfo = std::make_unique<AtomicConfigScope<int8_t>>(configs::administration::AdminConfig::ENCHANT_INFO, int8_t{9});
		savedGenerator = std::make_unique<commons::utils::Rnd::Xoshiro256PlusPlus>(commons::utils::Rnd::generator());
	}

	void TearDown() override {
		commons::utils::Rnd::generator() = *savedGenerator;
		enchantInfo.reset();
		taxes.reset();
		modifier.reset();
		prices.reset();
		amplifiedChances.reset();
		baseChances.reset();
		manastoneChances.reset();
		ItemServicesTest::TearDown();
		dataholders::DataManager::DECOMPOSABLE_ITEMS_DATA.resetForTests();
		dataholders::DataManager::ENCHANT_DATA.resetForTests(); // published by the equipped-item cases only
		dataholders::DataManager::ITEM_SET_DATA.resetForTests(); // likewise
	}

	/**
	 * ENCHANT_DATA with the SWORD list above, for applyEnchantEffect on an equipped sword, and an empty ITEM_SET_DATA: no sword of the fixture is
	 * in a set of item_sets.xml, so the empty holder answers isItemSet() as the shipped one does (TearDown resets both)
	 */
	void publishSwordEnchantData() {
		xml::LoadContext context;
		dataholders::DataManager::ENCHANT_DATA.publish(xml::bindString<dataholders::EnchantData>(context, SWORD_ENCHANT_TEMPLATES_XML));
		dataholders::DataManager::ITEM_SET_DATA.publish(std::make_unique<dataholders::ItemSetData>());
	}

	/**
	 * A sword the DAO loads as equipped in the main hand (slot 1) at the enchant level, through Equipment.onLoadHandler (no stats applied). The
	 * player first gets skill 37: a loaded player has its skill list before its equipment (PlayerService.java:116), and onLoadHandler keeps a
	 * sword only for a player with one of its skills (Equipment.java:437-440, 303-314; ItemGroup.java:16, SWORD requires 37 or 44)
	 */
	Item& equippedSword(int32_t objId, int32_t itemId, int32_t enchant = 0) {
		player().setSkillList(model::skill::PlayerSkillList::create(
			{model::skill::PlayerSkillEntry::create(37, 1, 0, model::gameobjects::Persistable_PersistentState::UPDATED)}));
		Ref<Item> sword = Item::create(objId, itemId, 1, std::nullopt, 0, "", 0, 0, true, false, 1, model::items::storage::getId(StorageType::CUBE),
			enchant, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, false, 0, 0);
		items.push_back(sword);
		player().getEquipment().onLoadHandler(*sword);
		EXPECT_EQ(player().getEquipment().getMainHandWeapon().get(), sword.get()) << "the sword is equipped in the main hand";
		return *sword;
	}

	/** An item of the template with the columns the DAO would load, in the cube, not yet in any storage */
	Ref<Item> itemRow(int32_t objId, int32_t itemId, int64_t count, int32_t enchant = 0, int32_t optionalSockets = 0, bool amplified = false) {
		Ref<Item> item = Item::create(objId, itemId, count, std::nullopt, 0, "", 0, 0, false, false, 0,
			model::items::storage::getId(StorageType::CUBE), enchant, 0, 0, 0, optionalSockets, 0, 0, 0, 0, 0, 0, 0, amplified, 0, 0);
		items.push_back(item);
		return item;
	}

	/** itemRow, loaded into the cube the way the DAO does (no packet) */
	Item& inCube(int32_t objId, int32_t itemId, int64_t count, int32_t enchant = 0, int32_t optionalSockets = 0, bool amplified = false) {
		Ref<Item> item = itemRow(objId, itemId, count, enchant, optionalSockets, amplified);
		storage(StorageType::CUBE).onLoadHandler(*item);
		return *item;
	}

	/** PacketSendUtility.sendMessage(player, text): SM_MESSAGE(0, null, text, GOLDEN_YELLOW) (PacketSendUtility.cpp:34-36) */
	std::vector<uint8_t> message(std::string_view text) {
		return serialized(network::aion::serverpackets::SM_MESSAGE(0, "", text, model::ChatType::GOLDEN_YELLOW));
	}

	/** Every captured packet with the given Java opcode */
	std::vector<std::vector<uint8_t>> sentWithOpcode(int32_t opcode) {
		std::vector<std::vector<uint8_t>> matching;
		for (const std::vector<uint8_t>& packet : sent()) {
			if (javaOpcodeOf(packet) == opcode)
				matching.push_back(packet);
		}
		return matching;
	}

	/** The count of the cube's stacks of an item id (0 if none) */
	int64_t cubeCount(int32_t itemId) { return player().getInventory().getItemCountByItemId(itemId); }

	std::unique_ptr<ConfigValueScope<std::vector<float>>> manastoneChances;
	std::unique_ptr<ConfigValueScope<std::vector<float>>> baseChances;
	std::unique_ptr<ConfigValueScope<std::vector<float>>> amplifiedChances;
	std::unique_ptr<AtomicConfigScope<int32_t>> prices;
	std::unique_ptr<AtomicConfigScope<int32_t>> modifier;
	std::unique_ptr<AtomicConfigScope<int32_t>> taxes;
	std::unique_ptr<AtomicConfigScope<int8_t>> enchantInfo;
	std::unique_ptr<commons::utils::Rnd::Xoshiro256PlusPlus> savedGenerator;
};

} // namespace aion::gameserver::services::item::test
