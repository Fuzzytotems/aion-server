#pragma once

// Shared fixture of the M5c stage-1 economy client packet tests (m5c-plan.md K-01, K-02; P5-15's tests/cm_ak and P5-16's tests/cm_lz, which
// includes this header by relative path as it includes ItemPacketTestSupport.h): CM_BUY_ITEM, the six CM_EXCHANGE_*, the five mail packets,
// CM_PRIVATE_STORE, CM_PRIVATE_STORE_NAME, CM_TUNE, CM_TUNE_RESULT, CM_SELECT_DECOMPOSABLE and CM_USE_ITEM's target-item lookup.
//
// - EconomyPacketTest is ItemPacketTest (a spawned level-1 ELYOS warrior "Holder" in Poeta, a real AionConnection whose send queue the cases
//   read) with the item rows of ItemPacketTestSupport.h plus the rows below, bound in one load context together with the decomposable rows (a
//   ResultedItem finds its item through the context's XmlIDs, ResultedItem.cpp:13-17), the npc, trade and goods rows the merchant cases need,
//   the audit log on, and second players on demand (otherPlayer) standing in the World, so World.getPlayer finds them.
// - Every row is copied verbatim from the shipped data (game-server/data/static_data, file:line beside each).
// - Prices come from the shipped prices.properties defaults (PricesConfig.java: 100/100/100, vendor buy 100, sell 20) with no siege location,
//   and the sell limits are off (m5c.properties.example: gameserver.limits.enable = false): Minor Life Elixir costs 352, Extraction Tools 1,412
//   and a Minor Life Potion sells for 50 at minalinerk (tests/economy/P5-09b/TradeServiceTest.cpp derives them; `oracle.py m5c-trade --npc
//   798007 --set gameserver.siege.enable=false` gives the same numbers).
// - The fixture does not use tests/support/WorldTestSupport.h: ItemPacketTestSupport.h publishes the Poeta world holders once per process, and
//   WorldTestSupport refuses a second publisher inside one executable (m5c-plan.md §18.7). World::getInstance() builds its maps from those
//   same holders.
// - EconomyDriver reads a body on a connection (CM_BUY_ITEM's readImpl asks the connection for the active player) and keeps the packet for
//   the TestAccess friends; runNow calls runImpl directly, so an exception the Java body throws reaches the case (AionClientPacket::run
//   would log it).

#include "ItemPacketTestSupport.h"

#include <atomic>
#include <cstdint>
#include <deque>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "aion/commons/utils/ByteBuffer.h"
#include "aion/commons/utils/Rnd.h"
#include "aion/gameserver/configs/main/AIConfig.h"
#include "aion/gameserver/configs/main/CustomConfig.h"
#include "aion/gameserver/configs/main/LoggingConfig.h"
#include "aion/gameserver/configs/main/PricesConfig.h"
#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/controllers/effect/EffectController.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/DecomposableItemsData.bind.h"
#include "aion/gameserver/dataholders/DecomposableItemsData.h"
#include "aion/gameserver/dataholders/GoodsListData.bind.h"
#include "aion/gameserver/dataholders/GoodsListData.h"
#include "aion/gameserver/dataholders/ItemData.bind.h"
#include "aion/gameserver/dataholders/ItemData.h"
#include "aion/gameserver/dataholders/NpcData.bind.h"
#include "aion/gameserver/dataholders/NpcData.h"
#include "aion/gameserver/dataholders/TradeListData.bind.h"
#include "aion/gameserver/dataholders/TradeListData.h"
#include "aion/gameserver/model/Race.h"
#include "aion/gameserver/model/gameobjects/Npc.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/gameobjects/player/PrivateStore.h"
#include "aion/gameserver/model/gameobjects/player/ResponseRequester.h"
#include "aion/gameserver/model/templates/spawns/SpawnGroup.h"
#include "aion/gameserver/model/templates/spawns/SpawnTemplate.h"
#include "aion/gameserver/network/aion/AionConnection.h"
#include "aion/gameserver/network/aion/StateSet.h"
#include "aion/gameserver/services/ExchangeService.h"
#include "aion/gameserver/services/RepurchaseService.h"
#include "aion/gameserver/world/World.h"
#include "aion/gameserver/world/WorldPosition.h"
#include "aion/gameserver/world/knownlist/NpcKnownList.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets::testing::items {

// the item ids of the rows below
inline constexpr int32_t MODORS_SWORD = 100001551;
inline constexpr int32_t STALWART_DANDI_CANDY_ELYOS = 160010297;
inline constexpr int32_t CLEVER_DANDI_CANDY_ELYOS = 160010298;
inline constexpr int32_t STALWART_DANDI_CANDY_ASMODIANS = 160010299;
inline constexpr int32_t CLEVER_DANDI_CANDY_ASMODIANS = 160010300;
inline constexpr int32_t SWIFT_DANDI_CANDY_ELYOS = 160010301;
inline constexpr int32_t SWIFT_DANDI_CANDY_ASMODIANS = 160010302;
inline constexpr int32_t MINOR_LIFE_ELIXIR = 162000052;
inline constexpr int32_t EXTRACTION_TOOLS = 165000001;
inline constexpr int32_t MYTHIC_WEAPON_TUNING_SCROLL = 166200009;
inline constexpr int32_t MYTHIC_ARMOR_TUNING_SCROLL = 166200010;
inline constexpr int32_t WORN_METAL_POWDER = 186000290;
inline constexpr int32_t DANDY_FORM_CANDY_BOX = 188052914;
inline constexpr int32_t COLD_BOX = 188051090;
inline constexpr int32_t COMPOSITE_MANASTONE_BUNDLE = 188053609;

// the npc ids of the rows below
inline constexpr int32_t MINALINERK = 798007;
inline constexpr int32_t BAEVRUNERK = 798008;
inline constexpr int32_t MOGIRONERK = 798088;
inline constexpr int32_t PIARINERK = 801531;
inline constexpr int32_t LALRINERK = 833548;

// minalinerk's prices under the fixture's configuration (see the header comment)
inline constexpr int64_t ELIXIR_PRICE = 352;  // PricesService.getBuyPrice(250): 250 -> 312 -> 312 -> 352
inline constexpr int64_t TOOLS_PRICE = 1412;  // getBuyPrice(1000): 1000 -> 1250 -> 1250 -> 1412
inline constexpr int64_t POTION_REWARD = 50;  // getSellReward(250, 20)
// piarinerk's purchase-list rate (TradeService.java:213): (long) (price 2200 * buy_price_rate 50 / 100D)
inline constexpr int64_t POWDER_PURCHASE_REWARD = 1100;

// ServerPacketsOpcodes.java:55-306
inline constexpr int32_t SM_EXCHANGE_REQUEST_OPCODE = 74;
inline constexpr int32_t SM_EXCHANGE_ADD_ITEM_OPCODE = 75;
inline constexpr int32_t SM_EXCHANGE_ADD_KINAH_OPCODE = 77;
inline constexpr int32_t SM_EXCHANGE_CONFIRMATION_OPCODE = 78;
inline constexpr int32_t SM_PRIVATE_STORE_OPCODE = 134;
inline constexpr int32_t SM_PRIVATE_STORE_NAME_OPCODE = 145;
inline constexpr int32_t SM_SECONDARY_SHOW_DECOMPOSABLE_OPCODE = 286;
inline constexpr int32_t SM_TUNE_RESULT_OPCODE = 288;

inline constexpr const char* AUDIT_LOGGER = "AUDIT_LOG";
inline constexpr const char* BASE_CLIENT_PACKET_LOGGER = "com.aionemu.commons.network.packet.BaseClientPacket";

/** item_templates.xml, verbatim rows appended to ItemPacketTestSupport.h's (the line of each <item_template> in the comment above it) */
inline constexpr std::string_view ECONOMY_ITEM_TEMPLATES_XML = R"xml(
	<!-- :13282 -->
	<item_template id="100001551" name="Modor's Sword" level="65" cName="sword_n_e_idunderrune_65a" mask="136268" item_group="SWORD" quality="EPIC" price="2687303" rnd_count="3" rnd_bonus="119" option_slot_bonus="1" restrict="65 65 65 65 65 65 65 65 65 65 65 65 65 65 65 65 65" desc="819871" attack_type="PHYSICAL" exceed_enchant_skill="RANK3_SET1_PHYSICAL_WEAPON" can_exceed_enchant="true" max_enchant_bonus="2" max_enchant="13" m_slots="5" s_slots="1">
		<modifiers>
			<add name="PARRY" value="138" bonus="true"/>
			<add name="PHYSICAL_ATTACK" value="34" bonus="true"/>
			<add name="PHYSICAL_CRITICAL" value="50" bonus="true"/>
			<add name="PHYSICAL_ACCURACY" value="123" bonus="true"/>
			<rate name="ATTACK_SPEED" value="-17" bonus="true"/>
		</modifiers>
		<actions>
			<remodel type="0"/>
		</actions>
		<weapon_stats hit_count="2" attack_range="1500" magical_accuracy="371" parry="1124" physical_accuracy="1096" critical="50" attack_speed="1400" max_damage="258" min_damage="211"/>
		<disposition id="188950015" count="6"/>
		<idian burn_attack="34" burn_defend="14"/>
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
	<!-- :829934 -->
	<item_template id="160010297" name="[Event] Stalwart Dandi Form Candy" level="40" cName="Event_food_l_Ncdinos_Phy_01" mask="12410" max_stack_count="1000" quality="RARE" price="5" race="ELYOS" desc="831585" activate_target="STANDALONE" activate_count="1">
		<actions>
			<skilluse level="1" skillid="10812"/>
		</actions>
		<uselimits usedelay="3600000" usedelayid="24"/>
	</item_template>
	<!-- :829940 -->
	<item_template id="160010298" name="[Event] Clever Dandi Form Candy" level="40" cName="Event_food_l_Ncdinos_Mag_01" mask="12410" max_stack_count="1000" quality="RARE" price="5" race="ELYOS" desc="831586" activate_target="STANDALONE" activate_count="1">
		<actions>
			<skilluse level="1" skillid="10813"/>
		</actions>
		<uselimits usedelay="3600000" usedelayid="24"/>
	</item_template>
	<!-- :829946 -->
	<item_template id="160010299" name="[Event] Stalwart Dandi Form Candy" level="40" cName="Event_food_d_Ncdinos_Phy_01" mask="12410" max_stack_count="1000" quality="RARE" price="5" race="ASMODIANS" desc="831587" activate_target="STANDALONE" activate_count="1">
		<actions>
			<skilluse level="1" skillid="10814"/>
		</actions>
		<uselimits usedelay="3600000" usedelayid="24"/>
	</item_template>
	<!-- :829952 -->
	<item_template id="160010300" name="[Event] Clever Dandi Form Candy" level="40" cName="Event_food_d_Ncdinos_Mag_01" mask="12410" max_stack_count="1000" quality="RARE" price="5" race="ASMODIANS" desc="831588" activate_target="STANDALONE" activate_count="1">
		<actions>
			<skilluse level="1" skillid="10815"/>
		</actions>
		<uselimits usedelay="3600000" usedelayid="24"/>
	</item_template>
	<!-- :829958 -->
	<item_template id="160010301" name="[Event] Swift Dandi Form Candy" level="40" cName="Event_food_l_Ncdinos_Speed_01" mask="12410" max_stack_count="1000" quality="RARE" price="5" race="ELYOS" desc="831593" activate_target="STANDALONE" activate_count="1">
		<actions>
			<skilluse level="1" skillid="10816"/>
		</actions>
		<uselimits usedelay="3600000" usedelayid="24"/>
	</item_template>
	<!-- :829964 -->
	<item_template id="160010302" name="[Event] Swift Dandi Form Candy" level="40" cName="Event_food_d_Ncdinos_Speed_01" mask="12410" max_stack_count="1000" quality="RARE" price="5" race="ASMODIANS" desc="831594" activate_target="STANDALONE" activate_count="1">
		<actions>
			<skilluse level="1" skillid="10817"/>
		</actions>
		<uselimits usedelay="3600000" usedelayid="24"/>
	</item_template>
	<!-- :831033 -->
	<item_template id="162000052" name="Minor Life Elixir" level="10" cName="shop_remedy_hp_10a" mask="12414" max_stack_count="1000" quality="COMMON" price="250" desc="741694" activate_target="STANDALONE" activate_count="1">
		<actions>
			<skilluse level="1" skillid="10202"/>
		</actions>
		<uselimits usedelay="60000" usedelayid="11"/>
	</item_template>
	<!-- :836407 -->
	<item_template id="165000001" name="Extraction Tools" level="1" cName="matter_extraction_01" mask="12414" max_stack_count="100" quality="COMMON" price="1000" desc="701680" activate_count="1">
		<actions>
			<extract/>
		</actions>
	</item_template>
	<!-- :839186 -->
	<item_template id="166200009" name="Mythic Weapon Tuning Scroll" level="65" cName="cash_weapon_reidentify_m_65a" mask="12414" max_stack_count="100" quality="MYTHIC" price="5" desc="825736" activate_count="1">
		<actions>
			<tuning no_reduce="false" target="WEAPON"/>
		</actions>
	</item_template>
	<!-- :839191 -->
	<item_template id="166200010" name="Mythic Armor Tuning Scroll" level="65" cName="cash_armor_reidentify_m_65a" mask="12414" max_stack_count="100" quality="MYTHIC" price="5" desc="825737" activate_count="1">
		<actions>
			<tuning no_reduce="false" target="ARMOR"/>
		</actions>
	</item_template>
	<!-- :896883 -->
	<item_template id="186000290" name="Worn Metal Powder" level="65" cName="junk_rock_c02_65a" mask="12414" max_stack_count="1000" quality="COMMON" price="2200" desc="824990"/>
	<!-- :907333 -->
	<item_template id="188051090" name="[Event] Cold Box" level="1" cName="wrap_event_hotbox_01a" casting_delay="1000" mask="12360" max_stack_count="100" quality="COMMON" price="5" race="ELYOS" desc="769178" activate_target="STANDALONE" activate_count="1">
		<actions>
			<decompose/>
		</actions>
		<acquisition type="REWARD" item="186000111" count="20"/>
		<uselimits usedelay="5000" usedelayid="88"/>
	</item_template>
	<!-- :918762 -->
	<item_template id="188052914" name="[Event] Dandy Form Candy Box" level="1" cName="wrap_event_candy_indiana_01" casting_delay="1000" mask="12360" max_stack_count="1000" quality="RARE" price="5" desc="831597" activate_target="STANDALONE" activate_count="1">
		<actions>
			<decompose/>
		</actions>
		<uselimits usedelay="1000" usedelayid="85"/>
	</item_template>
	<!-- :922972 -->
	<item_template id="188053609" name="[Event] Level 60 Composite Manastone Bundle" level="1" cName="wrap_world_cash_matter_option_60a_china" casting_delay="1000" mask="12360" max_stack_count="1000" quality="LEGEND" price="5" desc="842323" activate_target="STANDALONE" activate_count="1">
		<actions>
			<decompose/>
		</actions>
		<uselimits usedelay="1000" usedelayid="85"/>
	</item_template>
)xml";

/** decomposable_items/decomposable_items.xml, verbatim rows (the line of each <decomposable> in the comment above it) */
inline constexpr std::string_view ECONOMY_DECOMPOSABLE_ITEMS_XML = R"xml(<decomposable_items>
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
	<!-- :26565 -->
	<decomposable item_id="188052914" selectable="true"><!-- [Event] Dandy Form Candy Box -->
		<items>
			<item id="160010298" race="ELYOS" /><!-- [Event] Clever Dandi Form Candy -->
			<item id="160010300" race="ASMODIANS" /><!-- [Event] Clever Dandi Form Candy -->
			<item id="160010301" race="ELYOS" /><!-- [Event] Swift Dandi Form Candy -->
			<item id="160010302" race="ASMODIANS" /><!-- [Event] Swift Dandi Form Candy -->
			<item id="160010297" race="ELYOS" /><!-- [Event] Stalwart Dandi Form Candy -->
			<item id="160010299" race="ASMODIANS" /><!-- [Event] Stalwart Dandi Form Candy -->
		</items>
	</decomposable>
</decomposable_items>)xml";

/**
 * npcs/npc_templates.xml, verbatim rows: minalinerk, baevrunerk and lalrinerk (the rows tests/cm_ak/DialogSelectPacketsTest.cpp talks to),
 * mogironerk (BUY and SELL without a trade row: Npc.canBuy through the SELL dialog alone, canSell false) and piarinerk (a purchase list only:
 * Npc.canPurchase)
 */
inline constexpr std::string_view ECONOMY_NPC_TEMPLATES_XML = R"xml(<npc_templates>
	<!-- :461604-461610, BUY and SELL -->
	<npc_template npc_id="798007" level="9" name="minalinerk" name_id="351126" height="1.16875" title_id="350377" group_drop="NONE" rank="DISCIPLINED" rating="NORMAL" race="BROWNIE" tribe="GENERAL" type="GENERAL" ai="general" srange="20" sangle="240" attack_speed="2100" hpgauge="3">
		<stats maxHp="2568">
			<speeds walk="1.5" group_walk="1.5" run="4.23" run_fight="4.23" group_run_fight="4.23" />
		</stats>
		<bound_radius front="0.595" side="0.3774" upper="1.16875" />
		<talk_info distance="5" is_dialog="true" func_dialogs="2 3" can_talk_invisible="false" />
	</npc_template>
	<!-- :461611-461617, EXTEND_INVENTORY only -->
	<npc_template npc_id="798008" level="9" name="baevrunerk" name_id="351141" height="1.16875" title_id="350421" group_drop="NONE" rank="DISCIPLINED" rating="NORMAL" race="BROWNIE" tribe="GENERAL" type="GENERAL" ai="general" srange="20" sangle="240" attack_speed="2100" hpgauge="3">
		<stats maxHp="2568">
			<speeds walk="1.5" group_walk="1.5" run="4.23" run_fight="4.23" group_run_fight="4.23" />
		</stats>
		<bound_radius front="0.595" side="0.3774" upper="1.16875" />
		<talk_info distance="5" is_dialog="true" func_dialogs="47" can_talk_invisible="false" />
	</npc_template>
	<!-- :462170-462176, BUY and SELL, no trade row -->
	<npc_template npc_id="798088" level="1" name="mogironerk" name_id="353292" height="1.16875" title_id="350530" group_drop="NONE" rank="DISCIPLINED" rating="NORMAL" race="BROWNIE" tribe="GENERAL_DARK" type="GENERAL" ai="general" srange="20" sangle="240" attack_speed="2000" hpgauge="3">
		<stats maxHp="14535">
			<speeds walk="1.5" group_walk="1.5" run="4.23" run_fight="4.23" group_run_fight="4.23" />
		</stats>
		<bound_radius front="0.595" side="0.3774" upper="1.16875" />
		<talk_info distance="5" is_dialog="true" func_dialogs="2 3" can_talk_invisible="false" />
	</npc_template>
	<!-- :497864-497870, TRADE_SELL_LIST (103) only: the purchase row below -->
	<npc_template npc_id="801531" level="65" name="piarinerk" name_id="373150" height="1.16875" title_id="463203" group_drop="NONE" rank="DISCIPLINED" rating="NORMAL" race="BROWNIE" tribe="USEALL" type="GENERAL" ai="general" srange="20" sangle="240" attack_speed="2000" hpgauge="3">
		<stats maxHp="26116">
			<speeds walk="1.5" group_walk="1.5" run="4.23" run_fight="4.23" group_run_fight="4.23" />
		</stats>
		<bound_radius front="0.595" side="0.3774" upper="1.16875" />
		<talk_info distance="5" is_dialog="true" func_dialogs="103" can_talk_invisible="false" />
	</npc_template>
	<!-- :562457-562463, BUY and SELL for level 55 and above (LEVEL_HIGH) -->
	<npc_template npc_id="833548" level="1" name="lalrinerk" name_id="466577" height="1.375" title_id="466578" group_drop="NONE" rank="DISCIPLINED" rating="NORMAL" tribe="USEALL" type="GENERAL" ai="aggressive" srange="20" attack_speed="2000" hpgauge="3">
		<stats maxHp="124">
			<speeds walk="1.5" group_walk="1.5" run="4.23" run_fight="4.23" group_run_fight="4.23" />
		</stats>
		<bound_radius front="0.7" side="0.444" upper="1.375" />
		<talk_info distance="5" is_dialog="true" func_dialogs="2 3" subdialog_type="LEVEL_HIGH" subdialog_value="55" can_talk_invisible="false" />
	</npc_template>
</npc_templates>)xml";

/**
 * npc_trade_list.xml, verbatim: every shipped row of the npcs above, in the file's order - minalinerk's goods (:2186-2189), baevrunerk's
 * (:2190-2192; his dialogs have neither BUY nor SELL, so Npc.canSell and canBuy are false all the same), lalrinerk's REWARD goods (:8939-8941;
 * with BUY in his dialogs canSell is true, and his goods list 5203 is left out below: nothing may be bought from him before the interaction
 * audit, and the list's weekly salestime and limits belong to LimitedItemTradeService) and piarinerk's purchase row (:9874-9876). mogironerk
 * has no row.
 */
inline constexpr std::string_view ECONOMY_NPC_TRADE_LIST_XML = R"xml(<npc_trade_list>
	<tradelist_template npc_id="798007" buy_price_rate="200">
		<tradelist id="132" />
		<tradelist id="720" />
	</tradelist_template>
	<tradelist_template npc_id="798008" buy_price_rate="200">
		<tradelist id="132" />
	</tradelist_template>
	<tradelist_template npc_id="833548" npc_type="REWARD">
		<tradelist id="5203" />
	</tradelist_template>
	<purchase_template npc_id="801531" buy_price_rate="50">
		<tradelist id="13" />
	</purchase_template>
</npc_trade_list>)xml";

/** goodslists/goodslists.xml, verbatim: minalinerk's two tabs (:13484-13488, :27984-27987; baevrunerk's 132 too) and piarinerk's purchase list (:75998-76003) */
inline constexpr std::string_view ECONOMY_GOODSLISTS_XML = R"xml(<goodslists>
    <list id="132">
        <item id="169000003"/>
        <item id="165000001"/>
        <item id="169300002"/>
    </list>
    <list id="720">
        <item id="162000052"/>
        <item id="162000057"/>
    </list>
    <purchase_list id="13">
        <item id="186000290"/>
        <item id="186000308"/>
        <item id="182007029"/>
        <item id="182007047"/>
    </purchase_list>
</goodslists>)xml";

/** ItemPacketTestSupport.h's item rows with the rows above appended inside the same <item_templates> */
inline std::string economyItemTemplatesXml() {
	std::string xml(ITEM_TEMPLATES_XML);
	xml.insert(xml.rfind("</item_templates>"), ECONOMY_ITEM_TEMPLATES_XML);
	return xml;
}

/** The npc templates the npcs are created from, kept for the process as DataManager keeps its own (the published NPC_DATA is reset) */
inline const dataholders::NpcData& economyNpcTemplates() {
	static const dataholders::NpcData* holder = [] {
		static xml::LoadContext context;
		return xml::bindString<dataholders::NpcData>(context, ECONOMY_NPC_TEMPLATES_XML).release();
	}();
	return *holder;
}

/**
 * A client packet read from a body on a connection and kept for inspection: CM_BUY_ITEM's readImpl asks the connection for the active player,
 * so its bodies cannot be read without one. runNow calls runImpl directly (the PacketProcessor's AionClientPacket::run would catch and log what
 * a Java body throws).
 */
template <class P>
class EconomyDriver final : public P {
public:
	explicit EconomyDriver(int32_t opcode) : P(opcode, StateSet{AionConnection_State::IN_GAME}) {}

	/** read() on the body; the body stays alive with the packet (the buffer wraps it) */
	bool readOn(const std::vector<uint8_t>& data, const std::shared_ptr<AionConnection>& connection) {
		body = data;
		this->setBuffer(commons::utils::ByteBuffer::wrap(body));
		if (connection)
			this->setConnection(connection);
		return this->read();
	}

	int32_t unread() const { return this->getRemainingBytes(); }

	void runNow() { this->runImpl(); }

private:
	std::vector<uint8_t> body;
};

/**
 * A body read without a connection (the packets whose readImpl does not ask it): read() and the bytes it left, or nullptr when read() failed
 */
template <class P>
std::unique_ptr<EconomyDriver<P>> readAlone(int32_t opcode, const std::vector<uint8_t>& body, int32_t& unread) {
	auto packet = std::make_unique<EconomyDriver<P>>(opcode);
	if (!packet->readOn(body, nullptr))
		return nullptr;
	unread = packet->unread();
	return packet;
}

class EconomySpawnTemplate final : public model::templates::spawns::SpawnTemplate {
public:
	EconomySpawnTemplate(model::templates::spawns::SpawnGroup& group, float x, float y, float z)
		: SpawnTemplate(group, x, y, z, int8_t{0}, 0, std::nullopt, 0, 0, std::nullopt) {}
};

/** How many entries of the generated table name `className`, each checked against `expectedOpcode` and IN_GAME */
inline int32_t economyTableEntries(std::string_view className, int32_t expectedOpcode) {
	using enum AionConnection_State;
	int32_t found = 0;
#define AION_CLIENT_PACKET_INFO(opcode, wireOpcode, Class, clientName, ...)                                                                             \
	if (std::string_view(#Class) == className) {                                                                                                        \
		EXPECT_EQ(opcode, expectedOpcode) << className;                                                                                                 \
		EXPECT_EQ((StateSet{__VA_ARGS__}), (StateSet{IN_GAME})) << className;                                                                          \
		++found;                                                                                                                                        \
	}
#include "aion/gameserver/network/aion/ClientPacketInfo.gen.inc"
#undef AION_CLIENT_PACKET_INFO
	return found;
}

/** A configuration value set for the fixture's lifetime and restored after it */
template <class T>
class EconomyConfigScope {
public:
	EconomyConfigScope(std::atomic<T>& configValue, T value) : config(configValue), previous(configValue.exchange(value)) {}
	~EconomyConfigScope() { config.store(previous); }
	EconomyConfigScope(const EconomyConfigScope&) = delete;
	EconomyConfigScope& operator=(const EconomyConfigScope&) = delete;

private:
	std::atomic<T>& config;
	const T previous;
};

/**
 * SM_ITEM_USAGE_ANIMATION(playerObjId, itemObjId, itemId, time, end, unk) (SM_ITEM_USAGE_ANIMATION.java:41-49; the 3-argument constructor at
 * :22-30 is time 0, end 1, unk 1): writeImpl (:75-88) D player, D target (the player), D item object, D item id, D time, C end, C unk 0, C unk1 0,
 * C unk2 (the field initializer's 1), D unk3
 */
inline std::vector<uint8_t> usageAnimation(int32_t playerObjId, int32_t itemObjId, int32_t itemId, int32_t time, int32_t end, int32_t unk) {
	return javaPacket(SM_ITEM_USAGE_ANIMATION_OPCODE,
		PacketWriter().D(playerObjId).D(playerObjId).D(itemObjId).D(itemId).D(time).C(end).C(0).C(0).C(1).D(unk));
}

/** SM_EXCHANGE_CONFIRMATION (SM_EXCHANGE_CONFIRMATION.java writeImpl): C(action) */
inline std::vector<uint8_t> exchangeConfirmation(int32_t action) {
	return javaPacket(SM_EXCHANGE_CONFIRMATION_OPCODE, PacketWriter().C(action));
}

/** SM_EXCHANGE_REQUEST (SM_EXCHANGE_REQUEST.java writeImpl): S(the other player's name) */
inline std::vector<uint8_t> exchangeRequest(std::string_view name) {
	return javaPacket(SM_EXCHANGE_REQUEST_OPCODE, PacketWriter().S(name));
}

/** SM_EXCHANGE_ADD_KINAH (SM_EXCHANGE_ADD_KINAH.java writeImpl): C(action: 0 self, 1 the partner), Q(kinahCount) */
inline std::vector<uint8_t> exchangeAddKinah(int64_t count, int32_t action) {
	return javaPacket(SM_EXCHANGE_ADD_KINAH_OPCODE, PacketWriter().C(action).Q(count));
}

class EconomyPacketTest : public ItemPacketTest {
protected:
	/** A second player with his own connection, standing in the World (World.getPlayer finds him) */
	struct OtherPlayer {
		PlayerFixture f;
		std::unique_ptr<TestClient> client;

		model::gameobjects::player::Player& player() const { return *f.player; }
		std::vector<std::vector<uint8_t>> sent() const { return (*client)->sentBytes(); }
		void clearSent() const { (*client)->clearSent(); }
		std::vector<uint8_t> serializedFor(AionServerPacket&& packet) const { return serialized(std::move(packet), client->con()); }
	};

	void SetUp() override {
		ItemPacketTest::SetUp();
		savedGenerator = std::make_unique<commons::utils::Rnd::Xoshiro256PlusPlus>(commons::utils::Rnd::generator());
		savedMissingAiHandlers = configs::main::AIConfig::MISSING_AI_HANDLERS.get();
		configs::main::AIConfig::MISSING_AI_HANDLERS.set("warn"); // the npc rows name ai="general" / "aggressive", whose handlers are not linked
		// the base fixture's item rows and this fixture's, bound in one load context with the decomposables
		dataholders::DataManager::ITEM_DATA.resetForTests();
		xml::LoadContext context;
		dataholders::DataManager::ITEM_DATA.publish(xml::bindString<dataholders::ItemData>(context, economyItemTemplatesXml()));
		dataholders::DataManager::DECOMPOSABLE_ITEMS_DATA.publish(
			xml::bindString<dataholders::DecomposableItemsData>(context, ECONOMY_DECOMPOSABLE_ITEMS_XML));
		xml::LoadContext npcContext;
		dataholders::DataManager::NPC_DATA.publish(xml::bindString<dataholders::NpcData>(npcContext, ECONOMY_NPC_TEMPLATES_XML));
		dataholders::DataManager::TRADE_LIST_DATA.publish(xml::bindString<dataholders::TradeListData>(npcContext, ECONOMY_NPC_TRADE_LIST_XML));
		dataholders::DataManager::GOODSLIST_DATA.publish(xml::bindString<dataholders::GoodsListData>(npcContext, ECONOMY_GOODSLISTS_XML));
	}

	void TearDown() override {
		// no exchange or store outlives its players: the ExchangeService singleton holds Refs to both sides
		services::ExchangeService& exchanges = services::ExchangeService::getInstance();
		if (f.player && exchanges.isPlayerInExchange(*f.player))
			exchanges.cancelExchange(*f.player);
		for (OtherPlayer& other : others) {
			if (exchanges.isPlayerInExchange(other.player()))
				exchanges.cancelExchange(other.player());
		}
		for (OtherPlayer& other : others)
			other.player().getResponseRequester().denyAll(); // a pending question holds its asker (the request handler's requester)
		if (f.player)
			f.player->getResponseRequester().denyAll();
		for (OtherPlayer& other : others) {
			other.player().setStore(nullptr);
			other.player().setTarget(nullptr);
			world::World::getInstance().removeObject(other.player());
			other.player().setClientConnection(nullptr);
			other.client.reset();
		}
		others.clear();
		if (f.player) {
			f.player->setStore(nullptr);
			f.player->setTarget(nullptr);
			services::RepurchaseService::getInstance().removeRepurchaseItems(*f.player);
		}
		given.clear();
		npcs.clear(); // before the map instance their positions name
		spawnGroups.clear();
		ItemPacketTest::TearDown();
		dataholders::DataManager::GOODSLIST_DATA.resetForTests();
		dataholders::DataManager::TRADE_LIST_DATA.resetForTests();
		dataholders::DataManager::NPC_DATA.resetForTests();
		dataholders::DataManager::DECOMPOSABLE_ITEMS_DATA.resetForTests();
		if (savedMissingAiHandlers)
			configs::main::AIConfig::MISSING_AI_HANDLERS.set(*savedMissingAiHandlers);
		commons::utils::Rnd::generator() = *savedGenerator;
	}

	/** The npc of the row spawned `x` on the x axis at (x, 100, 50) and known to the player */
	model::gameobjects::Npc& npcAt(int32_t npcId, float x) {
		runtime::Ref<model::templates::spawns::SpawnGroup> group = model::templates::spawns::SpawnGroup::create(210010000, npcId, 0, nullptr);
		model::templates::spawns::SpawnTemplate& spawn = group->addSpawnTemplate(std::make_unique<EconomySpawnTemplate>(*group, x, 100.0f, 50.0f));
		runtime::Ref<model::gameobjects::Npc> npc = model::gameobjects::VisibleObject::create<model::gameobjects::Npc>(
			std::make_unique<controllers::NpcController>(), spawn, economyNpcTemplates().getNpcTemplate(npcId));
		npc->setKnownlist(std::make_unique<world::knownlist::NpcKnownList>(*npc));
		npc->setEffectController(std::make_unique<controllers::effect::EffectController>(*npc));
		npc->setPosition(world::WorldPosition::create(210010000, x, 100.0f, 50.0f, int8_t{0}, mapInstance->getRegion(x, 100.0f, 50.0f)));
		npc->getPosition()->setIsSpawned(true);
		f.knownList().addForTest(*npc);
		spawnGroups.push_back(group);
		npcs.push_back(npc);
		clearSent(); // what the player's see notification sent
		return *npc;
	}

	/**
	 * A second player at (x, 100, 50) in the fixture's map instance with his own connection, stored in the World. Not spawned (World.removeObject
	 * then skips the despawn); `knownByHolder` puts him into the holder's known list (a packet that looks its target up there finds him).
	 */
	OtherPlayer& otherPlayer(int32_t objectId, std::string_view name, model::Race race = model::Race::ELYOS, float x = 102.0f,
		bool knownByHolder = false) {
		OtherPlayer& other = others.emplace_back();
		other.f = makePlayer(objectId, objectId + 1000, name, race);
		other.f.player->setPosition(world::WorldPosition::create(210010000, x, 100.0f, 50.0f, int8_t{0}, mapInstance->getRegion(x, 100.0f, 50.0f)));
		other.client = std::make_unique<TestClient>();
		other.client->enterWorld(other.f);
		world::World::getInstance().storeObject(*other.f.player);
		if (knownByHolder)
			f.knownList().addForTest(*other.f.player);
		clearSent();
		return other;
	}

	/** An item loaded into a player's storage the way the DAO does (onLoadHandler: no packet) */
	Item& giveTo(model::gameobjects::player::Player& owner, int32_t objId, int32_t itemId, int64_t count, StorageType location = StorageType::CUBE) {
		runtime::Ref<Item> item = loadedItem(objId, itemId, count, location);
		owner.getStorage(model::items::storage::getId(location))->onLoadHandler(*item);
		given.push_back(item);
		return *item;
	}

	int64_t kinahOf(model::gameobjects::player::Player& owner) { return owner.getInventory().getKinah(); }

	int64_t countOf(model::gameobjects::player::Player& owner, int32_t itemId) { return owner.getInventory().getItemCountByItemId(itemId); }

	/** Reads `body` on the player's connection (the packet stays for the TestAccess friends) */
	template <class P>
	std::unique_ptr<EconomyDriver<P>> readPacket(int32_t opcode, const std::vector<uint8_t>& body) {
		auto packet = std::make_unique<EconomyDriver<P>>(opcode);
		EXPECT_TRUE(packet->readOn(body, client->get()));
		return packet;
	}

	/** Reads and runs `body` on `connection` (the player's by default), as AionConnection::processData and the PacketProcessor do */
	template <class P>
	void readAndRun(int32_t opcode, const std::vector<uint8_t>& body, const std::shared_ptr<AionConnection>& connection = nullptr) {
		EconomyDriver<P> packet(opcode);
		ASSERT_TRUE(packet.readOn(body, connection ? connection : std::shared_ptr<AionConnection>(client->get())));
		packet.runNow();
	}

	/** An item row as the inventory DAO loads it, identified or not (Item.isIdentified: tune count != -1) */
	Item& storedTuned(int32_t objId, int32_t itemId, int32_t tuneCount) {
		runtime::Ref<Item> item =
			Item::create(objId, itemId, 1, std::nullopt, 0, "", 0, 0, false, false, 0, model::items::storage::getId(StorageType::CUBE), 0, 0, 0, 0, 0,
				0, 0, tuneCount, 0, 0, 0, 0, false, 0, 0);
		storage(StorageType::CUBE).onLoadHandler(*item);
		items.push_back(item);
		return *item;
	}

	std::unique_ptr<commons::utils::Rnd::Xoshiro256PlusPlus> savedGenerator;
	std::shared_ptr<const std::string> savedMissingAiHandlers;
	std::vector<runtime::Ref<model::templates::spawns::SpawnGroup>> spawnGroups;
	std::vector<runtime::Ref<model::gameobjects::Npc>> npcs;
	std::vector<runtime::Ref<Item>> given;
	std::deque<OtherPlayer> others;
	// the shipped prices.properties and custom.properties values the trade services read, the gate profile's sell limits off, the audit log on
	// (LoggingConfig.java:10-11)
	EconomyConfigScope<int32_t> defaultPrices{configs::main::PricesConfig::DEFAULT_PRICES, 100};
	EconomyConfigScope<int32_t> defaultModifier{configs::main::PricesConfig::DEFAULT_MODIFIER, 100};
	EconomyConfigScope<int32_t> defaultTaxes{configs::main::PricesConfig::DEFAULT_TAXES, 100};
	EconomyConfigScope<int32_t> vendorBuyModifier{configs::main::PricesConfig::VENDOR_BUY_MODIFIER, 100};
	EconomyConfigScope<int32_t> vendorSellModifier{configs::main::PricesConfig::VENDOR_SELL_MODIFIER, 20};
	EconomyConfigScope<bool> limits{configs::main::CustomConfig::LIMITS_ENABLED, false};
	EconomyConfigScope<bool> logAudit{configs::main::LoggingConfig::LOG_AUDIT, true};
};

} // namespace aion::gameserver::network::aion::clientpackets::testing::items
