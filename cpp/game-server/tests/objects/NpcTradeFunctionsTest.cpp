// M5c D-01 (m5c-plan.md §5, P4-11a): the three trade predicates of Npc that SM_TRADELIST and SM_SELL_ITEM ask in their constructors
// (m5c-plan.md W-03: two "0-unported" packets that threw on construction while these were AION_UNPORTED).
//
// Java Npc.java:361-384: canSell = a tradelist_template && BUY (2), canBuy = SELL (3) || canSell, canTradeIn = a trade_in_list_template &&
// TRADE_IN (78), canPurchase = a purchase_template && TRADE_SELL_LIST (103). Every predicate is driven with a real npc row on both sides of its
// `&&`: an npc with the template and the function, one with the function and no template, and one with the template and no function. The npc
// rows are npc_templates.xml rows and the trade rows npc_trade_list.xml rows, copied verbatim (file:line beside each). Two rows carry an
// <equipment> list whose item ids stay unresolved IDREFs here (no item holder is bound, no resolveIdRefs runs): nothing on these paths reads
// the equipment, which Npc only builds on first use (NpcEquippedGear).

#include <gtest/gtest.h>

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "aion/gameserver/configs/main/AIConfig.h"
#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/NpcData.bind.h"
#include "aion/gameserver/dataholders/NpcData.h"
#include "aion/gameserver/dataholders/NpcSkillData.h"
#include "aion/gameserver/dataholders/TradeListData.bind.h"
#include "aion/gameserver/dataholders/TradeListData.h"
#include "aion/gameserver/dataholders/loadingutils/LoadContext.h"
#include "aion/gameserver/dataholders/loadingutils/StaticDataLoader.h"
#include "aion/gameserver/model/gameobjects/Npc.h"
#include "aion/gameserver/model/stats/container/CreatureLifeStats.h"
#include "aion/gameserver/model/stats/container/NpcGameStats.h"
#include "aion/gameserver/model/templates/npc/NpcTemplate.h"
#include "aion/gameserver/model/templates/spawns/SpawnGroup.h"
#include "aion/gameserver/model/templates/spawns/SpawnTemplate.h"
#include "aion/gameserver/runtime/base/TaskInfo.h"
#include "aion/gameserver/runtime/lifetime/Reclaimer.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/TaskScope.h"
#include "aion/gameserver/runtime/sched/Clock.h"
#include "aion/gameserver/runtime/sched/DeterministicExecutor.h"
#include "aion/gameserver/utils/ThreadPoolManager.h"
#include "aion/gameserver/utils/idfactory/IDFactory.h"

namespace aion::gameserver::model::gameobjects {
namespace {

using runtime::Ref;

#define TEST_SCOPE runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST))

/** npc_templates.xml, verbatim rows (the lines of each row in the comment above it) */
constexpr std::string_view NPC_TEMPLATES_XML = R"xml(<npc_templates>
	<!-- :461604-461610, BUY and SELL, with a tradelist_template -->
	<npc_template npc_id="798007" level="9" name="minalinerk" name_id="351126" height="1.16875" title_id="350377" group_drop="NONE" rank="DISCIPLINED" rating="NORMAL" race="BROWNIE" tribe="GENERAL" type="GENERAL" ai="general" srange="20" sangle="240" attack_speed="2100" hpgauge="3">
		<stats maxHp="2568">
			<speeds walk="1.5" group_walk="1.5" run="4.23" run_fight="4.23" group_run_fight="4.23" />
		</stats>
		<bound_radius front="0.595" side="0.3774" upper="1.16875" />
		<talk_info distance="5" is_dialog="true" func_dialogs="2 3" can_talk_invisible="false" />
	</npc_template>
	<!-- :462170-462176, BUY and SELL, no tradelist_template -->
	<npc_template npc_id="798088" level="1" name="mogironerk" name_id="353292" height="1.16875" title_id="350530" group_drop="NONE" rank="DISCIPLINED" rating="NORMAL" race="BROWNIE" tribe="GENERAL_DARK" type="GENERAL" ai="general" srange="20" sangle="240" attack_speed="2000" hpgauge="3">
		<stats maxHp="14535">
			<speeds walk="1.5" group_walk="1.5" run="4.23" run_fight="4.23" group_run_fight="4.23" />
		</stats>
		<bound_radius front="0.595" side="0.3774" upper="1.16875" />
		<talk_info distance="5" is_dialog="true" func_dialogs="2 3" can_talk_invisible="false" />
	</npc_template>
	<!-- :461611-461617, a tradelist_template but only EXTEND_INVENTORY (47) -->
	<npc_template npc_id="798008" level="9" name="baevrunerk" name_id="351141" height="1.16875" title_id="350421" group_drop="NONE" rank="DISCIPLINED" rating="NORMAL" race="BROWNIE" tribe="GENERAL" type="GENERAL" ai="general" srange="20" sangle="240" attack_speed="2100" hpgauge="3">
		<stats maxHp="2568">
			<speeds walk="1.5" group_walk="1.5" run="4.23" run_fight="4.23" group_run_fight="4.23" />
		</stats>
		<bound_radius front="0.595" side="0.3774" upper="1.16875" />
		<talk_info distance="5" is_dialog="true" func_dialogs="47" can_talk_invisible="false" />
	</npc_template>
	<!-- :341797-341803, TRADE_SELL_LIST with a purchase_template -->
	<npc_template npc_id="279058" level="40" name="amarunerk" name_id="314047" height="1.16875" title_id="314359" group_drop="NONE" rank="DISCIPLINED" rating="NORMAL" race="BROWNIE" tribe="USEALL" type="GENERAL" ai="general" srange="20" sangle="300" attack_speed="2000" hpgauge="3">
		<stats maxHp="9419">
			<speeds walk="1.5" group_walk="1.5" run="4.23" run_fight="4.23" group_run_fight="4.23" />
		</stats>
		<bound_radius front="0.595" side="0.3774" upper="1.16875" />
		<talk_info distance="5" is_dialog="true" func_dialogs="103" can_talk_invisible="false" />
	</npc_template>
	<!-- :485469-485483, a purchase_template but only BUY -->
	<npc_template npc_id="800588" level="65" name="dita" name_id="372663" height="2.5" title_id="370482" group_drop="LIGHT" rank="DISCIPLINED" rating="ELITE" race="ELYOS" tribe="GENERAL" type="GENERAL" ai="general" srange="20" arange="2" attack_speed="2100" hpgauge="11" cancel_level="40">
		<stats maxHp="403456">
			<speeds walk="1.457" group_walk="1.457" run="6" run_fight="8" group_run_fight="6" />
		</stats>
		<equipment>
			<item>113601247</item>
			<item>110601293</item>
			<item>100000930</item>
			<item>115001379</item>
			<item>111601258</item>
			<item>114601242</item>
		</equipment>
		<bound_radius front="0.3125" side="0.4375" upper="2.5" />
		<talk_info distance="5" is_dialog="true" func_dialogs="2" can_talk_invisible="false" />
	</npc_template>
	<!-- :501658-501671, TRADE_SELL_LIST without a purchase_template -->
	<npc_template npc_id="801968" level="65" name="epina" name_id="463649" height="2.5" title_id="463648" group_drop="LIGHT" rank="DISCIPLINED" rating="NORMAL" race="ELYOS" tribe="GENERAL" type="GENERAL" ai="general" srange="20" sangle="240" arange="2" attack_speed="2000" hpgauge="3">
		<stats maxHp="26116">
			<speeds walk="1.457" group_walk="1.457" run="6" run_fight="8" group_run_fight="6" />
		</stats>
		<equipment>
			<item>113501413</item>
			<item>110501439</item>
			<item>112501338</item>
			<item>111501396</item>
			<item>114501421</item>
		</equipment>
		<bound_radius front="0.3125" side="0.4375" upper="2.5" />
		<talk_info distance="5" is_dialog="true" func_dialogs="103" can_talk_invisible="false" />
	</npc_template>
	<!-- :487514-487520, TRADE_IN with a trade_in_list_template -->
	<npc_template npc_id="800735" level="60" name="bellmarinerk" name_id="372810" height="1.16875" title_id="463225" group_drop="NONE" rank="DISCIPLINED" rating="NORMAL" race="ELYOS" tribe="GENERAL" type="GENERAL" ai="general" srange="20" arange="2" attack_speed="2000" hpgauge="3">
		<stats maxHp="23691">
			<speeds walk="1.5" group_walk="1.5" run="4.23" run_fight="4.23" group_run_fight="4.23" />
		</stats>
		<bound_radius front="0.595" side="0.3774" upper="1.16875" />
		<talk_info distance="5" is_dialog="true" func_dialogs="78" can_talk_invisible="false" />
	</npc_template>
	<!-- :490432-490438, a trade_in_list_template but no function dialog -->
	<npc_template npc_id="800953" level="1" name="testonlynpc_item08" name_id="462972" height="1.375" group_drop="NONE" rank="DISCIPLINED" rating="NORMAL" race="BROWNIE" tribe="USEALL" type="GENERAL" ai="general" srange="20" arange="2" attack_speed="2000" hpgauge="3">
		<stats maxHp="23691">
			<speeds walk="1.5" group_walk="1.5" run="4.23" run_fight="4.23" group_run_fight="4.23" />
		</stats>
		<bound_radius front="0.7" side="0.444" upper="1.375" />
		<talk_info distance="5" can_talk_invisible="false" />
	</npc_template>
	<!-- :483062-483068, TRADE_IN without a trade_in_list_template -->
	<npc_template npc_id="800371" level="1" name="kyrshaka" name_id="356089" height="2.52" title_id="370493" group_drop="DRAKANFIGHTER" rank="DISCIPLINED" rating="NORMAL" race="BROWNIE" tribe="XDRAKAN_UNATTACK" type="GENERAL" ai="general" srange="20" sangle="300" arange="2" attack_speed="2000" hpgauge="3">
		<stats maxHp="23691">
			<speeds walk="1.5" group_walk="1.5" run="6" run_fight="7" group_run_fight="6" />
		</stats>
		<bound_radius front="0.35" side="0.49" upper="2.52" />
		<talk_info distance="5" is_dialog="true" func_dialogs="78" can_talk_invisible="false" />
	</npc_template>
</npc_templates>)xml";

/** npc_trade_list.xml, verbatim rows */
constexpr std::string_view NPC_TRADE_LIST_XML = R"xml(<npc_trade_list>
	<!-- :2186-2189 -->
	<tradelist_template npc_id="798007" buy_price_rate="200">
		<tradelist id="132" />
		<tradelist id="720" />
	</tradelist_template>
	<!-- :2190-2192 -->
	<tradelist_template npc_id="798008" buy_price_rate="200">
		<tradelist id="132" />
	</tradelist_template>
	<!-- :9211-9213 -->
	<trade_in_list_template npc_id="800735">
		<tradelist id="111" />
	</trade_in_list_template>
	<!-- :9239-9241 -->
	<trade_in_list_template npc_id="800953">
		<tradelist id="153" />
	</trade_in_list_template>
	<!-- :9570-9572 -->
	<purchase_template npc_id="279058" buy_price_rate="150" npc_type="ABYSS">
		<tradelist id="59" />
	</purchase_template>
	<!-- :9600-9604 -->
	<purchase_template npc_id="800588" buy_price_rate="40">
		<tradelist id="76" />
		<tradelist id="77" />
		<tradelist id="78" />
	</purchase_template>
</npc_trade_list>)xml";

/** Life stats with fixed HP, as CreatureBodiesTest's npc has them (the predicates read no stat) */
class FixedLifeStats final : public stats::container::CreatureLifeStats {
public:
	explicit FixedLifeStats(Creature& owner) : CreatureLifeStats(owner, 1000, 100) {}
};

class TradeTestNpc final : public Npc {
	AION_MAKE_REF_FRIEND
public:
	TradeTestNpc(CreateKey key, std::unique_ptr<controllers::NpcController> controller, templates::spawns::SpawnTemplate& spawnTemplate,
		const templates::npc::NpcTemplate* objectTemplate)
		: Npc(key, std::move(controller), spawnTemplate, objectTemplate) {}

protected:
	~TradeTestNpc() override = default;

	void setupStatContainers() override {
		setGameStats(std::make_unique<stats::container::NpcGameStats>(*this));
		setLifeStats(std::make_unique<FixedLifeStats>(*this));
	}
};

class TradeTestSpawnTemplate final : public templates::spawns::SpawnTemplate {
public:
	explicit TradeTestSpawnTemplate(templates::spawns::SpawnGroup& group)
		: SpawnTemplate(group, 10.0f, 20.0f, 30.0f, int8_t{0}, 0, std::nullopt, 0, 0, std::nullopt) {}
};

/** The holders are immortal static data, kept for the process as DataManager keeps its own (a Reclaimer-freed npc never outlives them). */
const dataholders::NpcData& npcData() {
	static const dataholders::NpcData* holder = [] {
		static xml::LoadContext context;
		return xml::bindString<dataholders::NpcData>(context, NPC_TEMPLATES_XML).release();
	}();
	return *holder;
}

class NpcTradeFunctionsTest : public testing::Test {
protected:
	void SetUp() override {
		utils::ThreadPoolManager::installBackend(nullptr);
		utils::ThreadPoolManager::installBackend(std::make_unique<runtime::DeterministicExecutor>(clock, 7));
		utils::idfactory::IDFactory::getInstance().resetForTests();
		dataholders::DataManager::NPC_SKILL_DATA.publish(std::make_unique<dataholders::NpcSkillData>());
		// the rows name ai="general", whose handler this executable does not link: the warn mode gives each npc AIEngine's substitute
		savedMissingAiHandlers = configs::main::AIConfig::MISSING_AI_HANDLERS.get();
		configs::main::AIConfig::MISSING_AI_HANDLERS.set("warn");
		xml::LoadContext context;
		dataholders::DataManager::TRADE_LIST_DATA.publish(xml::bindString<dataholders::TradeListData>(context, NPC_TRADE_LIST_XML));
	}

	void TearDown() override {
		runtime::Reclaimer::getInstance().drain();
		utils::ThreadPoolManager::installBackend(nullptr);
		runtime::Reclaimer::getInstance().drain();
		dataholders::DataManager::TRADE_LIST_DATA.resetForTests();
		dataholders::DataManager::NPC_SKILL_DATA.resetForTests();
		if (savedMissingAiHandlers)
			configs::main::AIConfig::MISSING_AI_HANDLERS.set(*savedMissingAiHandlers);
	}

	/** A spawned-shape npc of the row with this npc id (Java: VisibleObjectSpawner.spawnNpc builds it from the template) */
	Ref<TradeTestNpc> npc(int32_t npcId) {
		const templates::npc::NpcTemplate* objectTemplate = npcData().getNpcTemplate(npcId);
		EXPECT_NE(objectTemplate, nullptr) << npcId;
		Ref<templates::spawns::SpawnGroup> group = templates::spawns::SpawnGroup::create(210010000, npcId, 0, nullptr);
		templates::spawns::SpawnTemplate& spawn = group->addSpawnTemplate(std::make_unique<TradeTestSpawnTemplate>(*group));
		groups.push_back(group);
		return VisibleObject::create<TradeTestNpc>(std::make_unique<controllers::NpcController>(), spawn, objectTemplate);
	}

	runtime::ManualClock clock{0};
	std::shared_ptr<const std::string> savedMissingAiHandlers;
	std::vector<Ref<templates::spawns::SpawnGroup>> groups;
};

TEST_F(NpcTradeFunctionsTest, CanSellNeedsATradeListAndTheBuyFunction) {
	TEST_SCOPE;
	// Java Npc.java:361-363: TRADE_LIST_DATA.getTradeListTemplate(npcId) != null && supportsAction(BUY)
	EXPECT_TRUE(npc(798007)->canSell()) << "minalinerk: tradelist_template :2186 and func_dialogs 2";
	EXPECT_FALSE(npc(798088)->canSell()) << "mogironerk: func_dialogs 2, but no tradelist_template";
	EXPECT_FALSE(npc(798008)->canSell()) << "baevrunerk: tradelist_template :2190, but only func_dialogs 47";
	EXPECT_FALSE(npc(800735)->canSell()) << "a trade-in npc has no tradelist_template";
	groups.clear();
}

TEST_F(NpcTradeFunctionsTest, CanBuyIsTheSellFunctionOrCanSell) {
	TEST_SCOPE;
	// Java Npc.java:368-370: supportsAction(SELL) || canSell() - the ported body, now that its right operand answers
	EXPECT_TRUE(npc(798007)->canBuy());
	EXPECT_TRUE(npc(798088)->canBuy()) << "SELL alone is enough";
	EXPECT_FALSE(npc(798008)->canBuy()) << "neither SELL nor canSell";
	EXPECT_FALSE(npc(279058)->canBuy()) << "a purchase npc buys through canPurchase, not canBuy";
	groups.clear();
}

TEST_F(NpcTradeFunctionsTest, CanTradeInNeedsATradeInListAndTheTradeInFunction) {
	TEST_SCOPE;
	// Java Npc.java:375-377: TRADE_LIST_DATA.getTradeInListTemplate(npcId) != null && supportsAction(TRADE_IN)
	EXPECT_TRUE(npc(800735)->canTradeIn()) << "bellmarinerk: trade_in_list_template :9211 and func_dialogs 78";
	EXPECT_FALSE(npc(800371)->canTradeIn()) << "kyrshaka: func_dialogs 78, but no trade_in_list_template";
	EXPECT_FALSE(npc(800953)->canTradeIn()) << "testonlynpc_item08: trade_in_list_template :9239, but no function dialog";
	EXPECT_FALSE(npc(798007)->canTradeIn()) << "a merchant's tradelist_template is not a trade-in list";
	groups.clear();
}

TEST_F(NpcTradeFunctionsTest, CanPurchaseNeedsAPurchaseTemplateAndTheTradeSellListFunction) {
	TEST_SCOPE;
	// Java Npc.java:382-384: TRADE_LIST_DATA.getPurchaseTemplate(npcId) != null && supportsAction(TRADE_SELL_LIST)
	EXPECT_TRUE(npc(279058)->canPurchase()) << "amarunerk: purchase_template :9570 and func_dialogs 103";
	EXPECT_FALSE(npc(801968)->canPurchase()) << "epina: func_dialogs 103, but no purchase_template";
	EXPECT_FALSE(npc(800588)->canPurchase()) << "dita: purchase_template :9600, but only func_dialogs 2";
	EXPECT_FALSE(npc(798007)->canPurchase()) << "a merchant's tradelist_template is not a purchase list";
	groups.clear();
}

} // namespace
} // namespace aion::gameserver::model::gameobjects
