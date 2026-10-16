// M5d E-10 (m5d-plan.md §7, §18.3, D19; the dialog-and-rewards lane's file lease on model/templates/item/actions/{QuestStartAction,ReadAction}.*
// of P5-07): the two quest-item actions, against QuestStartAction.java:34-88 and ReadAction.java:21-66 - canAct, act, the private finishUse
// and the anonymous ItemUseObserver of each (the use bar).
// - QuestStartAction: canAct is always true (the cast always plays). act without a casting delay finishes at once; with one it broadcasts the
//   cast animation (unk 1), attaches the observer and schedules the finish. finishUse starts the cooldown, broadcasts the use animation, then:
//   (a) a quest held and not startable again (START, or COMPLETE without repeats) -> only STR_USE_ITEM, silently; (b) a restriction ->
//   QuestService.checkStartConditions(..., warn = true, ...) sends its message, then STR_USE_ITEM (the <start_conditions> of quest_data.xml
//   are checked too, and a failed one is silent); otherwise - no state, or a COMPLETE quest with repeats left - STR_USE_ITEM and
//   QuestEngine.onItemUseEvent, and QuestEngine.onDialog(ASK_QUEST_ACCEPT) unless the handler of a registered quest item answered SUCCESS.
//   The observer's abort cancels the task, sends STR_ITEM_CANCELED and the aborted animation (end 2).
// - ReadAction: canAct is always true. act does nothing for an item that also has <queststart> (QuestStartAction plays its use); else it
//   finishes at once or after the cast (animation unk 0). finishUse: the cooldown, STR_USE_ITEM, the read animation (end 1). The observer's
//   abort also removes itself.
// Every animation of both actions is broadcast (the cast, the finish and the abort: PacketSendUtility.broadcastPacket(player, packet, true) or
// broadcastPacketAndReceive), so the cast cases put a watcher in the player's known list and read his queue too.
// The quest engine is the singleton: the cases that need a handler add a probe for their quest (quest handlers are Immortal, so its record is
// static) and clear the engine again, as tests/cm_lz/AscensionPacketsTest.cpp does. CM_USE_ITEM's side - it skips onItemUseEvent for a
// <queststart> item, so the handler is asked once, after the cast - is tests/cm_lz/UseQuestItemPacketTest.cpp.
//
// Every row is the shipped data's, verbatim (file:line beside each): the items of items/item_templates.xml (the fixture's rows, plus the ones
// below; Odium Refining Method, <queststart> and <read> together, is the fixture's already) and the quests of quest_data/quest_data.xml.
// Expected packets are Java's bytes where the action chooses the fields (SM_ITEM_USAGE_ANIMATION.java); the messages are compared with the
// server's own serialization of the message Java builds (their bytes are pinned by the sm tests).

#include "ItemServicesTestSupport.h"

#include <chrono>
#include <cstdint>
#include <memory>
#include <optional>
#include <ostream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "aion/gameserver/controllers/ObserveController.h"
#include "aion/gameserver/controllers/PlayerController.h"
#include "aion/gameserver/dataholders/PlayerExperienceTable.bind.h"
#include "aion/gameserver/dataholders/PlayerExperienceTable.h"
#include "aion/gameserver/dataholders/QuestsData.bind.h"
#include "aion/gameserver/dataholders/QuestsData.h"
#include "aion/gameserver/model/DialogAction.h"
#include "aion/gameserver/model/Race.h"
#include "aion/gameserver/model/TaskId.h"
#include "aion/gameserver/model/gameobjects/Persistable.h"
#include "aion/gameserver/model/gameobjects/player/PlayerCommonData.h"
#include "aion/gameserver/model/gameobjects/player/QuestStateList.h"
#include "aion/gameserver/model/templates/item/ItemTemplate.h"
#include "aion/gameserver/model/templates/item/actions/ItemActions.h"
#include "aion/gameserver/model/templates/item/actions/QuestStartAction.h"
#include "aion/gameserver/model/templates/item/actions/ReadAction.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/questEngine/QuestEngine.h"
#include "aion/gameserver/questEngine/handlers/AbstractQuestHandler.h"
#include "aion/gameserver/questEngine/handlers/HandlerResult.h"
#include "aion/gameserver/questEngine/model/QuestEnv.h"
#include "aion/gameserver/questEngine/model/QuestState.h"
#include "aion/gameserver/questEngine/model/QuestStatus.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/services/cron/CronService.h"
#include "aion/gameserver/utils/cron/ThreadPoolManagerRunnableRunner.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::services::item::test {
namespace {

using namespace std::chrono_literals;
using model::templates::item::actions::QuestStartAction;
using model::templates::item::actions::ReadAction;
using network::aion::serverpackets::SM_SYSTEM_MESSAGE;
using questEngine::QuestEngine;
using questEngine::handlers::HandlerResult;
using questEngine::model::QuestState;
using questEngine::model::QuestStatus;
namespace DialogAction = model::DialogAction;

constexpr int32_t PLAYER_OBJECT_ID = 700101; // ItemServicesTest's player

constexpr int32_t BROKEN_AXE_HANDLE = 182200501; // <queststart questid="1107"/>, no casting delay, usedelayid 61
constexpr int32_t RUSTED_SPEAR = 182205668; // <queststart questid="2718"/>, no casting delay, usedelayid 41
constexpr int32_t VORGALTEM_SECRET_ORDER = 182206842; // <queststart questid="11056"/>, casting_delay 2000, usedelayid 61
constexpr int32_t POISON_RESEARCH_DIARY = 182200016; // <read/>, no casting delay, usedelayid 61
constexpr int32_t INVASION_BRIEF = 182215452; // <read/>, casting_delay 2000, usedelayid 41

// the quests of the start items: 1107 (minlevel 3, ELYOS, max_repeat_count 1), 2718 (minlevel 25, ASMODIANS, max_repeat_count 255), 11056
// (minlevel 54, ELYOS, not while 11057 or 11058 is held) and Odium Refining Method's 1197 (minlevel 14, ELYOS)
constexpr int32_t THE_LOST_AXE = 1107;
constexpr int32_t TRADING_DOWN = 2718;
constexpr int32_t ELIMINATION_ORDER = 11056;
constexpr int32_t STANISS_SECRET_ORDER = 11057;

/** items/item_templates.xml, verbatim rows */
constexpr std::string_view QUEST_ITEM_ROWS = R"xml(
	<!-- :877018 -->
	<item_template id="182200501" name="Broken Axe Handle" level="2" cName="quest_1107a" mask="20545" item_group="QUEST" quality="COMMON" price="1" desc="1105001" activate_target="STANDALONE" activate_count="1000">
		<actions>
			<queststart questid="1107"/>
		</actions>
		<uselimits usedelay="15000" usedelayid="61"/>
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
	<!-- :882004 -->
	<item_template id="182206842" name="Vorgaltem Secret Order" level="1" cName="quest_11056a" casting_delay="2000" mask="20545" item_group="QUEST" quality="EPIC" price="1" race="ELYOS" desc="1130786" activate_target="STANDALONE" activate_count="1000">
		<actions>
			<queststart questid="11056"/>
		</actions>
		<uselimits usedelay="2000" usedelayid="61"/>
		<inventory id="2"/>
	</item_template>
	<!-- :876881 -->
	<item_template id="182200016" name="Poison Research Diary" level="1" cName="doc_quest_1016d" mask="20545" quality="COMMON" price="1" desc="1106031" activate_target="STANDALONE" activate_count="1000">
		<actions>
			<read/>
		</actions>
		<uselimits usedelay="2000" usedelayid="61"/>
		<inventory id="2"/>
	</item_template>
	<!-- :889516 -->
	<item_template id="182215452" name="Invasion Brief" level="1" cName="doc_quest_10101a" casting_delay="2000" mask="28736" max_stack_count="20" quality="COMMON" price="1" race="ELYOS" desc="1131200" activate_target="STANDALONE" activate_count="1000">
		<actions>
			<read/>
		</actions>
		<uselimits usedelay="2000" usedelayid="41"/>
		<inventory id="2"/>
	</item_template>
)xml";

/** quest_data/quest_data.xml, verbatim rows */
constexpr std::string_view QUEST_DATA_XML = R"xml(<quests>
	<!-- :937-942 -->
	<quest id="1107" name="The Lost Axe" nameId="1102207" quest_zone="Poeta" minlevel_permitted="3" max_repeat_count="1" can_report="true" cannot_share="true" race_permitted="ELYOS" category="QUEST">
		<rewards gold="1560" exp="462"/>
		<quest_work_items>
			<quest_work_item item_id="182200501"/>
		</quest_work_items>
	</quest>
	<!-- :1675-1680 -->
	<quest id="1197" name="Krall Book" nameId="1102367" quest_zone="Verteron" minlevel_permitted="14" max_repeat_count="1" cannot_share="true" race_permitted="ELYOS" category="QUEST">
		<rewards gold="2860" exp="22275"/>
		<quest_work_items>
			<quest_work_item item_id="182200558"/>
		</quest_work_items>
	</quest>
	<!-- :15825-15836 -->
	<quest id="2718" name="Trading Down" nameId="1104818" quest_zone="Reshanta" minlevel_permitted="25" max_repeat_count="255" reward_repeat_count="5" cannot_share="true" race_permitted="ASMODIANS" category="QUEST" restricted="true">
		<rewards exp="1476088" ap="50">
			<reward_item item_id="182005204" count="1"/>
		</rewards>
		<extended_rewards title="95"/>
		<quest_work_items>
			<quest_work_item item_id="182205668"/>
			<quest_work_item item_id="182205669"/>
			<quest_work_item item_id="182205670"/>
			<quest_work_item item_id="182205671"/>
		</quest_work_items>
	</quest>
	<!-- :35067-35084 -->
	<quest id="11056" name="[Spy/Alliance] Elimination Order" nameId="1125056" quest_zone="Inggison" minlevel_permitted="54" max_repeat_count="1" cannot_share="true" race_permitted="ELYOS" category="QUEST" target="ALLIANCE" restricted="true">
		<collect_items>
			<collect_item item_id="182400001" count="5000000"/>
		</collect_items>
		<rewards exp="10758569">
			<selectable_reward_item item_id="121000993" count="1"/>
			<selectable_reward_item item_id="121000994" count="1"/>
		</rewards>
		<start_conditions>
			<noacquired>11057</noacquired>
		</start_conditions>
		<start_conditions>
			<noacquired>11058</noacquired>
		</start_conditions>
		<quest_work_items>
			<quest_work_item item_id="182206842" count="1"/>
		</quest_work_items>
	</quest>
</quests>)xml";

/** The first 56 levels of player_experience_table.xml (the base fixture's 16 end below the quests of 2718 and 11056) */
constexpr std::string_view PLAYER_EXPERIENCE_TABLE_TO_55_XML =
	"<player_experience_table>"
	"<exp>0</exp><exp>400</exp><exp>1433</exp><exp>3820</exp><exp>9054</exp><exp>17655</exp><exp>30978</exp><exp>52010</exp><exp>82982</exp>"
	"<exp>126069</exp><exp>182252</exp><exp>260622</exp><exp>360825</exp><exp>490331</exp><exp>649169</exp><exp>844378</exp>"
	"<exp>1083018</exp><exp>1401356</exp><exp>1808613</exp><exp>2314771</exp><exp>2941893</exp><exp>3769257</exp><exp>4811154</exp>"
	"<exp>6110198</exp><exp>7632340</exp><exp>9377726</exp><exp>11395643</exp><exp>13731725</exp><exp>16339413</exp><exp>19378549</exp>"
	"<exp>23162749</exp><exp>27585843</exp><exp>32841197</exp><exp>39127217</exp><exp>47350762</exp><exp>57829684</exp><exp>70654362</exp>"
	"<exp>87571065</exp><exp>107018757</exp><exp>129815732</exp><exp>157211282</exp><exp>189272188</exp><exp>226933751</exp>"
	"<exp>267247400</exp><exp>310053925</exp><exp>355815203</exp><exp>404823687</exp><exp>456685353</exp><exp>511683757</exp>"
	"<exp>570162075</exp><exp>632268545</exp><exp>701585822</exp><exp>776831823</exp><exp>857090855</exp><exp>947120930</exp>"
	"<exp>1051346275</exp>"
	"</player_experience_table>";

/** What a quest handler was asked: the hook, the QuestEnv's quest and dialog action, and the item used (0 for the dialog) */
struct HandlerCall {
	std::string hook;
	int32_t questId = 0;
	int32_t dialogActionId = 0;
	int32_t itemObjId = 0;

	bool operator==(const HandlerCall&) const = default;
};

std::ostream& operator<<(std::ostream& os, const HandlerCall& call) {
	return os << call.hook << "(quest " << call.questId << ", action " << call.dialogActionId << ", item " << call.itemObjId << ")";
}

/**
 * A quest handler that records its onItemUseEvent and onDialogEvent calls (quest handlers are Immortal: QuestEngine keeps them, so the record
 * and the answer are static). register_ registers the start item as a quest item (AbstractQuestHandler's registerQuestItem pattern) when
 * the case asks for it.
 */
class QuestItemProbe final : public questEngine::handlers::AbstractQuestHandler {
public:
	QuestItemProbe(int32_t questId, std::optional<int32_t> startItem) : AbstractQuestHandler(questId), startItem(startItem) {}

	static inline std::vector<HandlerCall> calls;
	static inline HandlerResult itemUseAnswer = HandlerResult::UNKNOWN;

	void register_() override {
		if (startItem)
			QuestEngine::getInstance().registerQuestItem(*startItem, getQuestId());
	}

	HandlerResult onItemUseEvent(questEngine::model::QuestEnv& env, model::gameobjects::Item& item) override {
		calls.push_back({"onItemUseEvent", env.getQuestId(), env.getDialogActionId(), item.getObjectId()});
		return itemUseAnswer;
	}

	bool onDialogEvent(questEngine::model::QuestEnv& env) override {
		calls.push_back({"onDialogEvent", env.getQuestId(), env.getDialogActionId(), 0});
		return true;
	}

private:
	const std::optional<int32_t> startItem;
};

/** `base` (the fixture's <item_templates>) with `rows` appended before its closing tag */
std::string withRows(std::string_view base, std::string_view rows) {
	std::string xml(base);
	xml.insert(xml.rfind("</item_templates>"), rows);
	return xml;
}

class QuestItemActionsTest : public ItemServicesTest {
protected:
	void SetUp() override {
		ItemServicesTest::SetUp();
		xml::LoadContext context;
		dataholders::DataManager::ITEM_DATA.resetForTests(); // the base rows, republished with the quest items
		dataholders::DataManager::ITEM_DATA.publish(xml::bindString<dataholders::ItemData>(context, withRows(ITEM_TEMPLATES_XML, QUEST_ITEM_ROWS)));
		dataholders::DataManager::QUEST_DATA.publish(xml::bindString<dataholders::QuestsData>(context, QUEST_DATA_XML));
		// Java PlayerService.loadPlayer: the quest states (PlayerQuestListDAO.load: none for this character)
		player().setQuestStateList(model::gameobjects::player::QuestStateList::create());
		QuestItemProbe::calls.clear();
		QuestItemProbe::itemUseAnswer = HandlerResult::UNKNOWN;
	}

	void TearDown() override {
		if (handlerAdded) {
			QuestEngine::getInstance().clear(); // the handlers themselves stay (Immortal), unreachable
			services::cron::CronService::resetForTests();
		}
		if (f.player)
			f.player->getController().cancelTask(model::TaskId::ITEM_USE);
		if (watcher.player)
			watcher.player->setClientConnection(nullptr);
		watcherClient.reset();
		watcher = {};
		ItemServicesTest::TearDown();
		dataholders::DataManager::QUEST_DATA.resetForTests();
	}

	/**
	 * Adds the probe of quest 1107 to the QuestEngine singleton. QuestEngine.clear cancels the daily message through the CronService, so it is
	 * set up here as QuestEngineTest's fixture does
	 */
	void addProbe(std::optional<int32_t> startItem) { addProbe(THE_LOST_AXE, startItem); }

	/** Adds the probe of `questId` (see addProbe(startItem)) */
	void addProbe(int32_t questId, std::optional<int32_t> startItem) {
		services::cron::CronService::resetForTests();
		services::cron::CronService::initSingleton(std::make_unique<utils::cron::ThreadPoolManagerRunnableRunner>(), std::chrono::locate_zone("UTC"),
			services::cron::CronService::Driver::EXECUTOR);
		handlerAdded = true;
		QuestEngine::getInstance().addQuestHandler(std::make_unique<QuestItemProbe>(questId, startItem));
		ASSERT_TRUE(QuestEngine::getInstance().isHaveHandler(questId));
	}

	/** A second character in the player's known list with a connection of his own (CraftLearnActionTest's watcher): a broadcast reaches him */
	void watch() {
		watcher = cp::makePlayer(700102, 9802, "Watcher");
		watcherClient = std::make_unique<cp::TestClient>();
		watcherClient->enterWorld(watcher);
		ASSERT_TRUE(f.knownList().addForTest(*watcher.player));
		clearSent(); // the player's SM_PLAYER_INFO of the watcher
		(*watcherClient)->clearSent();
	}

	/** The player at `level`, which the base fixture's experience table (16 levels) cannot hold: the table republished with 56 */
	void setHighLevel(int32_t level) {
		dataholders::DataManager::PLAYER_EXPERIENCE_TABLE.resetForTests(); // the base TearDown resets this one
		xml::LoadContext context;
		dataholders::DataManager::PLAYER_EXPERIENCE_TABLE.publish(
			xml::bindString<dataholders::PlayerExperienceTable>(context, PLAYER_EXPERIENCE_TABLE_TO_55_XML));
		f.commonData->setLevel(level);
	}

	std::vector<std::vector<uint8_t>> watcherSent() { return (*watcherClient)->sentBytes(); }

	void clearWatcherSent() { (*watcherClient)->clearSent(); }

	/** A quest state as PlayerQuestListDAO loads it (stored), in the player's quest list */
	void hold(int32_t questId, QuestStatus status, int32_t completeCount) {
		runtime::Ref<QuestState> qs = QuestState::create(questId, status, 0, 0, completeCount, std::nullopt, std::nullopt, std::nullopt);
		qs->setPersistentState(model::gameobjects::Persistable::PersistentState::UPDATED);
		player().getQuestStateList()->addQuest(questId, *qs);
	}

	const model::templates::item::ItemTemplate& templateOf(int32_t itemId) {
		const model::templates::item::ItemTemplate* itemTemplate = dataholders::DataManager::ITEM_DATA->getItemTemplate(itemId);
		if (itemTemplate == nullptr || itemTemplate->getActions() == nullptr)
			throw runtime::NullPointerException("no actions of item " + std::to_string(itemId));
		return *itemTemplate;
	}

	/** The item's bound action of class A (its <queststart> or <read>) */
	template <class A>
	const A& actionOf(int32_t itemId) {
		for (const auto& action : templateOf(itemId).getActions()->getItemActions()) {
			if (const auto* typed = dynamic_cast<const A*>(action.get()))
				return *typed;
		}
		throw runtime::NullPointerException("item " + std::to_string(itemId) + " has no such action");
	}

	/** SM_ITEM_USAGE_ANIMATION(player, item, itemId) (SM_ITEM_USAGE_ANIMATION.java: time 0, end 1, unk3 1), the finish of a quest-start use */
	static std::vector<uint8_t> useAnimation(int32_t itemObjId, int32_t itemId) { return itemUsageAnimation(PLAYER_OBJECT_ID, itemObjId, itemId, 0, 1, 1); }

	std::vector<uint8_t> message(SM_SYSTEM_MESSAGE&& packet) { return serialized(std::move(packet)); }

	bool coolsDown(int32_t delayId) { return player().getItemReuseTime(delayId) != 0; }

	bool handlerAdded = false;
	cp::PlayerFixture watcher;
	std::unique_ptr<cp::TestClient> watcherClient;
};

// ---- QuestStartAction ----------------------------------------------------------------------------------------------------------------------

TEST_F(QuestItemActionsTest, BothActionsAlwaysLetTheItemBeUsedAndSayNothing) {
	Item& axe = stored(840001, BROKEN_AXE_HANDLE, 1);
	Item& diary = stored(840002, POISON_RESEARCH_DIARY, 1);

	// QuestStartAction.java:35-38 ("Retail always plays the cast"), ReadAction.java:21-24 - even for a player who cannot start the quest
	EXPECT_TRUE(actionOf<QuestStartAction>(BROKEN_AXE_HANDLE).canAct(player(), Ptr<Item>(axe), nullptr));
	EXPECT_TRUE(actionOf<ReadAction>(POISON_RESEARCH_DIARY).canAct(player(), Ptr<Item>(diary), nullptr));
	EXPECT_TRUE(sent().empty());
	EXPECT_FALSE(coolsDown(61));
}

TEST_F(QuestItemActionsTest, AStartItemWithoutACastAsksTheQuestAtOnce) {
	f.commonData->setLevel(3);
	addProbe(std::nullopt); // a handler for 1107, but the axe is no registered quest item
	Item& axe = stored(840001, BROKEN_AXE_HANDLE, 1);

	actionOf<QuestStartAction>(BROKEN_AXE_HANDLE).act(player(), Ptr<Item>(axe), nullptr);

	// :43-45 -> finishUse: the cooldown (usedelayid 61), the use animation to all and himself, STR_USE_ITEM (:69-77); then onItemUseEvent finds
	// no handler for the item (UNKNOWN) and onDialog(ASK_QUEST_ACCEPT) reaches 1107's (:84-87)
	EXPECT_TRUE(coolsDown(61));
	EXPECT_EQ(sent(), cp::exactly({useAnimation(840001, BROKEN_AXE_HANDLE), message(SM_SYSTEM_MESSAGE::STR_USE_ITEM(axe.getL10n()))}));
	EXPECT_EQ(QuestItemProbe::calls, (std::vector<HandlerCall>{{"onDialogEvent", THE_LOST_AXE, DialogAction::ASK_QUEST_ACCEPT, 0}}));
}

TEST_F(QuestItemActionsTest, ARegisteredStartItemIsHandedToItsHandlerBeforeTheDialog) {
	f.commonData->setLevel(3);
	addProbe(BROKEN_AXE_HANDLE); // register_: registerQuestItem(182200501, 1107)
	Item& axe = stored(840001, BROKEN_AXE_HANDLE, 1);
	const HandlerCall itemUse{"onItemUseEvent", THE_LOST_AXE, DialogAction::ASK_QUEST_ACCEPT, 840001};

	// :84-87: the handler's answer SUCCESS ends it
	QuestItemProbe::itemUseAnswer = HandlerResult::SUCCESS;
	actionOf<QuestStartAction>(BROKEN_AXE_HANDLE).act(player(), Ptr<Item>(axe), nullptr);
	EXPECT_EQ(QuestItemProbe::calls, (std::vector<HandlerCall>{itemUse}));

	// any other answer goes on to the generic dialog routing
	QuestItemProbe::calls.clear();
	QuestItemProbe::itemUseAnswer = HandlerResult::FAILED;
	actionOf<QuestStartAction>(BROKEN_AXE_HANDLE).act(player(), Ptr<Item>(axe), nullptr);
	EXPECT_EQ(QuestItemProbe::calls,
		(std::vector<HandlerCall>{itemUse, {"onDialogEvent", THE_LOST_AXE, DialogAction::ASK_QUEST_ACCEPT, 0}}));
}

TEST_F(QuestItemActionsTest, AQuestHeldAndNotStartableAgainIsSilent) {
	f.commonData->setLevel(3);
	addProbe(BROKEN_AXE_HANDLE);
	Item& axe = stored(840001, BROKEN_AXE_HANDLE, 1);
	const std::vector<std::vector<uint8_t>> silent = {useAnimation(840001, BROKEN_AXE_HANDLE), message(SM_SYSTEM_MESSAGE::STR_USE_ITEM(axe.getL10n()))};

	// (a), :74-80: a quest in progress is not startable (QuestState.isStartable: COMPLETE and canRepeat), so checkStartConditions is not asked
	// and nothing warns - checkStartConditions would have sent STR_QUEST_ACQUIRE_ERROR_WORKING_QUEST without :75's skipStartedCheck
	hold(THE_LOST_AXE, QuestStatus::START, 0);
	actionOf<QuestStartAction>(BROKEN_AXE_HANDLE).act(player(), Ptr<Item>(axe), nullptr);
	EXPECT_EQ(sent(), silent);

	// and a quest completed as often as it may be (max_repeat_count 1)
	clearSent();
	player().getQuestStateList()->deleteQuest(THE_LOST_AXE);
	hold(THE_LOST_AXE, QuestStatus::COMPLETE, 1);
	actionOf<QuestStartAction>(BROKEN_AXE_HANDLE).act(player(), Ptr<Item>(axe), nullptr);
	EXPECT_EQ(sent(), silent);
	EXPECT_TRUE(QuestItemProbe::calls.empty()) << "neither onItemUseEvent nor onDialog";
}

TEST_F(QuestItemActionsTest, AQuestCompletedWithRepeatsLeftIsAskedAgain) {
	// an Asmodian of level 25 who completed 2718 five times (max_repeat_count 255: no limit, QuestState.canRepeat)
	f.commonData->setRace(model::Race::ASMODIANS);
	setHighLevel(25);
	addProbe(TRADING_DOWN, RUSTED_SPEAR);
	Item& spear = stored(840006, RUSTED_SPEAR, 1);
	hold(TRADING_DOWN, QuestStatus::COMPLETE, 5);

	actionOf<QuestStartAction>(RUSTED_SPEAR).act(player(), Ptr<Item>(spear), nullptr);

	// :74-75: the quest is held but startable again (QuestState.isStartable: COMPLETE and canRepeat), so checkStartConditions is asked, and it
	// passes; then STR_USE_ITEM, onItemUseEvent and, for the handler's UNKNOWN, onDialog(ASK_QUEST_ACCEPT) (:84-87)
	EXPECT_TRUE(coolsDown(41));
	EXPECT_EQ(sent(), cp::exactly({useAnimation(840006, RUSTED_SPEAR), message(SM_SYSTEM_MESSAGE::STR_USE_ITEM(spear.getL10n()))}));
	EXPECT_EQ(QuestItemProbe::calls, (std::vector<HandlerCall>{{"onItemUseEvent", TRADING_DOWN, DialogAction::ASK_QUEST_ACCEPT, 840006},
										  {"onDialogEvent", TRADING_DOWN, DialogAction::ASK_QUEST_ACCEPT, 0}}));
}

TEST_F(QuestItemActionsTest, AFailedStartConditionOfTheQuestDataIsSilent) {
	// an Elyos of level 54 (race and level pass) who holds 11057: 11056's first <start_conditions> (noacquired 11057) fails. :75 asks
	// checkStartConditions with skipXmlPreconditionCheck false, so it answers false; XMLStartCondition.check warns only about equipped items
	setHighLevel(54);
	addProbe(ELIMINATION_ORDER, VORGALTEM_SECRET_ORDER);
	Item& order = stored(840003, VORGALTEM_SECRET_ORDER, 1);
	hold(STANISS_SECRET_ORDER, QuestStatus::START, 0);

	actionOf<QuestStartAction>(VORGALTEM_SECRET_ORDER).act(player(), Ptr<Item>(order), nullptr);
	executor->advance(2000ms);

	EXPECT_EQ(sent(), cp::exactly({itemUsageAnimation(PLAYER_OBJECT_ID, 840003, VORGALTEM_SECRET_ORDER, 2000, 0, 1),
						  useAnimation(840003, VORGALTEM_SECRET_ORDER), message(SM_SYSTEM_MESSAGE::STR_USE_ITEM(order.getL10n()))}));
	EXPECT_TRUE(QuestItemProbe::calls.empty()) << "neither onItemUseEvent nor onDialog";

	// without 11057 both conditions hold, and the same use asks the quest
	clearSent();
	player().getQuestStateList()->deleteQuest(STANISS_SECRET_ORDER);
	actionOf<QuestStartAction>(VORGALTEM_SECRET_ORDER).act(player(), Ptr<Item>(order), nullptr);
	executor->advance(2000ms);
	EXPECT_EQ(QuestItemProbe::calls, (std::vector<HandlerCall>{{"onItemUseEvent", ELIMINATION_ORDER, DialogAction::ASK_QUEST_ACCEPT, 840003},
										  {"onDialogEvent", ELIMINATION_ORDER, DialogAction::ASK_QUEST_ACCEPT, 0}}));
}

TEST_F(QuestItemActionsTest, ARestrictedQuestIsWarnedAboutBeforeTheUseMessage) {
	addProbe(BROKEN_AXE_HANDLE);
	Item& axe = stored(840001, BROKEN_AXE_HANDLE, 1);

	// (b), :75-80: no state, so checkStartConditions(player, 1107, warn = true, ...) - level 1 is below minlevel_permitted 3
	actionOf<QuestStartAction>(BROKEN_AXE_HANDLE).act(player(), Ptr<Item>(axe), nullptr);
	EXPECT_EQ(sent(), cp::exactly({useAnimation(840001, BROKEN_AXE_HANDLE), message(SM_SYSTEM_MESSAGE::STR_QUEST_ACQUIRE_ERROR_MIN_LEVEL(3)),
						  message(SM_SYSTEM_MESSAGE::STR_USE_ITEM(axe.getL10n()))}));

	// the race comes first (QuestService.java: race_permitted ELYOS) - an Asmodian of level 3
	clearSent();
	f.commonData->setLevel(3);
	f.commonData->setRace(model::Race::ASMODIANS);
	actionOf<QuestStartAction>(BROKEN_AXE_HANDLE).act(player(), Ptr<Item>(axe), nullptr);
	EXPECT_EQ(sent(), cp::exactly({useAnimation(840001, BROKEN_AXE_HANDLE), message(SM_SYSTEM_MESSAGE::STR_QUEST_ACQUIRE_ERROR_RACE()),
						  message(SM_SYSTEM_MESSAGE::STR_USE_ITEM(axe.getL10n()))}));
	EXPECT_TRUE(QuestItemProbe::calls.empty());
}

TEST_F(QuestItemActionsTest, AStartItemWithACastFinishesOnlyAfterIt) {
	Item& order = stored(840003, VORGALTEM_SECRET_ORDER, 1);
	watch();

	actionOf<QuestStartAction>(VORGALTEM_SECRET_ORDER).act(player(), Ptr<Item>(order), nullptr);

	// :47-48: the cast animation (casting_delay 2000, end 0, unk 1) to all and himself; no cooldown, no message yet
	const std::vector<uint8_t> cast = itemUsageAnimation(PLAYER_OBJECT_ID, 840003, VORGALTEM_SECRET_ORDER, 2000, 0, 1);
	EXPECT_EQ(sent(), cp::exactly({cast}));
	EXPECT_EQ(watcherSent(), cp::exactly({cast}));
	EXPECT_FALSE(coolsDown(61));
	clearSent();
	clearWatcherSent();
	executor->advance(1999ms);
	EXPECT_TRUE(sent().empty());

	// :61-64: the task removes the observer and finishes; 11056 wants level 54 (b). The watcher sees the use animation (:70), not the messages
	executor->advance(1ms);
	EXPECT_TRUE(coolsDown(61));
	EXPECT_EQ(sent(), cp::exactly({useAnimation(840003, VORGALTEM_SECRET_ORDER), message(SM_SYSTEM_MESSAGE::STR_QUEST_ACQUIRE_ERROR_MIN_LEVEL(54)),
						  message(SM_SYSTEM_MESSAGE::STR_USE_ITEM(order.getL10n()))}));
	EXPECT_EQ(watcherSent(), cp::exactly({useAnimation(840003, VORGALTEM_SECRET_ORDER)}));

	clearSent();
	player().getObserveController()->notifyMoveObservers();
	EXPECT_TRUE(sent().empty()) << "the task removed its observer";
}

TEST_F(QuestItemActionsTest, AMoveDuringTheCastCancelsIt) {
	Item& order = stored(840003, VORGALTEM_SECRET_ORDER, 1);
	watch();
	actionOf<QuestStartAction>(VORGALTEM_SECRET_ORDER).act(player(), Ptr<Item>(order), nullptr);
	executor->advance(1000ms);
	clearSent();
	clearWatcherSent();

	// :51-57: the observer's abort cancels the ITEM_USE task, says STR_ITEM_CANCELED and broadcasts the aborted animation (end 2, unk 0)
	player().getObserveController()->notifyMoveObservers();
	const std::vector<uint8_t> aborted = itemUsageAnimation(PLAYER_OBJECT_ID, 840003, VORGALTEM_SECRET_ORDER, 0, 2, 0);
	EXPECT_EQ(sent(), cp::exactly({message(SM_SYSTEM_MESSAGE::STR_ITEM_CANCELED()), aborted}));
	EXPECT_EQ(watcherSent(), cp::exactly({aborted}));

	clearSent();
	clearWatcherSent();
	executor->advance(5000ms);
	player().getObserveController()->notifyMoveObservers();
	EXPECT_TRUE(sent().empty()) << "the task is cancelled and the one-time observer is gone";
	EXPECT_TRUE(watcherSent().empty());
	EXPECT_FALSE(coolsDown(61));
}

// ---- ReadAction ----------------------------------------------------------------------------------------------------------------------------

TEST_F(QuestItemActionsTest, AReadItemWithoutACastIsReadAtOnce) {
	Item& diary = stored(840002, POISON_RESEARCH_DIARY, 1);

	actionOf<ReadAction>(POISON_RESEARCH_DIARY).act(player(), Ptr<Item>(diary), nullptr);

	// ReadAction.java:31-34 -> finishUse (:59-64): the cooldown, STR_USE_ITEM, then the read animation (end 1, unk 0) to all and himself
	EXPECT_TRUE(coolsDown(61));
	EXPECT_EQ(sent(), cp::exactly({message(SM_SYSTEM_MESSAGE::STR_USE_ITEM(diary.getL10n())),
						  itemUsageAnimation(PLAYER_OBJECT_ID, 840002, POISON_RESEARCH_DIARY, 0, 1, 0)}));
}

TEST_F(QuestItemActionsTest, AReadItemWithACastIsReadAfterIt) {
	Item& brief = stored(840004, INVASION_BRIEF, 1);
	watch();

	actionOf<ReadAction>(INVASION_BRIEF).act(player(), Ptr<Item>(brief), nullptr);

	// :36-38: the cast animation (end 0, unk 0), to all and himself
	const std::vector<uint8_t> cast = itemUsageAnimation(PLAYER_OBJECT_ID, 840004, INVASION_BRIEF, 2000, 0, 0);
	EXPECT_EQ(sent(), cp::exactly({cast}));
	EXPECT_EQ(watcherSent(), cp::exactly({cast}));
	clearSent();
	clearWatcherSent();
	executor->advance(1999ms);
	EXPECT_TRUE(sent().empty());
	EXPECT_FALSE(coolsDown(41));

	// the read animation (end 1) to all and himself (:62-63); the message to himself alone
	executor->advance(1ms);
	EXPECT_TRUE(coolsDown(41));
	const std::vector<uint8_t> read = itemUsageAnimation(PLAYER_OBJECT_ID, 840004, INVASION_BRIEF, 0, 1, 0);
	EXPECT_EQ(sent(), cp::exactly({message(SM_SYSTEM_MESSAGE::STR_USE_ITEM(brief.getL10n())), read}));
	EXPECT_EQ(watcherSent(), cp::exactly({read}));
	clearSent();
	player().getObserveController()->notifyMoveObservers();
	EXPECT_TRUE(sent().empty()) << "the task removed its observer";
}

TEST_F(QuestItemActionsTest, AMoveDuringTheReadingCancelsIt) {
	Item& brief = stored(840004, INVASION_BRIEF, 1);
	watch();
	actionOf<ReadAction>(INVASION_BRIEF).act(player(), Ptr<Item>(brief), nullptr);
	executor->advance(500ms);
	clearSent();
	clearWatcherSent();

	// :42-49: cancel the task, STR_ITEM_CANCELED, the aborted animation (end 2) to all and himself, and the observer removes itself
	player().getObserveController()->notifyMoveObservers();
	const std::vector<uint8_t> aborted = itemUsageAnimation(PLAYER_OBJECT_ID, 840004, INVASION_BRIEF, 0, 2, 0);
	EXPECT_EQ(sent(), cp::exactly({message(SM_SYSTEM_MESSAGE::STR_ITEM_CANCELED()), aborted}));
	EXPECT_EQ(watcherSent(), cp::exactly({aborted}));

	clearSent();
	executor->advance(5000ms);
	player().getObserveController()->notifyMoveObservers();
	EXPECT_TRUE(sent().empty());
	EXPECT_FALSE(coolsDown(41));
}

TEST_F(QuestItemActionsTest, AnItemThatAlsoStartsAQuestIsReadByItsQuestStartAction) {
	Item& method = stored(840005, ODIUM_REFINING_METHOD, 1);

	// ReadAction.java:27-29: <queststart> is one of the item's actions, so the read does nothing - no message, no animation, no cooldown
	actionOf<ReadAction>(ODIUM_REFINING_METHOD).act(player(), Ptr<Item>(method), nullptr);
	EXPECT_TRUE(sent().empty());
	EXPECT_FALSE(coolsDown(41));

	// the QuestStartAction of the same item plays the use (1197 wants level 14)
	actionOf<QuestStartAction>(ODIUM_REFINING_METHOD).act(player(), Ptr<Item>(method), nullptr);
	EXPECT_TRUE(coolsDown(41));
	EXPECT_EQ(sent(), cp::exactly({useAnimation(840005, ODIUM_REFINING_METHOD), message(SM_SYSTEM_MESSAGE::STR_QUEST_ACQUIRE_ERROR_MIN_LEVEL(14)),
						  message(SM_SYSTEM_MESSAGE::STR_USE_ITEM(method.getL10n()))}));
}

} // namespace
} // namespace aion::gameserver::services::item::test
