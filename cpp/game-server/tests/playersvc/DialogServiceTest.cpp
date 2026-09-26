// M5c D-02 / D-06 (m5c-plan.md §5, P5-08): DialogService, the npc dialog service every talkable npc reaches once CM_SHOW_DIALOG and
// CM_DIALOG_SELECT are ported (m5c-plan.md §2.1, W-01).
//
// Java: DialogService.java:53-377. The cases drive the service directly against the item packet fixture (tests/cm_ak/ItemPacketTestSupport.h:
// a spawned level-1 warrior in Poeta and a real AionConnection whose send queue the cases read), with npcs of real rows spawned beside him:
// - isInteractionAllowed: the summon-owner arms (Java isSummonOwner) and one row per SubDialogType the data carries and the fixture can build
//   (isSubDialogRestricted), each on both sides of its decision where the data allows it. ABYSSRANKING is not driven: AbyssRankingCache's
//   constructor loads the ranking from the database. SKILL_ID's and LEVEL's rows are the only ones of their type and carry an <equipment>
//   list, whose item ids stay unresolved IDREFs here (no item holder is bound with them, no resolveIdRefs runs): nothing on this path reads it.
// - DialogPage.getStartPageId's four answers through isInteractionAllowed (DialogPageInfo.cpp:17-29).
// - onDialogSelect: BUY -> SM_TRADELIST (the vendor modifier times a row's sell_price_rate / 100 in Java int arithmetic) and its two
//   refusals, SELL / TRADE_SELL_LIST -> SM_SELL_ITEM (the packets whose constructors reached the unported Npc.canSell / canPurchase before
//   D-01, W-03), TRADE_IN -> SM_TRADE_IN_LIST and its refusal, the page arm and its function check, the Poeta teleporter's non-Daeva refusal,
//   MATCH_MAKER with autogroup off, FACTION_JOIN / FACTION_SEPARATE through NpcFactions, the character edit and pet windows, the quest /
//   next-page fallback, RECOVERY (soul healing) with its price arithmetic, its question handler and the Soul Sickness (8291, SPEC2) it
//   removes, and every arm that reaches a body of another item or milestone, each asserted as the UnportedException of that function (W-08,
//   W-09, W-29, W-31, P5-07, P5-09's craft bodies, P5-11's legion bodies). Not driven: HOUSING_RECREATE_PERSONAL_INS, whose
//   HousingService singleton loads the houses from the database when it is first asked (HousingService.cpp, P5-11's test database fixture).
// - onCloseDialog: the mailbox, the null target, and a legion-warehouse npc closed by a player without a legion.
// Expected packets are Java's bytes where the fields are the packet's own choice; packets with a localized message or a question parameter
// are compared against the server's own serialization of the packet Java constructs there (their bytes are pinned by tests/sm_ak, sm_lz).
// Every npc row is npc_templates.xml's, every trade row npc_trade_list.xml's, every goods row goodslists.xml's, the faction row
// npc_factions.xml's and the skill row skill_templates.xml's, verbatim (file:line beside each); the experience table is
// player_experience_table.xml:3-68 without its comments.

#include "../cm_ak/ItemPacketTestSupport.h"
#include "../world/WorldTestSupport.h"

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "aion/gameserver/ai/AIState.h"
#include "aion/gameserver/configs/main/AIConfig.h"
#include "aion/gameserver/configs/main/AutoGroupConfig.h"
#include "aion/gameserver/configs/main/PricesConfig.h"
#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/controllers/effect/EffectController.h"
#include "aion/gameserver/dataholders/GoodsListData.bind.h"
#include "aion/gameserver/dataholders/GoodsListData.h"
#include "aion/gameserver/dataholders/NpcData.bind.h"
#include "aion/gameserver/dataholders/NpcData.h"
#include "aion/gameserver/dataholders/NpcFactionsData.bind.h"
#include "aion/gameserver/dataholders/NpcFactionsData.h"
#include "aion/gameserver/dataholders/PlayerExperienceTable.bind.h"
#include "aion/gameserver/dataholders/PlayerExperienceTable.h"
#include "aion/gameserver/dataholders/TradeListData.bind.h"
#include "aion/gameserver/dataholders/TradeListData.h"
#include "aion/gameserver/model/DialogAction.h"
#include "aion/gameserver/model/DialogPageInfo.h"
#include "aion/gameserver/model/gameobjects/Npc.h"
#include "aion/gameserver/model/gameobjects/player/AbyssRank.h"
#include "aion/gameserver/model/gameobjects/player/Mailbox.h"
#include "aion/gameserver/model/gameobjects/player/PlayerCommonData.h"
#include "aion/gameserver/model/gameobjects/player/QuestStateList.h"
#include "aion/gameserver/model/gameobjects/player/ResponseRequester.h"
#include "aion/gameserver/model/gameobjects/player/npcFaction/NpcFactions.h"
#include "aion/gameserver/model/skill/PlayerSkillEntry.h"
#include "aion/gameserver/model/skill/PlayerSkillList.h"
#include "aion/gameserver/model/templates/npc/NpcTemplate.h"
#include "aion/gameserver/model/templates/spawns/SpawnGroup.h"
#include "aion/gameserver/model/templates/spawns/SpawnTemplate.h"
#include "aion/gameserver/network/aion/serverpackets/SM_QUESTION_WINDOW.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/services/DialogService.h"
#include "aion/gameserver/services/player/PlayerMailboxState.h"
#include "aion/gameserver/skillengine/SkillEngine.h"
#include "aion/gameserver/skillengine/effect/SummonOwner.h"
#include "aion/gameserver/skillengine/model/Effect.h"
#include "aion/gameserver/utils/stats/AbyssRankEnum.h"
#include "aion/gameserver/world/knownlist/NpcKnownList.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets::testing::items {
namespace {

using model::gameobjects::Creature;
using model::gameobjects::Npc;
using network::test::LogCapture;
using serverpackets::SM_QUESTION_WINDOW;
using serverpackets::SM_SYSTEM_MESSAGE;
using services::DialogService;
using services::player::PlayerMailboxState;
using skillengine::effect::SummonOwner;
namespace DialogAction = model::DialogAction;

// ServerPacketsOpcodes.java:78, :80, :83 (via :101), :119, :169, :185, :271
constexpr int32_t SM_DIALOG_WINDOW_OPCODE = 60;
constexpr int32_t SM_SELL_ITEM_OPCODE = 62;
constexpr int32_t SM_PLASTIC_SURGERY_OPCODE = 83;
constexpr int32_t SM_PET_OPCODE = 101;
constexpr int32_t SM_TRADE_IN_LIST_OPCODE = 151;
constexpr int32_t SM_REPURCHASE_OPCODE = 167;
constexpr int32_t SM_TRADELIST_OPCODE = 253;

// the npc ids of the rows below
constexpr int32_t MAILBOX = 700000;
constexpr int32_t MINALINERK = 798007;
constexpr int32_t MOGIRONERK = 798088;
constexpr int32_t GWENSPENA = 203724;
constexpr int32_t AMARUNERK = 279058;
constexpr int32_t PERNOS = 790001;
constexpr int32_t PALAEMON = 203105;
constexpr int32_t SERIL = 203336;
constexpr int32_t DAINES = 203194;
constexpr int32_t GUARDIAN_BATTERY = 251725;
constexpr int32_t HALLIWELL = 831405;
constexpr int32_t DEFENSE_PORTAL = 804677;
constexpr int32_t COMMANDERS_CORRIDOR = 730940;
constexpr int32_t STONESPEAR_ENTRANCE = 833024;
constexpr int32_t PUCORINERK = 805011;
constexpr int32_t MELANIE = 832012;
constexpr int32_t LUSHRUNERK = 833547;
constexpr int32_t LALRINERK = 833548;
constexpr int32_t PORIRUNERK = 802162;
constexpr int32_t INCOMPLETE_SHUGOROBO = 833545;
constexpr int32_t SHUGOROBO = 833543;
constexpr int32_t BLESSED_TOTEM = 831987;
constexpr int32_t NORNES = 203619;
constexpr int32_t TYPHON = 799803;
constexpr int32_t PERBANO = 205315;
constexpr int32_t EPEIOS = 203764;
constexpr int32_t NEPIS = 203875;
constexpr int32_t PAUTON = 203751;
constexpr int32_t NEW_USER_MELANIE = 831406;
constexpr int32_t BUNTON = 831408;

constexpr int32_t ABBEY_RETURN_STONE = 164000335;
constexpr int32_t SOUL_SICKNESS = 8291;

/** npc_templates.xml, verbatim rows (the lines of each row in the comment above it) */
constexpr std::string_view NPC_TEMPLATES_XML = R"xml(<npc_templates>
	<!-- :439517-439521 -->
	<npc_template npc_id="700000" level="1" name="mailbox" name_id="350707" height="2" group_drop="NONE" rank="DISCIPLINED" rating="NORMAL" tribe="FIELD_OBJECT_LIGHT" type="GENERAL" ai="postbox" sangle="0" attack_speed="2000" hpgauge="3">
		<stats maxHp="172" />
		<bound_radius front="0.25" side="0.35" upper="2" />
		<talk_info distance="5" can_talk_invisible="false" />
	</npc_template>
	<!-- :461604-461610 -->
	<npc_template npc_id="798007" level="9" name="minalinerk" name_id="351126" height="1.16875" title_id="350377" group_drop="NONE" rank="DISCIPLINED" rating="NORMAL" race="BROWNIE" tribe="GENERAL" type="GENERAL" ai="general" srange="20" sangle="240" attack_speed="2100" hpgauge="3">
		<stats maxHp="2568">
			<speeds walk="1.5" group_walk="1.5" run="4.23" run_fight="4.23" group_run_fight="4.23" />
		</stats>
		<bound_radius front="0.595" side="0.3774" upper="1.16875" />
		<talk_info distance="5" is_dialog="true" func_dialogs="2 3" can_talk_invisible="false" />
	</npc_template>
	<!-- :462170-462176 -->
	<npc_template npc_id="798088" level="1" name="mogironerk" name_id="353292" height="1.16875" title_id="350530" group_drop="NONE" rank="DISCIPLINED" rating="NORMAL" race="BROWNIE" tribe="GENERAL_DARK" type="GENERAL" ai="general" srange="20" sangle="240" attack_speed="2000" hpgauge="3">
		<stats maxHp="14535">
			<speeds walk="1.5" group_walk="1.5" run="4.23" run_fight="4.23" group_run_fight="4.23" />
		</stats>
		<bound_radius front="0.595" side="0.3774" upper="1.16875" />
		<talk_info distance="5" is_dialog="true" func_dialogs="2 3" can_talk_invisible="false" />
	</npc_template>
	<!-- :9573-9586 -->
	<npc_template npc_id="203724" level="40" name="gwenspena" name_id="351405" height="2" title_id="350655" group_drop="LIGHT" rank="DISCIPLINED" rating="NORMAL" race="ELYOS" tribe="GENERAL" type="GENERAL" ai="general" srange="10" sangle="240" arange="2" attack_speed="2000" hpgauge="3">
		<stats maxHp="9426">
			<speeds walk="1.68" group_walk="1.68" run="6" run_fight="4.2" group_run_fight="4.2" />
		</stats>
		<equipment>
			<item>113100270</item>
			<item>110100318</item>
			<item>112100250</item>
			<item>111100271</item>
			<item>114100288</item>
		</equipment>
		<bound_radius front="0.25" side="0.35" upper="2" />
		<talk_info distance="5" is_dialog="true" func_dialogs="2 3" can_talk_invisible="false" />
	</npc_template>
	<!-- :341797-341803 -->
	<npc_template npc_id="279058" level="40" name="amarunerk" name_id="314047" height="1.16875" title_id="314359" group_drop="NONE" rank="DISCIPLINED" rating="NORMAL" race="BROWNIE" tribe="USEALL" type="GENERAL" ai="general" srange="20" sangle="300" attack_speed="2000" hpgauge="3">
		<stats maxHp="9419">
			<speeds walk="1.5" group_walk="1.5" run="4.23" run_fight="4.23" group_run_fight="4.23" />
		</stats>
		<bound_radius front="0.595" side="0.3774" upper="1.16875" />
		<talk_info distance="5" is_dialog="true" func_dialogs="103" can_talk_invisible="false" />
	</npc_template>
	<!-- :461402-461408 -->
	<npc_template npc_id="790001" level="50" name="pernos" name_id="351014" height="1.8" title_id="350436" group_drop="NONE" rank="DISCIPLINED" rating="NORMAL" race="ELYOS" tribe="GENERAL" type="GENERAL" ai="general" srange="20" sangle="240" attack_speed="2000" hpgauge="3">
		<stats maxHp="14614">
			<speeds walk="1.5" group_walk="1.5" run="6" run_fight="4.2" group_run_fight="4.2" />
		</stats>
		<bound_radius front="0.225" side="0.315" upper="1.8" />
		<talk_info distance="5" is_dialog="true" can_talk_invisible="false" />
	</npc_template>
	<!-- :2642-2648 -->
	<npc_template npc_id="203105" level="45" name="palaemon" name_id="351030" height="1.8" title_id="350412" group_drop="NONE" rank="VETERAN" rating="ELITE" race="ELYOS" tribe="GENERAL" type="GENERAL" ai="general" srange="20" sangle="240" arange="2" attack_speed="2000" hpgauge="14" cancel_level="20">
		<stats maxHp="108866">
			<speeds walk="2.1" group_walk="2.1" run="6" run_fight="4.2" group_run_fight="4.2" />
		</stats>
		<bound_radius front="0.25" side="0.35" upper="1.8" />
		<talk_info distance="5" is_dialog="true" func_dialogs="35" can_talk_invisible="false" />
	</npc_template>
	<!-- :5845-5858 -->
	<npc_template npc_id="203336" level="20" name="seril" name_id="351162" height="2" title_id="350414" group_drop="LIGHT" rank="DISCIPLINED" rating="NORMAL" race="ELYOS" tribe="GENERAL" type="GENERAL" ai="general" srange="10" sangle="300" arange="2" attack_speed="2000" hpgauge="3">
		<stats maxHp="2961">
			<speeds walk="1.5" group_walk="1.5" run="6" run_fight="4.2" group_run_fight="4.2" />
		</stats>
		<equipment>
			<item>113100270</item>
			<item>110100320</item>
			<item>112100250</item>
			<item>111100271</item>
			<item>114100288</item>
		</equipment>
		<bound_radius front="0.25" side="0.35" upper="2" />
		<talk_info distance="5" is_dialog="true" func_dialogs="42" can_talk_invisible="false" />
	</npc_template>
	<!-- :3864-3870 -->
	<npc_template npc_id="203194" level="40" name="daines" name_id="351402" height="1.8" title_id="350423" group_drop="NONE" rank="DISCIPLINED" rating="NORMAL" race="ELYOS" tribe="GENERAL" type="GENERAL" ai="general" srange="10" sangle="300" arange="2" attack_speed="2000" hpgauge="3">
		<stats maxHp="9426">
			<speeds walk="1.5" group_walk="1.5" run="6" run_fight="4.2" group_run_fight="4.2" />
		</stats>
		<bound_radius front="0.25" side="0.35" upper="1.8" />
		<talk_info distance="3" is_dialog="true" func_dialogs="44" can_talk_invisible="false" />
	</npc_template>
	<!-- :208736-208740 -->
	<npc_template npc_id="251725" level="65" name="guardian battery" name_id="346362" height="2" group_drop="SIEGEWEAPON" rank="EXPERT" rating="ELITE" race="ELYOS" tribe="SIEGEWEAPON_PC" type="GENERAL" ai="siege_cannon" srange="15" sangle="0" attack_speed="2180" hpgauge="13" cancel_level="20">
		<stats maxHp="346251" msup="621" />
		<bound_radius front="0.25" side="0.35" upper="2" />
		<talk_info distance="2" delay="3" is_dialog="true" subdialog_type="FORT_CAPTURE" can_talk_invisible="false" />
	</npc_template>
	<!-- :547375-547389 -->
	<npc_template npc_id="831405" level="1" name="halliwell (for testing skill exclusive npcs)" name_id="356470" height="2" group_drop="LIGHT" rank="DISCIPLINED" rating="NORMAL" race="ELYOS" tribe="GENERAL" type="GENERAL" ai="general" srange="20" sangle="300" arange="2" attack_speed="2000" hpgauge="3">
		<stats maxHp="23691">
			<speeds walk="1.5" group_walk="1.5" run="6" run_fight="4.2" group_run_fight="4.2" />
		</stats>
		<equipment>
			<item>113100283</item>
			<item>110100345</item>
			<item>100500017</item>
			<item>112100261</item>
			<item>111100284</item>
			<item>114100301</item>
		</equipment>
		<bound_radius front="0.25" side="0.35" upper="2" />
		<talk_info distance="5" subdialog_type="SKILL_ID" subdialog_value="1377" can_talk_invisible="false" />
	</npc_template>
	<!-- :522556-522560 -->
	<npc_template npc_id="804677" level="1" name="atreia defense portal" name_id="360123" height="2" title_id="373285" group_drop="NONE" rank="DISCIPLINED" rating="NORMAL" tribe="FIELD_OBJECT_ALL" type="GENERAL" ai="portal_dialog" srange="20" attack_speed="2000" hpgauge="3">
		<stats maxHp="118" />
		<bound_radius front="0.25" side="0.35" upper="2" />
		<talk_info distance="10" delay="3" is_dialog="true" subdialog_type="RETURN" can_talk_invisible="false" />
	</npc_template>
	<!-- :456836-456840 -->
	<npc_template npc_id="730940" level="65" name="advance corridor for commanders" name_id="464309" height="2" title_id="464313" group_drop="NONE" rank="DISCIPLINED" rating="NORMAL" tribe="GUARD" type="GENERAL" ai="governor_advance_corridor" srange="20" arange="2" attack_speed="2000" hpgauge="3">
		<stats maxHp="26116" />
		<bound_radius front="0.25" side="0.35" upper="2" />
		<talk_info distance="5" is_dialog="true" subdialog_type="ABYSSRANK" subdialog_value="18" can_talk_invisible="false" />
	</npc_template>
	<!-- :559636-559640 -->
	<npc_template npc_id="833024" level="1" name="stonespear siege entrance" name_id="466238" height="2" title_id="466401" group_drop="NONE" rank="DISCIPLINED" rating="NORMAL" race="ELYOS" tribe="GENERAL" type="GENERAL" ai="legion_dominion_portal" srange="20" arange="2" attack_speed="2000" hpgauge="3">
		<stats maxHp="172" />
		<bound_radius front="0.25" side="0.35" upper="2" />
		<talk_info distance="5" is_dialog="true" subdialog_type="TARGET_LEGION_DOMINION" subdialog_value="4" can_talk_invisible="false" />
	</npc_template>
	<!-- :526402-526408 -->
	<npc_template npc_id="805011" level="65" name="pucorinerk" name_id="357902" height="1.375" title_id="358083" group_drop="NONE" rank="DISCIPLINED" rating="NORMAL" race="BROWNIE" tribe="GENERAL_DARK" type="GENERAL" ai="general" srange="20" arange="2" attack_speed="2000" hpgauge="3">
		<stats maxHp="26116">
			<speeds walk="1.5" group_walk="1.5" run="4.23" run_fight="4.23" group_run_fight="4.23" />
		</stats>
		<bound_radius front="0.7" side="0.444" upper="1.375" />
		<talk_info distance="5" is_dialog="true" func_dialogs="103" subdialog_type="LEGION_DOMINION_NPC" subdialog_value="3" can_talk_invisible="false" />
	</npc_template>
	<!-- :552222-552236 -->
	<npc_template npc_id="832012" level="1" name="melanie (to test npcs for lv 40)" name_id="464225" height="2" group_drop="LIGHT" rank="DISCIPLINED" rating="NORMAL" race="ELYOS" tribe="GENERAL" type="GENERAL" ai="general" srange="20" arange="2" attack_speed="2000" hpgauge="3">
		<stats maxHp="142">
			<speeds walk="1.5" group_walk="1.5" run="6" run_fight="4.2" group_run_fight="4.2" />
		</stats>
		<equipment>
			<item>113100276</item>
			<item>110100298</item>
			<item>112100256</item>
			<item>111100277</item>
			<item>114100294</item>
			<item>125001508</item>
		</equipment>
		<bound_radius front="0.25" side="0.35" upper="2" />
		<talk_info distance="5" subdialog_type="LEVEL" subdialog_value="40" can_talk_invisible="false" />
	</npc_template>
	<!-- :562450-562456 -->
	<npc_template npc_id="833547" level="1" name="lushrunerk" name_id="466576" height="1.496" title_id="466578" group_drop="NONE" rank="DISCIPLINED" rating="NORMAL" tribe="USEALL" type="GENERAL" ai="aggressive" srange="20" attack_speed="2000" hpgauge="3">
		<stats maxHp="126">
			<speeds walk="1.5" group_walk="1.5" run="4.23" run_fight="4.23" group_run_fight="4.23" />
		</stats>
		<bound_radius front="0.8" side="0.32" upper="1.496" />
		<talk_info distance="5" is_dialog="true" func_dialogs="2 3" subdialog_type="LEVEL_LOW" subdialog_value="54" can_talk_invisible="false" />
	</npc_template>
	<!-- :562457-562463 -->
	<npc_template npc_id="833548" level="1" name="lalrinerk" name_id="466577" height="1.375" title_id="466578" group_drop="NONE" rank="DISCIPLINED" rating="NORMAL" tribe="USEALL" type="GENERAL" ai="aggressive" srange="20" attack_speed="2000" hpgauge="3">
		<stats maxHp="124">
			<speeds walk="1.5" group_walk="1.5" run="4.23" run_fight="4.23" group_run_fight="4.23" />
		</stats>
		<bound_radius front="0.7" side="0.444" upper="1.375" />
		<talk_info distance="5" is_dialog="true" func_dialogs="2 3" subdialog_type="LEVEL_HIGH" subdialog_value="55" can_talk_invisible="false" />
	</npc_template>
	<!-- :503906-503912 -->
	<npc_template npc_id="802162" level="1" name="porirunerk" name_id="464129" height="1.5125" title_id="464169" group_drop="NONE" rank="DISCIPLINED" rating="NORMAL" race="BROWNIE" tribe="USEALL" type="GENERAL" ai="general" srange="10" arange="2" attack_speed="2000" hpgauge="3">
		<stats maxHp="26116">
			<speeds walk="1.5" group_walk="1.5" run="4.23" run_fight="4.23" group_run_fight="4.23" />
		</stats>
		<bound_radius front="0.77" side="0.4884" upper="1.5125" />
		<talk_info distance="5" is_dialog="true" subdialog_type="PCBANG" can_talk_invisible="false" />
	</npc_template>
	<!-- :562436-562442 -->
	<npc_template npc_id="833545" level="1" name="incomplete shugorobo" name_id="466560" height="1.7875" title_id="466562" group_drop="NONE" rank="DISCIPLINED" rating="NORMAL" tribe="USEALL" type="GENERAL" ai="aggressive" srange="20" attack_speed="2000" hpgauge="3">
		<stats maxHp="143">
			<speeds walk="1.5" group_walk="1.5" run="4.23" run_fight="4.23" group_run_fight="4.23" />
		</stats>
		<bound_radius front="0.91" side="0.5772" upper="1.7875" />
		<talk_info distance="5" is_dialog="true" subdialog_type="PACK_3" can_talk_invisible="false" />
	</npc_template>
	<!-- :562422-562428 -->
	<npc_template npc_id="833543" level="1" name="shugorobo" name_id="466558" height="1.7875" title_id="466562" group_drop="NONE" rank="DISCIPLINED" rating="NORMAL" tribe="USEALL" type="GENERAL" ai="aggressive" srange="20" attack_speed="2000" hpgauge="3">
		<stats maxHp="106">
			<speeds walk="1.5" group_walk="1.5" run="4.23" run_fight="4.23" group_run_fight="4.23" />
		</stats>
		<bound_radius front="0.91" side="0.5772" upper="1.7875" />
		<talk_info distance="5" is_dialog="true" subdialog_type="PACK_4" can_talk_invisible="false" />
	</npc_template>
	<!-- :552002-552006 -->
	<npc_template npc_id="831987" level="1" name="blessed totem" name_id="464179" height="2" title_id="464183" group_drop="NONE" rank="DISCIPLINED" rating="NORMAL" tribe="USEALL" type="GENERAL" ai="general" sangle="0" attack_speed="2000" hpgauge="3">
		<stats maxHp="26116" />
		<bound_radius front="0.25" side="0.35" upper="2" />
		<talk_info distance="5" is_dialog="true" subdialog_type="CASH" can_talk_invisible="false" />
	</npc_template>
	<!-- :8150-8164 -->
	<npc_template npc_id="203619" level="10" name="nornes" name_id="352165" height="2" title_id="350552" group_drop="DARK" rank="DISCIPLINED" rating="NORMAL" race="ASMODIANS" tribe="GENERAL_DARK" type="GENERAL" ai="general" srange="20" sangle="240" arange="10" attack_speed="2000" hpgauge="3">
		<stats maxHp="1392">
			<speeds walk="1.5" group_walk="1.5" run="6" run_fight="4.2" group_run_fight="4.2" />
		</stats>
		<equipment>
			<item>113100276</item>
			<item>110100311</item>
			<item>100000521</item>
			<item>112100256</item>
			<item>111100277</item>
			<item>114100294</item>
		</equipment>
		<bound_radius front="0.25" side="0.35" upper="2" />
		<talk_info distance="5" is_dialog="true" func_dialogs="2 3" can_talk_invisible="false" />
	</npc_template>
	<!-- :475658-475671 -->
	<npc_template npc_id="799803" level="50" name="typhon" name_id="355117" height="1.8" title_id="370284" group_drop="LIGHT" rank="DISCIPLINED" rating="NORMAL" race="ELYOS" tribe="GENERAL" type="GENERAL" ai="general" srange="20" sangle="300" arange="2" attack_speed="2000" hpgauge="3">
		<stats maxHp="14614">
			<speeds walk="1.5" group_walk="1.5" run="6" run_fight="4.2" group_run_fight="4.2" />
		</stats>
		<equipment>
			<item>113501125</item>
			<item>110501148</item>
			<item>111501113</item>
			<item>114501129</item>
			<item>125002466</item>
		</equipment>
		<bound_radius front="0.25" side="0.35" upper="1.8" />
		<talk_info distance="5" is_dialog="true" func_dialogs="68 69" can_talk_invisible="false" />
	</npc_template>
	<!-- :26411-26424 -->
	<npc_template npc_id="205315" level="55" name="perbano" name_id="354625" height="2.1" title_id="370380" group_drop="LIGHT" rank="DISCIPLINED" rating="NORMAL" race="ELYOS" tribe="GENERAL" type="GENERAL" ai="general" srange="20" sangle="300" arange="2" attack_speed="2000" hpgauge="3">
		<stats maxHp="18756">
			<speeds walk="1.5" group_walk="1.5" run="6" run_fight="4.2" group_run_fight="4.2" />
		</stats>
		<equipment>
			<item>113301154</item>
			<item>110301186</item>
			<item>112301075</item>
			<item>111301131</item>
			<item>114301187</item>
		</equipment>
		<bound_radius front="0.2625" side="0.3675" upper="2.1" />
		<talk_info distance="5" is_dialog="true" func_dialogs="78" can_talk_invisible="false" />
	</npc_template>
	<!-- :10127-10140 -->
	<npc_template npc_id="203764" level="40" name="epeios" name_id="351251" height="2" title_id="350422" group_drop="LIGHT" rank="DISCIPLINED" rating="NORMAL" race="ELYOS" tribe="GENERAL" type="GENERAL" ai="general" srange="10" sangle="300" arange="2" attack_speed="2000" hpgauge="3" state="6">
		<stats maxHp="9426">
			<speeds walk="1.5" group_walk="1.5" run="6" run_fight="4.2" group_run_fight="4.2" />
		</stats>
		<equipment>
			<item>113500307</item>
			<item>110500324</item>
			<item>112500299</item>
			<item>111500315</item>
			<item>114500319</item>
		</equipment>
		<bound_radius front="0.25" side="0.35" upper="2" />
		<talk_info distance="5" is_dialog="true" func_dialogs="36" can_talk_invisible="false" />
	</npc_template>
	<!-- :11745-11758 -->
	<npc_template npc_id="203875" level="40" name="nepis" name_id="351365" height="2" title_id="350422" group_drop="LIGHT" rank="DISCIPLINED" rating="NORMAL" race="ELYOS" tribe="GENERAL" type="GENERAL" ai="general" srange="10" sangle="300" attack_speed="2000" hpgauge="3" state="6">
		<stats maxHp="9426">
			<speeds walk="1.5" group_walk="1.5" run="6" run_fight="4.2" group_run_fight="4.2" />
		</stats>
		<equipment>
			<item>113500307</item>
			<item>110500324</item>
			<item>112500299</item>
			<item>111500315</item>
			<item>114500319</item>
		</equipment>
		<bound_radius front="0.25" side="0.35" upper="2" />
		<talk_info distance="5" is_dialog="true" func_dialogs="37" can_talk_invisible="false" />
	</npc_template>
	<!-- :9939-9952 -->
	<npc_template npc_id="203751" level="40" name="pauton" name_id="351300" height="2" title_id="350409" group_drop="LIGHT" rank="DISCIPLINED" rating="NORMAL" race="ELYOS" tribe="GENERAL" type="GENERAL" ai="general" srange="10" sangle="300" arange="2" attack_speed="2000" hpgauge="3">
		<stats maxHp="9426">
			<speeds walk="1.5" group_walk="1.5" run="6" run_fight="4.2" group_run_fight="4.2" />
		</stats>
		<equipment>
			<item>113500303</item>
			<item>110500320</item>
			<item>112500295</item>
			<item>111500311</item>
			<item>114500315</item>
		</equipment>
		<bound_radius front="0.25" side="0.35" upper="2" />
		<talk_info distance="5" is_dialog="true" func_dialogs="53" can_talk_invisible="false" />
	</npc_template>
	<!-- :547390-547404 -->
	<npc_template npc_id="831406" level="1" name="melanie (for new user exclusive npc testing)" name_id="356471" height="2" group_drop="LIGHT" rank="DISCIPLINED" rating="NORMAL" race="ELYOS" tribe="GENERAL" type="GENERAL" ai="general" srange="20" sangle="300" arange="2" attack_speed="2000" hpgauge="3">
		<stats maxHp="23691">
			<speeds walk="1.5" group_walk="1.5" run="6" run_fight="4.2" group_run_fight="4.2" />
		</stats>
		<equipment>
			<item>113100276</item>
			<item>110100298</item>
			<item>112100256</item>
			<item>111100277</item>
			<item>114100294</item>
			<item>125001508</item>
		</equipment>
		<bound_radius front="0.25" side="0.35" upper="2" />
		<talk_info distance="5" subdialog_type="NEWBIE" can_talk_invisible="false" />
	</npc_template>
	<!-- :547420-547434 -->
	<npc_template npc_id="831408" level="1" name="bunton (for testing paid user exclusive npcs)" name_id="356473" height="2" group_drop="LIGHT" rank="DISCIPLINED" rating="NORMAL" race="ELYOS" tribe="GENERAL" type="GENERAL" ai="general" srange="20" sangle="300" arange="2" attack_speed="2000" hpgauge="3">
		<stats maxHp="23691">
			<speeds walk="1.5" group_walk="1.5" run="6" run_fight="4.2" group_run_fight="4.2" />
		</stats>
		<equipment>
			<item>113100276</item>
			<item>110100298</item>
			<item>112100256</item>
			<item>111100277</item>
			<item>114100294</item>
			<item>125001508</item>
		</equipment>
		<bound_radius front="0.25" side="0.35" upper="2" />
		<talk_info distance="5" subdialog_type="PAID_USER" can_talk_invisible="false" />
	</npc_template>
</npc_templates>)xml";

/** npc_trade_list.xml, verbatim rows */
constexpr std::string_view NPC_TRADE_LIST_XML = R"xml(<npc_trade_list>
	<!-- :412-415 -->
	<tradelist_template npc_id="203619" buy_price_rate="200" sell_price_rate="150">
		<tradelist id="277" />
		<tradelist id="404" />
	</tradelist_template>
	<!-- :442-447 -->
	<tradelist_template npc_id="203724" buy_price_rate="200">
		<tradelist id="357" />
		<tradelist id="358" />
		<tradelist id="1151" />
		<tradelist id="1152" />
	</tradelist_template>
	<!-- :2186-2189 -->
	<tradelist_template npc_id="798007" buy_price_rate="200">
		<tradelist id="132" />
		<tradelist id="720" />
	</tradelist_template>
	<!-- :9062-9064 -->
	<trade_in_list_template npc_id="205315">
		<tradelist id="39" />
	</trade_in_list_template>
	<!-- :9570-9572 -->
	<purchase_template npc_id="279058" buy_price_rate="150" npc_type="ABYSS">
		<tradelist id="59" />
	</purchase_template>
</npc_trade_list>)xml";

/** goodslists/goodslists.xml, verbatim rows */
constexpr std::string_view GOODSLISTS_XML = R"xml(<goodslists>
    <!-- :13484-13488 -->
    <list id="132">
        <item id="169000003"/>
        <item id="165000001"/>
        <item id="169300002"/>
    </list>
    <!-- :16922-16926 -->
    <list id="277">
        <item id="110300804"/>
        <item id="113300783"/>
        <item id="123000854"/>
    </list>
    <!-- :18804-18807 -->
    <list id="357" legion_lvl="2">
        <item id="115000254"/>
        <item id="188010002"/>
    </list>
    <!-- :18808-18818 -->
    <list id="358" legion_lvl="3">
        <item id="184000020"/>
        <item id="188010008"/>
        <item id="188010010"/>
        <item id="164000101"/>
        <item id="184000048"/>
        <item id="184000049"/>
        <item id="184000050"/>
        <item id="184000051"/>
        <item id="184000052"/>
    </list>
    <!-- :20179-20186 -->
    <list id="404">
        <item id="152205598"/>
        <item id="152205599"/>
        <item id="152205600"/>
        <item id="152205601"/>
        <item id="152205602"/>
        <item id="152205603"/>
    </list>
    <!-- :27984-27987 -->
    <list id="720">
        <item id="162000052"/>
        <item id="162000057"/>
    </list>
    <!-- :38105-38108 -->
    <list id="1151" legion_lvl="4">
        <item id="110101254"/>
        <item id="125100138"/>
    </list>
    <!-- :38109-38111 -->
    <list id="1152" legion_lvl="5">
        <item id="110101255"/>
    </list>
</goodslists>)xml";

/** item_templates.xml:834300-834305, verbatim: the item the RETURN subdialog asks for (DialogService.java:348) */
constexpr std::string_view ABBEY_RETURN_STONE_XML = R"xml(	<item_template id="164000335" name="Abbey Return Stone (30 days)" level="10" cName="scroll_return_arena_l_clobby_l1" casting_delay="10000" mask="4097" quality="RARE" price="5" race="ELYOS" restrict="10 10 10 10 10 10 10 10 10 10 10 10 10 10 10 10 10" desc="841820" return_world="130090000" return_alias="ARENA_L_CLOBBY_RETURN01" activate_target="STANDALONE" activate_count="1000" expire_time="43260">
		<actions>
			<skilluse level="1" skillid="8198"/>
		</actions>
		<uselimits usedelay="600000" usedelayid="58"/>
	</item_template>
)xml";

/** npc_factions/npc_factions.xml:4, verbatim: the faction whose registrar is typhon (799803) */
constexpr std::string_view NPC_FACTIONS_XML = R"xml(<npc_factions>
	<npc_faction id="2" name="Alabaster Order" npc_ids="799803 805145" name_id="1129000" category="DAILY" min_level="30" race="ELYOS"/>
</npc_factions>)xml";

/** skills/skill_templates.xml:80933-80954, verbatim: Soul Sickness, the SPEC2 debuff soul healing removes (DialogService.java:129, :144) */
constexpr std::string_view SOUL_SICKNESS_XML = R"xml(	<skill_template skill_id="8291" name="Soul Sickness" nameId="282709" stack="CH_RESURRECTDEBUFF" lvl="1" skilltype="MAGICAL" skillsubtype="NONE" tslot="SPEC2" activation="PROVOKED" cooldown="0" duration="0" apply_magical_skill_boost_bonus="true" apply_magical_critical="true">
		<properties first_target="TARGET" first_target_range="1" target_relation="FRIEND" target_type="ONLYONE" />
		<startconditions>
			<weapon weapon="GREATSWORD SPELLBOOK BOW DAGGER MACE ORB POLEARM STAFF SWORD GUN CANNON HARP KEYBLADE" />
		</startconditions>
		<useconditions>
			<move_casting allow="false" />
		</useconditions>
		<effects>
			<statdown duration2="40000" duration1="20000" e="1" noresist="true" element="FIRE">
				<change stat="MAXHP" func="PERCENT" value="-30" />
			</statdown>
			<statdown duration2="40000" duration1="20000" e="2" noresist="true" element="FIRE" preeffect="1">
				<change stat="MAXMP" func="PERCENT" value="-30" />
			</statdown>
			<statdown duration2="40000" duration1="20000" e="3" noresist="true" element="FIRE" preeffect="1">
				<change stat="SPEED" func="PERCENT" value="-50" />
				<change stat="FLY_SPEED" func="PERCENT" value="-50" />
			</statdown>
		</effects>
		<motion name="normalfire" />
	</skill_template>
)xml";

/** player_experience_table.xml:3-68, the <exp> values without their comments (the fixture's 16 levels end below the level rows' values) */
constexpr std::string_view PLAYER_EXPERIENCE_TABLE_FULL_XML =
	"<player_experience_table><exp>0</exp><exp>400</exp><exp>1433</exp><exp>3820</exp><exp>9054</exp><exp>17655</exp><exp>30978</exp>"
	"<exp>52010</exp><exp>82982</exp><exp>126069</exp><exp>182252</exp><exp>260622</exp><exp>360825</exp><exp>490331</exp><exp>649169</exp>"
	"<exp>844378</exp><exp>1083018</exp><exp>1401356</exp><exp>1808613</exp><exp>2314771</exp><exp>2941893</exp><exp>3769257</exp>"
	"<exp>4811154</exp><exp>6110198</exp><exp>7632340</exp><exp>9377726</exp><exp>11395643</exp><exp>13731725</exp><exp>16339413</exp>"
	"<exp>19378549</exp><exp>23162749</exp><exp>27585843</exp><exp>32841197</exp><exp>39127217</exp><exp>47350762</exp><exp>57829684</exp>"
	"<exp>70654362</exp><exp>87571065</exp><exp>107018757</exp><exp>129815732</exp><exp>157211282</exp><exp>189272188</exp><exp>226933751</exp>"
	"<exp>267247400</exp><exp>310053925</exp><exp>355815203</exp><exp>404823687</exp><exp>456685353</exp><exp>511683757</exp><exp>570162075</exp>"
	"<exp>632268545</exp><exp>701585822</exp><exp>776831823</exp><exp>857090855</exp><exp>947120930</exp><exp>1051346275</exp><exp>1175571620</exp>"
	"<exp>1318550121</exp><exp>1484090156</exp><exp>1674064804</exp><exp>1913274732</exp><exp>2162140395</exp><exp>2419819338</exp>"
	"<exp>2700930959</exp><exp>3209499233</exp><exp>3794060468</exp></player_experience_table>";

/** The npc rows, bound through NpcData as the server loads npc_templates.xml; kept for the process as DataManager keeps its own */
const dataholders::NpcData& npcData() {
	static const dataholders::NpcData* holder = [] {
		static xml::LoadContext context;
		return xml::bindString<dataholders::NpcData>(context, NPC_TEMPLATES_XML).release();
	}();
	return *holder;
}

class DialogSpawnTemplate final : public model::templates::spawns::SpawnTemplate {
public:
	DialogSpawnTemplate(model::templates::spawns::SpawnGroup& group, float x, float y, float z)
		: SpawnTemplate(group, x, y, z, int8_t{0}, 0, std::nullopt, 0, 0, std::nullopt) {}
};

/** The number of hits of the AION_UNPORTED sites in `file` since the last reset */
uint64_t unportedHitsIn(std::string_view file) {
	uint64_t hits = 0;
	for (const runtime::UnportedHit& hit : runtime::unportedHits()) {
		if (hit.file.find(file) != std::string::npos)
			hits += hit.hits;
	}
	return hits;
}

/** The number of hits of the AION_UNPORTED sites whose function (the compiler's spelling) names `function` since the last reset */
uint64_t unportedHitsOf(std::string_view function) {
	uint64_t hits = 0;
	for (const runtime::UnportedHit& hit : runtime::unportedHits()) {
		if (hit.function.find(function) != std::string::npos)
			hits += hit.hits;
	}
	return hits;
}

class DialogServiceTest : public ItemPacketTest {
protected:
	void SetUp() override {
		// The world holders are published once per process. This executable's other fixtures publish P4-10's test set
		// (tests/world/WorldTestSupport.h), which refuses a second publisher, so this fixture publishes the same set first; its Poeta row is the
		// item fixture's, whose own once-publisher then finds the holders published and leaves them (ItemPacketTestSupport.h).
		ASSERT_TRUE(world::test::publishTestStaticData()) << "this process published the real static data";
		ItemPacketTest::SetUp();
		runtime::resetUnportedHitsForTests();
		savedMissingAiHandlers = configs::main::AIConfig::MISSING_AI_HANDLERS.get();
		configs::main::AIConfig::MISSING_AI_HANDLERS.set("warn");
		// the Java defaults of the keys the arms read (PricesConfig.java:28-32, AutoGroupConfig.java:12-13 is true; every gate profile and the
		// shipped mygs.properties set it false, and so does this fixture unless a case says otherwise)
		savedBuyModifier = configs::main::PricesConfig::VENDOR_BUY_MODIFIER.exchange(100);
		savedSellModifier = configs::main::PricesConfig::VENDOR_SELL_MODIFIER.exchange(20);
		savedAutoGroup = configs::main::AutoGroupConfig::AUTO_GROUP_ENABLE.exchange(false);
		xml::LoadContext context;
		dataholders::DataManager::TRADE_LIST_DATA.publish(xml::bindString<dataholders::TradeListData>(context, NPC_TRADE_LIST_XML));
		dataholders::DataManager::GOODSLIST_DATA.publish(xml::bindString<dataholders::GoodsListData>(context, GOODSLISTS_XML));
		// the fixture's item rows plus the Abbey Return Stone
		std::string itemRows(ITEM_TEMPLATES_XML);
		itemRows.insert(itemRows.rfind("</item_templates>"), ABBEY_RETURN_STONE_XML);
		dataholders::DataManager::ITEM_DATA.resetForTests();
		dataholders::DataManager::ITEM_DATA.publish(xml::bindString<dataholders::ItemData>(context, itemRows));
		dataholders::DataManager::PLAYER_EXPERIENCE_TABLE.resetForTests();
		dataholders::DataManager::PLAYER_EXPERIENCE_TABLE.publish(
			xml::bindString<dataholders::PlayerExperienceTable>(context, PLAYER_EXPERIENCE_TABLE_FULL_XML));
		// the item fixture's skill rows plus Soul Sickness (the base TearDown resets SKILL_DATA)
		std::string skillRows(SKILL_TEMPLATES_XML);
		skillRows.insert(skillRows.rfind("</skill_data>"), SOUL_SICKNESS_XML);
		dataholders::DataManager::SKILL_DATA.resetForTests();
		dataholders::DataManager::SKILL_DATA.publish(xml::bindString<dataholders::SkillData>(context, skillRows));
		dataholders::DataManager::NPC_FACTIONS_DATA.publish(xml::bindString<dataholders::NpcFactionsData>(context, NPC_FACTIONS_XML));
		// Java PlayerService.loadPlayer: a mailbox, the quest states (DialogPage.hasQuestInteraction reads the uncompleted quests) and the npc
		// factions (PlayerNpcFactionsDAO.loadNpcFactions: none for this character)
		player().setMailbox(std::make_unique<model::gameobjects::player::Mailbox>(player()));
		questStates = model::gameobjects::player::QuestStateList::create();
		player().setQuestStateList(questStates);
		player().setNpcFactions(std::make_unique<model::gameobjects::player::npcFaction::NpcFactions>(player()));
	}

	void TearDown() override {
		npcs.clear(); // before the map instance their positions name
		spawnGroups.clear();
		if (f.player)
			f.player->setQuestStateList(nullptr);
		questStates = nullptr;
		dataholders::DataManager::NPC_FACTIONS_DATA.resetForTests();
		dataholders::DataManager::GOODSLIST_DATA.resetForTests();
		dataholders::DataManager::TRADE_LIST_DATA.resetForTests();
		configs::main::AutoGroupConfig::AUTO_GROUP_ENABLE.store(savedAutoGroup);
		configs::main::PricesConfig::VENDOR_SELL_MODIFIER.store(savedSellModifier);
		configs::main::PricesConfig::VENDOR_BUY_MODIFIER.store(savedBuyModifier);
		if (savedMissingAiHandlers)
			configs::main::AIConfig::MISSING_AI_HANDLERS.set(*savedMissingAiHandlers);
		ItemPacketTest::TearDown();
	}

	/** The npc of the row, spawned 3 m from the player at (103, 100, 50) in his map instance (Java VisibleObjectSpawner.spawnNpc) */
	Npc& npc(int32_t npcId) {
		const model::templates::npc::NpcTemplate* objectTemplate = npcData().getNpcTemplate(npcId);
		EXPECT_NE(objectTemplate, nullptr) << npcId;
		runtime::Ref<model::templates::spawns::SpawnGroup> group = model::templates::spawns::SpawnGroup::create(210010000, npcId, 0, nullptr);
		model::templates::spawns::SpawnTemplate& spawn = group->addSpawnTemplate(std::make_unique<DialogSpawnTemplate>(*group, 103.0f, 100.0f, 50.0f));
		runtime::Ref<Npc> created =
			model::gameobjects::VisibleObject::create<Npc>(std::make_unique<controllers::NpcController>(), spawn, objectTemplate);
		created->setKnownlist(std::make_unique<world::knownlist::NpcKnownList>(*created));
		created->setEffectController(std::make_unique<controllers::effect::EffectController>(*created));
		created->setPosition(world::WorldPosition::create(210010000, 103.0f, 100.0f, 50.0f, int8_t{0}, mapInstance->getRegion(103.0f, 100.0f, 50.0f)));
		created->getPosition()->setIsSpawned(true);
		spawnGroups.push_back(group);
		npcs.push_back(created);
		return *created;
	}

	bool allowed(Npc& target) { return DialogService::isInteractionAllowed(player(), target); }

	void select(int32_t dialogActionId, Npc& target, int32_t questId = 0) { DialogService::onDialogSelect(dialogActionId, player(), target, questId, 0); }

	model::gameobjects::player::PlayerCommonData& commonData() { return *player().getCommonData(); }

	/** A level of the full experience table on the (offline) player's common data; a Daeva's levels go past 9 (PlayerCommonData.java:276-281) */
	void setDaevaLevel(int32_t level) {
		commonData().setDaeva(true);
		commonData().setLevel(level);
		ASSERT_EQ(player().getLevel(), level);
	}

	/** SM_DIALOG_WINDOW.writeImpl (SM_DIALOG_WINDOW.java:29-41) of a page that is neither MAIL nor TOWN_CHALLENGE_TASK */
	static std::vector<uint8_t> dialogWindow(int32_t npcObjectId, int32_t page, int32_t questId = 0) {
		return javaPacket(SM_DIALOG_WINDOW_OPCODE, PacketWriter().D(npcObjectId).H(page).D(questId).H(0).H(0));
	}

	std::vector<uint8_t> recoverQuestion(int32_t price) {
		return serializedFor(SM_QUESTION_WINDOW(SM_QUESTION_WINDOW::STR_ASK_RECOVER_EXPERIENCE, 0, 0, std::to_string(price)));
	}

	/**
	 * Soul Sickness on the player as a revive leaves it (PlayerController.updateSoulSickness casts 8291 on him): the effect applied directly
	 * (SkillEngine.applyEffectDirectly), in the SPEC2 slot. The packets it sent are cleared.
	 */
	void makeSoulSick() {
		skillengine::SkillEngine::getInstance().applyEffectDirectly(SOUL_SICKNESS, player(), player());
		ASSERT_TRUE(soulSick());
		clearSent();
	}

	bool soulSick() { return static_cast<Creature&>(player()).getEffectController()->hasAbnormalEffect(SOUL_SICKNESS); }

	std::shared_ptr<const std::string> savedMissingAiHandlers;
	int32_t savedBuyModifier = 0;
	int32_t savedSellModifier = 0;
	bool savedAutoGroup = false;
	runtime::Ref<model::gameobjects::player::QuestStateList> questStates;
	std::vector<runtime::Ref<model::templates::spawns::SpawnGroup>> spawnGroups;
	std::vector<runtime::Ref<Npc>> npcs;
};

// ---- isInteractionAllowed: the summon owner (DialogService.java:298-312) -----------------------------------------------------------------------

TEST_F(DialogServiceTest, AnNpcWithoutASummonOwnerOrASubdialogIsOpenToEveryone) {
	EXPECT_TRUE(allowed(npc(MINALINERK)));
	EXPECT_TRUE(allowed(npc(PERNOS)));
}

TEST_F(DialogServiceTest, APrivateSummonTalksOnlyToItsCreator) {
	Npc& summoned = npc(PERNOS);
	summoned.setSummonOwner(SummonOwner::PRIVATE);

	summoned.setCreatorId(player().getObjectId());
	EXPECT_TRUE(allowed(summoned));
	summoned.setCreatorId(player().getObjectId() + 1);
	EXPECT_FALSE(allowed(summoned)) << "PRIVATE -> playerIsCreator";
}

TEST_F(DialogServiceTest, AGroupAllianceOrLegionSummonTalksToItsCreatorAndToNoOutsider) {
	// the player is in no group, no alliance and no legion: each arm's second operand is false before it asks the team (GeneralTeam.hasMember
	// and Legion.isMember are other milestones' bodies), so the creator alone decides
	for (SummonOwner owner : {SummonOwner::GROUP, SummonOwner::ALLIANCE, SummonOwner::LEGION}) {
		SCOPED_TRACE(std::string(xml::enumName(owner)));
		Npc& summoned = npc(PERNOS);
		summoned.setSummonOwner(owner);
		summoned.setCreatorId(player().getObjectId());
		EXPECT_TRUE(allowed(summoned));
		summoned.setCreatorId(player().getObjectId() + 1);
		EXPECT_FALSE(allowed(summoned));
	}
	EXPECT_EQ(runtime::unportedHitCount(), 0u);
}

TEST_F(DialogServiceTest, AForeignSummonIsRefusedBeforeItsSubdialogIsAsked) {
	// isInteractionAllowed: `getSummonOwner() != null && !isSummonOwner` returns first - TARGET_LEGION_DOMINION would throw (W-30) if asked
	Npc& entrance = npc(STONESPEAR_ENTRANCE);
	entrance.setSummonOwner(SummonOwner::PRIVATE);
	entrance.setCreatorId(player().getObjectId() + 1);
	EXPECT_FALSE(allowed(entrance));
	EXPECT_EQ(runtime::unportedHitCount(), 0u);
}

// ---- isSubDialogRestricted (DialogService.java:314-377) ---------------------------------------------------------------------------------------

TEST_F(DialogServiceTest, FortCaptureNeedsALegion) {
	// :319-321: no legion -> restricted, before the npc's zones are searched
	EXPECT_FALSE(allowed(npc(GUARDIAN_BATTERY)));
}

TEST_F(DialogServiceTest, SkillIdNeedsTheSkill) {
	Npc& halliwell = npc(HALLIWELL); // subdialog_value 1377
	EXPECT_FALSE(allowed(halliwell)) << "the warrior knows only the sword skill";
	player().setSkillList(model::skill::PlayerSkillList::create({model::skill::PlayerSkillEntry::create(SWORD_SKILL, 1, 0,
																	 model::gameobjects::Persistable_PersistentState::UPDATED),
		model::skill::PlayerSkillEntry::create(1377, 1, 0, model::gameobjects::Persistable_PersistentState::UPDATED)}));
	EXPECT_TRUE(allowed(halliwell));
}

TEST_F(DialogServiceTest, ReturnNeedsAnAbbeyReturnStone) {
	Npc& portal = npc(DEFENSE_PORTAL);
	EXPECT_FALSE(allowed(portal));
	stored(793001, ABBEY_RETURN_STONE, 1);
	EXPECT_TRUE(allowed(portal)) << ":347-348: getItemCountByItemId(164000335) == 0";
}

TEST_F(DialogServiceTest, AbyssRankNeedsTheRankOrStaff) {
	using utils::stats::AbyssRankEnum;
	Npc& corridor = npc(COMMANDERS_CORRIDOR); // subdialog_value 18: SUPREME_COMMANDER
	EXPECT_FALSE(allowed(corridor)) << "GRADE9_SOLDIER is rank id 1";
	// :352: AbyssRankEnum.getId() < value, the id being the ordinal + 1 (AbyssRankEnum.java:15-32): 17 is below 18, 18 is not
	player().getAbyssRank()->setRank(AbyssRankEnum::COMMANDER);
	EXPECT_FALSE(allowed(corridor)) << "COMMANDER is rank id 17";
	player().getAbyssRank()->setRank(AbyssRankEnum::SUPREME_COMMANDER);
	EXPECT_TRUE(allowed(corridor)) << "SUPREME_COMMANDER is rank id 18";
	player().getAbyssRank()->setRank(AbyssRankEnum::GRADE9_SOLDIER);
	EXPECT_FALSE(allowed(corridor));
	f.account->setAccessLevel(1);
	EXPECT_TRUE(allowed(corridor)) << ":350-351: staff pass";
}

TEST_F(DialogServiceTest, TargetLegionDominionAsksTheUnportedCalculationTimeFirst) {
	// :355-357: LegionDominionService.isInCalculationTime is M5h's (m5c-plan.md W-30): loud, before the legion is looked at
	EXPECT_THROW(allowed(npc(STONESPEAR_ENTRANCE)), runtime::UnportedException);
	EXPECT_EQ(unportedHitsIn("LegionDominionService.cpp"), 1u);
	EXPECT_EQ(runtime::unportedHitCount(), 1u);
}

TEST_F(DialogServiceTest, LegionDominionNpcNeedsALegion) {
	EXPECT_FALSE(allowed(npc(PUCORINERK))) << ":362-366: no legion -> restricted";
}

TEST_F(DialogServiceTest, TheLevelSubdialogsCompareTheLevelWithTheValue) {
	Npc& melanie = npc(MELANIE);       // LEVEL 40: exactly 40
	Npc& lushrunerk = npc(LUSHRUNERK); // LEVEL_LOW 54: at most 54
	Npc& lalrinerk = npc(LALRINERK);   // LEVEL_HIGH 55: at least 55
	struct Row {
		int32_t level;
		bool melanie, lushrunerk, lalrinerk;
	};
	const Row rows[] = {{39, false, true, false}, {40, true, true, false}, {41, false, true, false}, {54, false, true, false}, {55, false, false, true}};
	for (const Row& row : rows) {
		SCOPED_TRACE("level " + std::to_string(row.level));
		setDaevaLevel(row.level);
		EXPECT_EQ(allowed(melanie), row.melanie) << ":367-368 level != value";
		EXPECT_EQ(allowed(lushrunerk), row.lushrunerk) << ":369-370 level > value";
		EXPECT_EQ(allowed(lalrinerk), row.lalrinerk) << ":371-372 level < value";
	}
}

TEST_F(DialogServiceTest, TheOtherSubdialogTypesAreRestrictedWithAWarning) {
	// :373-375: the six types of the data without an arm of their own
	LogCapture capture({"com.aionemu.gameserver.services.DialogService"});
	const std::pair<int32_t, std::string_view> rows[] = {{PORIRUNERK, "PCBANG"}, {INCOMPLETE_SHUGOROBO, "PACK_3"}, {SHUGOROBO, "PACK_4"},
		{BLESSED_TOTEM, "CASH"}, {NEW_USER_MELANIE, "NEWBIE"}, {BUNTON, "PAID_USER"}};
	for (const auto& [npcId, type] : rows) {
		SCOPED_TRACE(std::string(type));
		EXPECT_FALSE(allowed(npc(npcId)));
		EXPECT_TRUE(capture.contains("Unhandled subdialog type " + std::string(type) + " for npc: " + std::to_string(npcId))) << capture.dump();
	}
}

// ---- DialogPage.getStartPageId through isInteractionAllowed (DialogPage.java:113-125) --------------------------------------------------------

TEST_F(DialogServiceTest, TheStartPageFollowsTheTalkInfoAndTheInteractionCheck) {
	EXPECT_EQ(model::getStartPageId(npc(MAILBOX), player()), 0) << "no dialog and no function dialog";
	EXPECT_EQ(model::getStartPageId(npc(MINALINERK), player()), 10) << "a function npc";
	EXPECT_EQ(model::getStartPageId(npc(LALRINERK), player()), 1011) << "a function npc whose subdialog refuses the level-1 player";
	Npc& pernos = npc(PERNOS);
	EXPECT_EQ(model::getStartPageId(pernos, player()), 1011) << "a dialog npc without quests";
	commonData().setDaeva(true);
	EXPECT_EQ(model::getStartPageId(pernos, player()), 1352) << "pernos has another dialog for a Daeva";
}

// ---- onDialogSelect (DialogService.java:69-291) -----------------------------------------------------------------------------------------------

TEST_F(DialogServiceTest, BuyOpensTheTradeListOfTheMerchant) {
	Npc& merchant = npc(MINALINERK);

	select(DialogAction::BUY, merchant);

	// :80-92 -> SM_TRADELIST.writeImpl: D npc, C TradeNpcType.NORMAL.index() 1, D vendor buy modifier 100 * sell_price_rate 100 / 100,
	// D 100, C canSell, C canBuy, H tabs, D 132, D 720 (both legion_lvl 0), H no limited items
	EXPECT_EQ(sent(), exactly({javaPacket(SM_TRADELIST_OPCODE,
						  PacketWriter().D(merchant.getObjectId()).C(1).D(100).D(100).C(1).C(1).H(2).D(132).D(720).H(0))}));
}

TEST_F(DialogServiceTest, BuyAtTheDefaultRatePassesTheVendorModifierOn) {
	configs::main::PricesConfig::VENDOR_BUY_MODIFIER.store(113);
	Npc& merchant = npc(MINALINERK);

	select(DialogAction::BUY, merchant);

	// :92: PricesService.getVendorBuyModifier() * tradeModifier / 100, the rate's default 100 (TradeListTemplate.java:27-28)
	EXPECT_EQ(sent(), exactly({javaPacket(SM_TRADELIST_OPCODE,
						  PacketWriter().D(merchant.getObjectId()).C(1).D(113).D(100).C(1).C(1).H(2).D(132).D(720).H(0))}));
}

TEST_F(DialogServiceTest, BuyScalesTheVendorModifierByTheSellPriceRate) {
	Npc& nornes = npc(NORNES); // sell_price_rate 150 (npc_trade_list.xml:412), goods lists 277 and 404 without a legion level
	const auto tradeList = [&](int32_t modifier) {
		return javaPacket(SM_TRADELIST_OPCODE, PacketWriter().D(nornes.getObjectId()).C(1).D(modifier).D(100).C(1).C(1).H(2).D(277).D(404).H(0));
	};

	// :80, :92: PricesService.getVendorBuyModifier() * tradeModifier / 100 in Java int arithmetic
	select(DialogAction::BUY, nornes);
	EXPECT_EQ(sent(), exactly({tradeList(150)})) << "100 * 150 / 100";
	clearSent();

	configs::main::PricesConfig::VENDOR_BUY_MODIFIER.store(113);
	select(DialogAction::BUY, nornes);
	EXPECT_EQ(sent(), exactly({tradeList(169)})) << "113 * 150 = 16950, / 100 = 169 (the product first, then the int division)";
	clearSent();

	configs::main::PricesConfig::VENDOR_BUY_MODIFIER.store(20000000);
	select(DialogAction::BUY, nornes);
	EXPECT_EQ(sent(), exactly({tradeList(-12949672)})) << "20000000 * 150 wraps to -1294967296 as a Java int; / 100 truncates toward 0";
}

TEST_F(DialogServiceTest, BuyAtAnNpcWithoutATradeListOrWithOnlyLegionGoodsSaysItSellsNothing) {
	Npc& mogironerk = npc(MOGIRONERK); // func_dialogs 2, no tradelist_template
	select(DialogAction::BUY, mogironerk);
	EXPECT_EQ(sent(), exactly({serializedFor(SM_SYSTEM_MESSAGE::STR_BUY_SELL_HE_DOES_NOT_SELL_ITEM(mogironerk.getObjectTemplate()->getL10n()))}))
		<< ":76-78";
	clearSent();

	Npc& gwenspena = npc(GWENSPENA); // four goods lists of legion levels 2 to 5, and the player has no legion (legion level 0)
	select(DialogAction::BUY, gwenspena);
	EXPECT_EQ(sent(), exactly({serializedFor(SM_SYSTEM_MESSAGE::STR_BUY_SELL_HE_DOES_NOT_SELL_ITEM(gwenspena.getObjectTemplate()->getL10n()))}))
		<< ":82-94";
}

TEST_F(DialogServiceTest, SellOpensTheSellWindow) {
	Npc& merchant = npc(MINALINERK);

	select(DialogAction::SELL, merchant);

	// :251-254 -> SM_SELL_ITEM: no purchase_template, so NORMAL (1), the vendor sell modifier 20, canSell, canBuy || canPurchase, no tabs
	EXPECT_EQ(sent(), exactly({javaPacket(SM_SELL_ITEM_OPCODE, PacketWriter().D(merchant.getObjectId()).C(1).D(20).C(1).C(1).H(0))}));
}

TEST_F(DialogServiceTest, TradeSellListOpensThePurchaseListOfAPurchaseNpc) {
	Npc& amarunerk = npc(AMARUNERK);

	select(DialogAction::TRADE_SELL_LIST, amarunerk);

	// SM_SELL_ITEM with the purchase_template :9570: ABYSS (2), buy_price_rate 150, no BUY, canPurchase, its one tab 59
	EXPECT_EQ(sent(), exactly({javaPacket(SM_SELL_ITEM_OPCODE, PacketWriter().D(amarunerk.getObjectId()).C(2).D(150).C(0).C(1).H(1).D(59))}));
}

TEST_F(DialogServiceTest, TradeInOpensTheTradeInListOrSaysTheNpcSellsNothing) {
	configs::main::PricesConfig::VENDOR_BUY_MODIFIER.store(113); // not read by this arm: its modifier is the literal 100
	Npc& perbano = npc(PERBANO);
	select(DialogAction::TRADE_IN, perbano);
	// :244-249 -> SM_TRADE_IN_LIST.writeImpl (SM_TRADE_IN_LIST.java:26-35): D npc, C TradeNpcType.NORMAL.index() 1 (the default), D 100, D 100,
	// H one tab, D 39 (npc_trade_list.xml:9062-9064)
	EXPECT_EQ(sent(), exactly({javaPacket(SM_TRADE_IN_LIST_OPCODE, PacketWriter().D(perbano.getObjectId()).C(1).D(100).D(100).H(1).D(39))}));
	clearSent();

	Npc& merchant = npc(MINALINERK); // a tradelist_template, but no trade_in_list_template
	select(DialogAction::TRADE_IN, merchant);
	EXPECT_EQ(sent(), exactly({serializedFor(SM_SYSTEM_MESSAGE::STR_BUY_SELL_HE_DOES_NOT_SELL_ITEM(merchant.getObjectTemplate()->getL10n()))}))
		<< ":246-247";
}

TEST_F(DialogServiceTest, APageFunctionOpensItsPageOnlyAtAnNpcThatHasIt) {
	Npc& seril = npc(SERIL); // func_dialogs 42 = REMOVE_ITEM_OPTION
	select(DialogAction::REMOVE_ITEM_OPTION, seril);
	// :103, :118 -> sendDialogWindow: DialogPage.getByActionId(42) is REMOVE_MANASTONE, page 20 (DialogPage.java:37)
	EXPECT_EQ(sent(), exactly({dialogWindow(seril.getObjectId(), 20)}));
	clearSent();

	select(DialogAction::REMOVE_ITEM_OPTION, npc(MINALINERK));
	EXPECT_TRUE(sent().empty()) << ":294: supportsAction(42) is false";
}

TEST_F(DialogServiceTest, ThePoetaTeleporterRefusesAPlayerWhoIsNoDaevaAndShowsADaevaTheUnportedMap) {
	Npc& daines = npc(DAINES);
	select(DialogAction::AIRLINE_SERVICE, daines);
	// :190-193: DialogPage.NO_RIGHT (27), and the arm returns
	EXPECT_EQ(sent(), exactly({dialogWindow(daines.getObjectId(), 27)}));
	EXPECT_EQ(runtime::unportedHitCount(), 0u);

	commonData().setDaeva(true);
	// :196: TeleportService.showMap stays AION_UNPORTED until M5f (m5c-plan.md W-08, D4)
	EXPECT_THROW(select(DialogAction::AIRLINE_SERVICE, daines), runtime::UnportedException);
	EXPECT_EQ(unportedHitsIn("TeleportService.cpp"), 1u);
}

TEST_F(DialogServiceTest, MatchMakerWithAutogroupOffOpensTheFirstPage) {
	Npc& merchant = npc(MINALINERK);
	select(DialogAction::MATCH_MAKER, merchant);
	EXPECT_EQ(sent(), exactly({dialogWindow(merchant.getObjectId(), 1011)})) << ":222-224";

	// autogroup on: AutoGroupType has no C++ companion yet, so the branch is loud (m5c-plan.md W-31, D4)
	configs::main::AutoGroupConfig::AUTO_GROUP_ENABLE.store(true);
	EXPECT_THROW(select(DialogAction::MATCH_MAKER, merchant), runtime::UnportedException);
	EXPECT_EQ(unportedHitsIn("DialogService.cpp"), 1u);
}

TEST_F(DialogServiceTest, TheFactionArmsJoinAndLeaveThroughTheNpcFactions) {
	Npc& typhon = npc(TYPHON); // func_dialogs 68 69, the registrar of npc_faction 2 (Alabaster Order, min_level 30)

	select(DialogAction::FACTION_JOIN, typhon);
	// :226-228 -> NpcFactions.enterGuild (NpcFactions.java:108-137): no skill points, and level 1 is below min_level 30 -> page 1182
	EXPECT_EQ(sent(), exactly({dialogWindow(typhon.getObjectId(), 1182)}));
	clearSent();

	select(DialogAction::FACTION_SEPARATE, typhon);
	// :229-231 -> NpcFactions.leaveNpcFaction(Npc) (NpcFactions.java:82-91): the character is in no faction -> page 1438
	EXPECT_EQ(sent(), exactly({dialogWindow(typhon.getObjectId(), 1438)}));
	EXPECT_EQ(runtime::unportedHitCount(), 0u);
}

TEST_F(DialogServiceTest, AnyOtherActionIsTheNextPage) {
	Npc& pernos = npc(PERNOS);

	select(1012, pernos); // SELECT2: not an arm of the switch
	// :273-274 -> handleQuestDialogueOrSendNextPage: no quest id and no USE_OBJECT/EXCHANGE_COIN, so "action id = next page id"
	EXPECT_EQ(sent(), exactly({dialogWindow(pernos.getObjectId(), 1012)}));
}

TEST_F(DialogServiceTest, WithAQuestIdEvenAFunctionIsHandedToTheQuestsFirst) {
	Npc& merchant = npc(MINALINERK);

	select(DialogAction::BUY, merchant, 1100);

	// :72, :277-278: a quest id skips the switch; QuestEngine.onDialog finds no handler for quest 1100, so the page is the action id and the
	// quest id travels with it (SM_DIALOG_WINDOW.java:33) - no trade list
	EXPECT_EQ(sent(), exactly({dialogWindow(merchant.getObjectId(), DialogAction::BUY, 1100)}));
}

TEST_F(DialogServiceTest, TheStartActionWithoutAQuestAsksTheNpcsQuestsAndSendsItsOwnId) {
	Npc& pernos = npc(PERNOS);

	select(DialogAction::USE_OBJECT, pernos);

	// :283-290: USE_OBJECT asks QuestEngine.onDialog (no quest npc registered), then the page is the action id -1, written as an unsigned H
	EXPECT_EQ(sent(), exactly({dialogWindow(pernos.getObjectId(), 0xFFFF)}));
}

TEST_F(DialogServiceTest, TheArmsOfOtherServicesReachTheirOwnUnportedBodies) {
	// Each arm hands the dialog to a body of another item or milestone that is still AION_UNPORTED: the select throws that body's
	// UnportedException, and that function is the one site hit. DialogService does not ask the npc's functions on these arms (CM_DIALOG_SELECT's
	// audit does), so the rows use an npc that offers the function where the fixture has one and minalinerk otherwise; the pvp arms act only
	// at their arena npcs (the next case).
	struct Row {
		int32_t action;
		int32_t npcId;
		std::string_view function;
	};
	const Row rows[] = {
		{DialogAction::DISPERSE_LEGION, MINALINERK, "LegionService::requestDisbandLegion"},      // :120-122, P5-11
		{DialogAction::RECREATE_LEGION, MINALINERK, "LegionService::recreateLegion"},           // :123-125, P5-11
		{DialogAction::ENTER_PVP, EPEIOS, "TeleportService::teleportTo"},                       // :166-168, Sanctum's arena (W-29)
		{DialogAction::LEAVE_PVP, NEPIS, "TeleportService::teleportTo"},                        // :179-181, out of Sanctum's arena (W-29)
		{DialogAction::GATHER_SKILL_LEVELUP, MINALINERK, "CraftSkillUpdateService::learnSkill"}, // :199-202, C-01
		{DialogAction::COMBINE_SKILL_LEVELUP, MINALINERK, "CraftSkillUpdateService::learnSkill"},
		{DialogAction::EXTEND_INVENTORY, MINALINERK, "CubeExpandService::expandCube"},                  // :203-205, P-05 (W-09)
		{DialogAction::EXTEND_CHAR_WAREHOUSE, MINALINERK, "WarehouseService::expandWarehouse"},        // :206-208, P5-07
		{DialogAction::OPEN_LEGION_WAREHOUSE, PAUTON, "LegionService::openLegionWarehouse"},           // :209-211, P5-11
		{DialogAction::CHARGE_ITEM_MULTI, MINALINERK, "ItemChargeService::startChargingEquippedItems"}, // :241-243, P5-07
		{DialogAction::GIVEUP_CRAFT_EXPERT, MINALINERK, "CraftSkillUpdateService::getProfessionByNpc"}, // :255-257, C-01
		{DialogAction::GIVEUP_CRAFT_MASTER, MINALINERK, "CraftSkillUpdateService::getProfessionByNpc"}, // :258-260, C-01
		{DialogAction::CHARGE_ITEM_MULTI2, MINALINERK, "ItemChargeService::startChargingEquippedItems"}, // :267-269, P5-07
	};
	for (const Row& row : rows) {
		SCOPED_TRACE(std::string(DialogAction::nameOf(row.action).value_or("?")) + " at " + std::to_string(row.npcId));
		Npc& target = npc(row.npcId);
		runtime::resetUnportedHitsForTests();
		clearSent();
		EXPECT_THROW(select(row.action, target), runtime::UnportedException);
		EXPECT_EQ(unportedHitsOf(row.function), 1u);
		EXPECT_EQ(runtime::unportedHitCount(), 1u);
		EXPECT_TRUE(sent().empty());
	}
}

TEST_F(DialogServiceTest, ThePvpArmsTeleportOnlyFromTheirArenaNpcs) {
	// :161-186: an inner switch on the npc id with no default - any other npc, and the enter npc asked to leave or the other way round, does
	// nothing
	Npc& merchant = npc(MINALINERK);
	Npc& epeios = npc(EPEIOS);
	Npc& nepis = npc(NEPIS);
	EXPECT_NO_THROW(select(DialogAction::ENTER_PVP, merchant));
	EXPECT_NO_THROW(select(DialogAction::LEAVE_PVP, merchant));
	EXPECT_NO_THROW(select(DialogAction::ENTER_PVP, nepis));
	EXPECT_NO_THROW(select(DialogAction::LEAVE_PVP, epeios));
	EXPECT_TRUE(sent().empty());
	EXPECT_EQ(runtime::unportedHitCount(), 0u);
}

TEST_F(DialogServiceTest, TheCharacterEditArmsOpenTheEditorAndMarkTheEditMode) {
	select(DialogAction::EDIT_CHARACTER_GENDER, npc(MINALINERK));

	// :212-216: SM_PLASTIC_SURGERY.writeImpl: D player, C no ticket 2 (the player holds none), C gender switch 1
	EXPECT_EQ(sent(), exactly({javaPacket(SM_PLASTIC_SURGERY_OPCODE, PacketWriter().D(player().getObjectId()).C(2).C(1))}));
	EXPECT_TRUE(commonData().isInEditMode());
}

TEST_F(DialogServiceTest, TheFullCharacterEditIsNoGenderSwitch) {
	select(DialogAction::EDIT_CHARACTER_ALL, npc(MINALINERK));

	// :214: dialogActionId == EDIT_CHARACTER_GENDER is false for EDIT_CHARACTER_ALL (61) -> C gender switch 0
	EXPECT_EQ(sent(), exactly({javaPacket(SM_PLASTIC_SURGERY_OPCODE, PacketWriter().D(player().getObjectId()).C(2).C(0))}));
	EXPECT_TRUE(commonData().isInEditMode());
}

TEST_F(DialogServiceTest, ThePetArmsSendTheirPetWindows) {
	Npc& merchant = npc(MINALINERK);
	select(DialogAction::FUNC_PET_ADOPT, merchant);
	select(DialogAction::FUNC_PET_ABANDON, merchant);
	select(DialogAction::FUNC_PET_H_ADOPT, merchant);
	select(DialogAction::FUNC_PET_H_ABANDON, merchant);

	// :235-240, :261-266 -> SM_PET.writeImpl: H the action id and no body for TALK_WITH_MERCHANT 6, TALK_WITH_MINDER 7, H_ADOPT 16 and
	// H_ABANDON 17 (PetAction.java:15-16, :22-23)
	EXPECT_EQ(sent(), exactly({javaPacket(SM_PET_OPCODE, PacketWriter().H(6)), javaPacket(SM_PET_OPCODE, PacketWriter().H(7)),
						  javaPacket(SM_PET_OPCODE, PacketWriter().H(16)), javaPacket(SM_PET_OPCODE, PacketWriter().H(17))}));
}

TEST_F(DialogServiceTest, TheRepurchaseAndPetArmsSendTheirWindows) {
	Npc& merchant = npc(MINALINERK);
	select(DialogAction::BUY_AGAIN, merchant);
	select(DialogAction::FUNC_PET_ADOPT, merchant);
	EXPECT_EQ(opcodesOf(sent()), (std::vector<int32_t>{SM_REPURCHASE_OPCODE, SM_PET_OPCODE})) << ":232-237";
}

// ---- RECOVERY: soul healing (DialogService.java:126-160) --------------------------------------------------------------------------------------

TEST_F(DialogServiceTest, RecoveryWithoutLostExperienceSaysSoAndClearsTheDeathCount) {
	commonData().setDeathCount(3);

	select(DialogAction::RECOVERY, npc(PALAEMON));

	// :128-131 clears the death count, :157-158 says there is nothing to recover and asks nothing
	EXPECT_EQ(sent(), exactly({serializedFor(SM_SYSTEM_MESSAGE::STR_DONOT_HAVE_RECOVER_EXPERIENCE())}));
	EXPECT_EQ(commonData().getDeathCount(), 0);
	EXPECT_FALSE(player().getResponseRequester().respond(SM_QUESTION_WINDOW::STR_ASK_RECOVER_EXPERIENCE, 1)) << "no question is pending";
}

TEST_F(DialogServiceTest, RecoveryWithoutLostExperienceEndsTheSoulSickness) {
	commonData().setDeathCount(3);
	makeSoulSick();
	Npc& palaemon = npc(PALAEMON);

	select(DialogAction::RECOVERY, palaemon);

	// :128-130: removeByDispelSlotType(SPECIAL2) ends the SPEC2 effect before anything is said; :157-158 then says there is nothing to recover
	EXPECT_FALSE(soulSick());
	std::vector<std::vector<uint8_t>> packets = sent();
	ASSERT_FALSE(packets.empty());
	EXPECT_EQ(packets.back(), serializedFor(SM_SYSTEM_MESSAGE::STR_DONOT_HAVE_RECOVER_EXPERIENCE()));
	EXPECT_EQ(packetsOf(packets, SM_SYSTEM_MESSAGE_OPCODE), exactly({serializedFor(SM_SYSTEM_MESSAGE::STR_DONOT_HAVE_RECOVER_EXPERIENCE())}));
	EXPECT_TRUE(packetsOf(packets, SM_QUESTION_WINDOW_OPCODE).empty());
	EXPECT_EQ(commonData().getDeathCount(), 0);
}

TEST_F(DialogServiceTest, RecoveryPricesTheLostExperienceWithJavasDoubleArithmetic) {
	// :132-133: factor = expLost < 1000000 ? 0.25 - 0.00000015 * expLost : 0.1; price = (int) (expLost * factor), a saturating cast. The prices
	// are the IEEE double results (Python floats compute the same values): 24.9985 -> 24, 66666.61666665 -> 66666, 100000.04999985002 ->
	// 100000, 100000.0, and 3.0E9, which Java's (int) saturates to Integer.MAX_VALUE (a plain C++ cast is undefined there)
	const std::pair<int64_t, int32_t> rows[] = {{100, 24}, {333333, 66666}, {999999, 100000}, {1000000, 100000}, {30000000000LL, 2147483647}};
	Npc& palaemon = npc(PALAEMON);
	commonData().setDeathCount(2);
	for (const auto& [expLost, price] : rows) {
		SCOPED_TRACE("expLost " + std::to_string(expLost));
		commonData().setRecoverableExp(expLost);
		select(DialogAction::RECOVERY, palaemon);
		EXPECT_EQ(sent(), exactly({recoverQuestion(price)}));
		clearSent();
		ASSERT_TRUE(player().getResponseRequester().respond(SM_QUESTION_WINDOW::STR_ASK_RECOVER_EXPERIENCE, 0)) << "the question was pending";
		EXPECT_TRUE(sent().empty()) << "declining says nothing (RequestResponseHandler.denyRequest is empty)";
	}
	EXPECT_EQ(commonData().getDeathCount(), 2) << "only an empty recoverable experience clears the count before the question";
}

TEST_F(DialogServiceTest, RecoveryAskedTwiceShowsOneQuestion) {
	commonData().setRecoverableExp(100);
	Npc& palaemon = npc(PALAEMON);
	select(DialogAction::RECOVERY, palaemon);
	clearSent();

	select(DialogAction::RECOVERY, palaemon);

	EXPECT_TRUE(sent().empty()) << ":152-156: putRequest refuses a second request with the same id, and nothing is sent";
}

TEST_F(DialogServiceTest, AcceptingTheRecoveryPaysAndRestoresTheExperience) {
	Item& kinah = stored(793002, KINAH, 1000);
	commonData().setExp(1000);
	commonData().setRecoverableExp(100);
	commonData().setDeathCount(4);
	makeSoulSick();
	select(DialogAction::RECOVERY, npc(PALAEMON));
	clearSent();
	ASSERT_TRUE(soulSick()) << ":128: with experience to recover the question comes first";

	ASSERT_TRUE(player().getResponseRequester().respond(SM_QUESTION_WINDOW::STR_ASK_RECOVER_EXPERIENCE, 1));

	// DialogService$1.acceptRequest (:139-145): the two messages, then the experience is given back, the price taken and the Soul Sickness
	// (SPEC2) ended
	std::vector<std::vector<uint8_t>> packets = sent();
	ASSERT_GE(packets.size(), 2u);
	EXPECT_EQ(packets[0], serializedFor(SM_SYSTEM_MESSAGE::STR_GET_EXP2(100)));
	EXPECT_EQ(packets[1], serializedFor(SM_SYSTEM_MESSAGE::STR_SUCCESS_RECOVER_EXPERIENCE()));
	EXPECT_EQ(commonData().getExpRecoverable(), 0);
	EXPECT_EQ(commonData().getExp(), 1100);
	EXPECT_EQ(kinah.getItemCount(), 1000 - 24);
	EXPECT_EQ(player().getInventory().getKinah(), 976);
	EXPECT_FALSE(soulSick()) << ":144";
	EXPECT_EQ(commonData().getDeathCount(), 0);
}

TEST_F(DialogServiceTest, AcceptingTheRecoveryWithoutEnoughKinahOnlySaysSo) {
	stored(793003, KINAH, 23);
	commonData().setRecoverableExp(100);
	commonData().setDeathCount(4);
	makeSoulSick();
	select(DialogAction::RECOVERY, npc(PALAEMON));
	clearSent();

	ASSERT_TRUE(player().getResponseRequester().respond(SM_QUESTION_WINDOW::STR_ASK_RECOVER_EXPERIENCE, 1));

	// :139, :146-148: getKinah() >= price is false for 23 < 24
	EXPECT_EQ(sent(), exactly({serializedFor(SM_SYSTEM_MESSAGE::STR_MSG_NOT_ENOUGH_KINA(24))}));
	EXPECT_EQ(commonData().getExpRecoverable(), 100);
	EXPECT_EQ(player().getInventory().getKinah(), 23);
	EXPECT_TRUE(soulSick()) << "the refused heal keeps the Soul Sickness";
	EXPECT_EQ(commonData().getDeathCount(), 4);
}

TEST_F(DialogServiceTest, AcceptingTheRecoveryWithExactlyThePriceSucceeds) {
	stored(793004, KINAH, 24);
	commonData().setRecoverableExp(100);
	select(DialogAction::RECOVERY, npc(PALAEMON));
	clearSent();

	ASSERT_TRUE(player().getResponseRequester().respond(SM_QUESTION_WINDOW::STR_ASK_RECOVER_EXPERIENCE, 1));

	EXPECT_EQ(player().getInventory().getKinah(), 0) << ":139: >= price";
	EXPECT_EQ(commonData().getExpRecoverable(), 0);
}

// ---- onCloseDialog (DialogService.java:53-67) -------------------------------------------------------------------------------------------------

TEST_F(DialogServiceTest, ClosingADialogClosesAnOpenMailbox) {
	Npc& merchant = npc(MINALINERK);
	player().getMailbox()->mailBoxState.set(PlayerMailboxState::EXPRESS);

	DialogService::onCloseDialog(player(), runtime::Ptr<model::gameobjects::VisibleObject>(merchant));

	EXPECT_EQ(player().getMailbox()->mailBoxState.get(), PlayerMailboxState::CLOSED);
	EXPECT_TRUE(sent().empty());
}

TEST_F(DialogServiceTest, ClosingWithoutATargetDoesNothing) {
	player().getMailbox()->mailBoxState.set(PlayerMailboxState::REGULAR);

	DialogService::onCloseDialog(player(), nullptr);

	EXPECT_EQ(player().getMailbox()->mailBoxState.get(), PlayerMailboxState::REGULAR) << ":54-55 returns before the mailbox";
}

TEST_F(DialogServiceTest, ClosingALegionWarehouseDialogWithoutALegionOnlyClosesTheMailbox) {
	Npc& pauton = npc(PAUTON); // func_dialogs 53, OPEN_LEGION_WAREHOUSE
	player().getMailbox()->mailBoxState.set(PlayerMailboxState::REGULAR);
	ASSERT_FALSE(player().isLegionMember());

	// :60-61: supportsAction(OPEN_LEGION_WAREHOUSE) && isLegionMember() - the second operand keeps getLegion()'s null from being dereferenced
	EXPECT_NO_THROW(DialogService::onCloseDialog(player(), runtime::Ptr<model::gameobjects::VisibleObject>(pauton)));

	EXPECT_EQ(player().getMailbox()->mailBoxState.get(), PlayerMailboxState::CLOSED);
	EXPECT_EQ(runtime::unportedHitCount(), 0u) << "LegionWarehouse.unsetInUse (W-13) is not asked";
	EXPECT_TRUE(sent().empty());
}

} // namespace
} // namespace aion::gameserver::network::aion::clientpackets::testing::items
