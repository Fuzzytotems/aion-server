// QuestEngine and QuestService at M5a (P5-06, m5a-plan.md W-02): init with the empty quest handler registry (quest drops, the update items,
// the 09:00 message cron job; since M5d's D3 join also the XML quests' registration, m5d-plan.md I-05; since E-08 the quest spawn analysis
// on the long-running pool, no AION_PARTIAL site any more), the registration maps, handler registration and the lookups over empty
// registrations. Expectations are hand-derived from QuestEngine.java and QuestService.java. Dispatch with a QuestEnv needs a Player; the
// enter-world and logout paths are covered by the scenario flows (m5a-plan.md §5).

#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <memory>
#include <regex>
#include <sstream>
#include <string>
#include <vector>

#include <spdlog/sinks/ostream_sink.h>

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/gameserver/configs/main/GSConfig.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/EventData.bind.h"
#include "aion/gameserver/dataholders/EventData.h"
#include "aion/gameserver/dataholders/NpcData.bind.h"
#include "aion/gameserver/dataholders/NpcData.h"
#include "aion/gameserver/dataholders/NpcFactionsData.bind.h"
#include "aion/gameserver/dataholders/NpcFactionsData.h"
#include "aion/gameserver/dataholders/QuestsData.bind.h"
#include "aion/gameserver/dataholders/QuestsData.h"
#include "aion/gameserver/dataholders/SpawnsData.bind.h"
#include "aion/gameserver/dataholders/SpawnsData.h"
#include "aion/gameserver/dataholders/TownSpawnsData.bind.h"
#include "aion/gameserver/dataholders/TownSpawnsData.h"
#include "aion/gameserver/dataholders/XMLQuests.bind.h"
#include "aion/gameserver/dataholders/XMLQuests.h"
#include "aion/gameserver/dataholders/loadingutils/LoadContext.h"
#include "aion/gameserver/dataholders/loadingutils/StaticDataLoader.h"
#include "aion/gameserver/handlers/HandlerRegistry.h"
#include "aion/gameserver/model/templates/quest/QuestDrop.h"
#include "aion/gameserver/model/templates/quest/QuestNpc.h"
#include "aion/gameserver/questEngine/QuestEngine.h"
#include "aion/gameserver/questEngine/handlers/AbstractQuestHandler.h"
#include "aion/gameserver/runtime/base/TaskInfo.h"
#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/runtime/lifetime/Reclaimer.h"
#include "aion/gameserver/runtime/lifetime/TaskScope.h"
#include "aion/gameserver/runtime/sched/Clock.h"
#include "aion/gameserver/runtime/sched/DeterministicExecutor.h"
#include "aion/gameserver/runtime/sched/Future.h"
#include "aion/gameserver/services/QuestService.h"
#include "aion/gameserver/services/cron/CronService.h"
#include "aion/gameserver/utils/ThreadPoolManager.h"
#include "aion/gameserver/utils/cron/ThreadPoolManagerRunnableRunner.h"
#include "aion/gameserver/utils/stats/AbyssRankEnum.h"

namespace aion::gameserver::questEngine {
namespace {

#define QUEST_TEST_SCOPE runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST))

/** Captures the messages of one logger ("level|message" per line) while it exists */
class LogCapture {
public:
	explicit LogCapture(std::string loggerName) : name(std::move(loggerName)) {
		auto sink = std::make_shared<spdlog::sinks::ostream_sink_mt>(stream);
		sink->set_pattern("%l|%v");
		commons::logging::LoggerFactory::configure(name, {.sinks = {sink}, .additive = false});
	}
	~LogCapture() { commons::logging::LoggerFactory::removeConfig(name); }
	LogCapture(const LogCapture&) = delete;
	LogCapture& operator=(const LogCapture&) = delete;

	std::string text() const { return stream.str(); }

private:
	std::string name;
	std::ostringstream stream;
};

const char* const QUESTS_XML = R"(<quests>)"
							   R"(<quest id="1000" minlevel_permitted="5"><quest_drop npc_id="210001" item_id="182200001"/><quest_drop npc_id="210001" item_id="182200002"/>)"
							   R"(<inventory_items><inventory_item item_id="182200010"/><inventory_item item_id="182200010"/></inventory_items></quest>)"
							   R"(<quest id="1001" race_permitted="ASMODIANS" minlevel_permitted="10"/>)"
							   R"(<quest id="1002"/>)"
							   R"(</quests>)";

const char* const NPCS_XML = R"(<npc_templates>)"
							 R"(<npc_template npc_id="700001" level="1" name_id="1" name="use" rank="NOVICE" rating="NORMAL" tribe="GENERAL" ai="quest_use_item"><stats maxHp="10"/></npc_template>)"
							 R"(<npc_template npc_id="700002" level="1" name_id="1" name="plain" rank="NOVICE" rating="NORMAL" tribe="GENERAL" ai="general"><stats maxHp="10"/></npc_template>)"
							 R"(</npc_templates>)";

/** A quest handler whose register_() records its calls (quest handlers are Immortal: the engine never frees them) */
class RecordingQuestHandler final : public handlers::AbstractQuestHandler {
public:
	explicit RecordingQuestHandler(int32_t questId) : AbstractQuestHandler(questId) {}

	static inline int32_t registrations = 0;

	void register_() override {
		registrations++;
		qe.registerOnEnterWorld(getQuestId());
		qe.registerOnLogOut(getQuestId());
	}
};

/** QUEST_DATA, NPC_DATA and XML_QUESTS holders, a deterministic executor and the cron service; clears the engine and QuestService again. */
class QuestEngineTest : public ::testing::Test {
protected:
	void SetUp() override {
		utils::ThreadPoolManager::installBackend(nullptr);
		auto backend = std::make_unique<runtime::DeterministicExecutor>(clock, 3);
		executor = backend.get();
		utils::ThreadPoolManager::installBackend(std::move(backend));
		services::cron::CronService::resetForTests();
		services::cron::CronService::initSingleton(std::make_unique<utils::cron::ThreadPoolManagerRunnableRunner>(), std::chrono::locate_zone("UTC"),
			services::cron::CronService::Driver::EXECUTOR);
		xml::LoadContext context;
		dataholders::DataManager::QUEST_DATA.publish(xml::bindString<dataholders::QuestsData>(context, QUESTS_XML));
		dataholders::DataManager::NPC_DATA.publish(xml::bindString<dataholders::NpcData>(context, NPCS_XML));
		dataholders::DataManager::XML_QUESTS.publish(xml::bindString<dataholders::XMLQuests>(context, "<quest_scripts/>"));
		configs::main::GSConfig::ANALYZE_QUESTHANDLERS.store(false);
	}

	void TearDown() override {
		{
			QUEST_TEST_SCOPE;
			QuestEngine::getInstance().clear();
		}
		configs::main::GSConfig::ANALYZE_QUESTHANDLERS.store(true);
		services::cron::CronService::resetForTests();
		// the holders only the spawn analysis case publishes (resetting an unpublished holder does nothing)
		dataholders::DataManager::EVENT_DATA.resetForTests();
		dataholders::DataManager::TOWN_SPAWNS_DATA.resetForTests();
		dataholders::DataManager::SPAWNS_DATA.resetForTests();
		dataholders::DataManager::NPC_FACTIONS_DATA.resetForTests();
		dataholders::DataManager::XML_QUESTS.resetForTests();
		dataholders::DataManager::NPC_DATA.resetForTests();
		dataholders::DataManager::QUEST_DATA.resetForTests();
		runtime::Reclaimer::getInstance().drain();
		utils::ThreadPoolManager::installBackend(nullptr);
		executor = nullptr;
		runtime::Reclaimer::getInstance().drain();
	}

	runtime::ManualClock clock{0};
	runtime::DeterministicExecutor* executor = nullptr;
};

TEST_F(QuestEngineTest, InitRegistersQuestDropsAndSchedulesTheDailyMessageWithAnEmptyHandlerRegistry) {
	ASSERT_TRUE(gameserver::handlers::questHandlerEntries().empty()) << "this test executable links the empty registry";
	LogCapture capture("com.aionemu.gameserver.questEngine.QuestEngine");
	QUEST_TEST_SCOPE;
	uint64_t partialsBefore = runtime::partialHitCount();
	QuestEngine::getInstance().init();

	std::vector<const gameserver::model::templates::quest::QuestDrop*> drops = services::QuestService::getQuestDrop(210001);
	ASSERT_EQ(drops.size(), 2u);
	EXPECT_EQ(drops[0]->getItemId(), 182200001);
	EXPECT_EQ(drops[1]->getItemId(), 182200002);
	// the quest id of a drop is set by the holder while loading (QuestDrop.h, P4-09 QuestsData), not by QuestEngine::init; not checked here
	EXPECT_TRUE(services::QuestService::getQuestDrop(210002).empty());
	EXPECT_NE(capture.text().find("info|Loaded 0 quest handlers."), std::string::npos) << capture.text();
	EXPECT_EQ(QuestEngine::getInstance().getQuestHandlerCount(), 0);
	EXPECT_EQ(runtime::partialHitCount(), partialsBefore) << "no XML quests and no quest handler analysis: no partial site";
	EXPECT_EQ(services::cron::CronService::getInstance().getJobCount(), 1u) << "the 09:00 daily/weekly reset message";

	QuestEngine::getInstance().clear();
	EXPECT_EQ(services::cron::CronService::getInstance().getJobCount(), 0u) << "clear cancels the message task";
	EXPECT_TRUE(services::QuestService::getQuestDrop(210001).empty()) << "clear drops the quest drops";
}

TEST_F(QuestEngineTest, InitRegistersEveryXmlQuestAndRunsTheSpawnAnalysisOnTheLongRunningPool) {
	// the D3 join (m5d-plan.md I-05): init registers every XML quest (QuestEngine.java:104-105) before it logs the handler count (:106). Since
	// E-08 the spawn analysis (:107-108) is no partial site: init hands QuestSpawnAnalyzer.run(questHandlers.values(), questNpcs.values(), true)
	// to the long-running pool, and it runs when the pool does, over the engine's own handlers and quest npcs. The handlers count: 1000
	// (minlevel_permitted 99) and 1003 (a faction whose only npc nothing spawns) are unobtainable because they have a handler
	// (QuestSpawnAnalyzer.java:47-51), so of the three quests at the two unspawned start npcs only 1002 is reported
	xml::LoadContext context;
	dataholders::DataManager::QUEST_DATA.resetForTests();
	dataholders::DataManager::QUEST_DATA.publish(xml::bindString<dataholders::QuestsData>(context,
		R"(<quests><quest id="1000" minlevel_permitted="99"/><quest id="1002"/><quest id="1003" npcfaction_id="2"/></quests>)"));
	dataholders::DataManager::XML_QUESTS.resetForTests();
	dataholders::DataManager::XML_QUESTS.publish(xml::bindString<dataholders::XMLQuests>(context,
		R"(<quest_scripts><item_collecting id="1000" start_npc_ids="700001"/><item_collecting id="1002" start_npc_ids="700002"/>)"
		R"(<item_collecting id="1003" start_npc_ids="700002"/></quest_scripts>)"));
	// the other holders the analysis reads, empty but for the factions: nothing spawns the two start npcs or faction 2's npc
	dataholders::DataManager::NPC_FACTIONS_DATA.publish(xml::bindString<dataholders::NpcFactionsData>(context,
		R"(<npc_factions><npc_faction id="1" name="The Jeridises" name_id="650263" category="MENTOR" min_level="5" race="ELYOS"/>)"
		R"(<npc_faction id="2" name="Unspawned" npc_ids="700900" name_id="650264" category="DAILY" min_level="5" race="ELYOS"/></npc_factions>)"));
	dataholders::DataManager::SPAWNS_DATA.publish(xml::bindString<dataholders::SpawnsData>(context, "<spawns/>"));
	dataholders::DataManager::TOWN_SPAWNS_DATA.publish(xml::bindString<dataholders::TownSpawnsData>(context, "<town_spawns_data/>"));
	dataholders::DataManager::EVENT_DATA.publish(xml::bindString<dataholders::EventData>(context, "<timed_events/>"));
	configs::main::GSConfig::ANALYZE_QUESTHANDLERS.store(true);
	LogCapture capture("com.aionemu.gameserver.questEngine.QuestEngine");
	LogCapture analysis("com.aionemu.gameserver.questEngine.QuestSpawnAnalyzer");
	QUEST_TEST_SCOPE;
	uint64_t partialsBefore = runtime::partialHitCount();
	EXPECT_NO_THROW(QuestEngine::getInstance().init());
	EXPECT_EQ(runtime::partialHitCount(), partialsBefore) << "no partial site: the spawn analysis is ported";
	QuestEngine& qe = QuestEngine::getInstance();
	EXPECT_EQ(qe.getQuestHandlerCount(), 3);
	EXPECT_TRUE(qe.isHaveHandler(1000));
	EXPECT_TRUE(qe.isHaveHandler(1002));
	EXPECT_TRUE(qe.isHaveHandler(1003));
	EXPECT_TRUE(qe.getQuestNpc(700001)->getOnQuestStart().contains(1000)) << "ItemCollecting.register: the start npc offers the quest";
	EXPECT_TRUE(qe.getQuestNpc(700002)->getOnQuestStart().contains(1002));
	EXPECT_TRUE(qe.getQuestNpc(700002)->getOnQuestStart().contains(1003));
	EXPECT_NE(capture.text().find("info|Loaded 3 quest handlers."), std::string::npos) << capture.text();
	EXPECT_EQ(analysis.text(), "") << "the analysis waits for the long-running pool";

	// QuestEngine.java:108, executeLongRunning: one task on the long-running pool and none on the instant pool (addMessageSendingTask's cron
	// job is a timer, not due yet)
	std::vector<runtime::FutureRef> pending = executor->pendingTasks();
	EXPECT_EQ(std::ranges::count_if(pending, [](const runtime::FutureRef& task) { return task->getPool() == runtime::PoolKind::LONG_RUNNING; }), 1);
	EXPECT_EQ(std::ranges::count_if(pending, [](const runtime::FutureRef& task) { return task->getPool() == runtime::PoolKind::INSTANT; }), 0);
	pending.clear();
	EXPECT_EQ(executor->runReady(), 1u) << "the analysis is the one task that is due";
	// "\n" line ends (spdlog ends a line with "\r\n" on Windows), and the wall-clock time of the analysis as "#"
	EXPECT_EQ(std::regex_replace(std::regex_replace(analysis.text(), std::regex("\r\n"), "\n"), std::regex("finished in [0-9]+ ms"),
				  "finished in # ms"),
		"info|Analyzing quest handlers (ignoreEventQuests=true)...\n"
		"warning|Quest handler analysis finished in # ms. Found 1 missing quest npc spawns:\n\tNpc 700002 (quests: 1002)\n");
}

TEST_F(QuestEngineTest, InitLeavesTheSpawnAnalysisOutWhenItIsSwitchedOff) {
	// gameserver.analysis.quest_handlers = false (QuestEngine.java:107): init reads the flag itself and submits nothing, so nothing is read or
	// logged
	xml::LoadContext context;
	dataholders::DataManager::XML_QUESTS.resetForTests();
	dataholders::DataManager::XML_QUESTS.publish(xml::bindString<dataholders::XMLQuests>(context,
		R"(<quest_scripts><item_collecting id="1000" start_npc_ids="700001"/></quest_scripts>)"));
	LogCapture analysis("com.aionemu.gameserver.questEngine.QuestSpawnAnalyzer");
	QUEST_TEST_SCOPE;
	EXPECT_NO_THROW(QuestEngine::getInstance().init());
	std::vector<runtime::FutureRef> pending = executor->pendingTasks();
	EXPECT_TRUE(std::ranges::all_of(pending, [](const runtime::FutureRef& task) { return task->getPool() == runtime::PoolKind::SCHEDULED; }))
		<< "only addMessageSendingTask's cron timer is pending";
	pending.clear();
	EXPECT_EQ(executor->runReady(), 0u) << "no task was submitted";
	EXPECT_EQ(analysis.text(), "") << "the holders the analysis reads are not even published here";
}

TEST_F(QuestEngineTest, QuestNpcsAreCreatedForLookupsAndKeptWhenRegistered) {
	QUEST_TEST_SCOPE;
	QuestEngine& qe = QuestEngine::getInstance();
	runtime::Ref<gameserver::model::templates::quest::QuestNpc> unregistered = qe.getQuestNpc(210001);
	EXPECT_EQ(unregistered->getNpcId(), 210001);
	EXPECT_NE(qe.getQuestNpc(210001).get(), unregistered.get()) << "Java: new QuestNpc(npcId) per lookup";

	runtime::Ptr<gameserver::model::templates::quest::QuestNpc> registered = qe.registerQuestNpc(210001, 20);
	EXPECT_EQ(registered->getQuestRange(), 20);
	EXPECT_EQ(qe.registerQuestNpc(210001).get(), registered.get()) << "the first registration stays";
	EXPECT_EQ(qe.getQuestNpc(210001).get(), registered.get());
}

TEST_F(QuestEngineTest, ItemRegistrationsAndCanActNeedTheQuestUseItemAi) {
	QUEST_TEST_SCOPE;
	QuestEngine& qe = QuestEngine::getInstance();
	EXPECT_FALSE(qe.isRegisteredQuestItem(182200001));
	qe.registerQuestItem(182200001, 1000);
	EXPECT_TRUE(qe.isRegisteredQuestItem(182200001));

	LogCapture capture("com.aionemu.gameserver.questEngine.QuestEngine");
	EXPECT_TRUE(qe.registerCanAct(1000, 700001));
	EXPECT_FALSE(qe.registerCanAct(1000, 700002)) << "another AI";
	EXPECT_FALSE(qe.registerCanAct(1000, 799999));
	EXPECT_NE(capture.text().find("warning|[QuestEngine] No such NPC template for 799999 in Q1000"), std::string::npos) << capture.text();
}

TEST_F(QuestEngineTest, RegisterOnLevelChangedNeedsAKnownQuest) {
	QUEST_TEST_SCOPE;
	QuestEngine& qe = QuestEngine::getInstance();
	EXPECT_NO_THROW(qe.registerOnLevelChanged(1000)) << "no race: both races";
	EXPECT_NO_THROW(qe.registerOnLevelChanged(1001));
	EXPECT_NO_THROW(qe.registerOnLevelChanged(1001)) << "registered once";
	EXPECT_THROW(qe.registerOnLevelChanged(4242), runtime::NullPointerException) << "Java: template.getRacePermitted() on null";
	EXPECT_NO_THROW(qe.registerOnKillRanked(utils::stats::AbyssRankEnum::GRADE9_SOLDIER, 1000));
}

TEST_F(QuestEngineTest, AddQuestHandlerRegistersOnceAndWarnsForADuplicate) {
	QUEST_TEST_SCOPE;
	QuestEngine& qe = QuestEngine::getInstance();
	RecordingQuestHandler::registrations = 0;
	EXPECT_FALSE(qe.isHaveHandler(1002));
	qe.addQuestHandler(std::make_unique<RecordingQuestHandler>(1002));
	EXPECT_TRUE(qe.isHaveHandler(1002));
	EXPECT_EQ(qe.getQuestHandlerCount(), 1);
	EXPECT_EQ(RecordingQuestHandler::registrations, 1);

	LogCapture capture("com.aionemu.gameserver.questEngine.QuestEngine");
	qe.addQuestHandler(std::make_unique<RecordingQuestHandler>(1002));
	EXPECT_EQ(RecordingQuestHandler::registrations, 1) << "the duplicate is not registered";
	EXPECT_EQ(qe.getQuestHandlerCount(), 1);
	EXPECT_NE(capture.text().find("warning|Duplicate handler for quest: 1002"), std::string::npos) << capture.text();

	qe.clear();
	EXPECT_FALSE(qe.isHaveHandler(1002));
}

TEST(QuestServiceTest, LevelRequirementsAndQuestDrops) {
	struct Unpublish {
		~Unpublish() { dataholders::DataManager::QUEST_DATA.resetForTests(); }
	} unpublish;
	xml::LoadContext context;
	dataholders::DataManager::QUEST_DATA.publish(xml::bindString<dataholders::QuestsData>(context,
		R"(<quests><quest id="1" minlevel_permitted="10" maxlevel_permitted="20"/><quest id="2" minlevel_permitted="3"/></quests>)"));
	QUEST_TEST_SCOPE;
	EXPECT_EQ(services::QuestService::getLevelRequirementDiff(1, 7), 3);
	EXPECT_EQ(services::QuestService::getLevelRequirementDiff(1, 12), -2);
	EXPECT_EQ(services::QuestService::getLevelRequirementDiff(99, 1), 99) << "unknown quest";
	EXPECT_FALSE(services::QuestService::checkLevelRequirement(1, 9));
	EXPECT_TRUE(services::QuestService::checkLevelRequirement(1, 20));
	EXPECT_FALSE(services::QuestService::checkLevelRequirement(1, 21));
	EXPECT_TRUE(services::QuestService::checkLevelRequirement(2, 60)) << "max level 0: no limit";
	EXPECT_THROW(static_cast<void>(services::QuestService::checkLevelRequirement(99, 1)), runtime::NullPointerException);
}

} // namespace
} // namespace aion::gameserver::questEngine
