// QuestEngine and QuestService at M5a (P5-06, m5a-plan.md W-02): init with the empty quest handler registry (quest drops, the update items, the
// quest spawn analysis and the XML quests as AION_PARTIAL sites, the 09:00 message cron job), the registration maps, handler registration and
// the lookups over empty registrations. Expectations are hand-derived from QuestEngine.java and QuestService.java. Dispatch with a QuestEnv needs a
// Player; the enter-world and logout paths are covered by the scenario flows (m5a-plan.md §5).

#include <gtest/gtest.h>

#include <cstdint>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include <spdlog/sinks/ostream_sink.h>

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/gameserver/configs/main/GSConfig.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/NpcData.bind.h"
#include "aion/gameserver/dataholders/NpcData.h"
#include "aion/gameserver/dataholders/QuestsData.bind.h"
#include "aion/gameserver/dataholders/QuestsData.h"
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
		utils::ThreadPoolManager::installBackend(std::make_unique<runtime::DeterministicExecutor>(clock, 3));
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
		dataholders::DataManager::XML_QUESTS.resetForTests();
		dataholders::DataManager::NPC_DATA.resetForTests();
		dataholders::DataManager::QUEST_DATA.resetForTests();
		runtime::Reclaimer::getInstance().drain();
		utils::ThreadPoolManager::installBackend(nullptr);
		runtime::Reclaimer::getInstance().drain();
	}

	runtime::ManualClock clock{0};
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

TEST_F(QuestEngineTest, InitReachesThePartialSitesOfXmlQuestsAndTheSpawnAnalysis) {
	xml::LoadContext context;
	dataholders::DataManager::XML_QUESTS.resetForTests();
	dataholders::DataManager::XML_QUESTS.publish(xml::bindString<dataholders::XMLQuests>(context, R"(<quest_scripts><item_collecting id="1002"/></quest_scripts>)"));
	configs::main::GSConfig::ANALYZE_QUESTHANDLERS.store(true);
	QUEST_TEST_SCOPE;
	uint64_t partialsBefore = runtime::partialHitCount();
	EXPECT_NO_THROW(QuestEngine::getInstance().init());
	EXPECT_EQ(runtime::partialHitCount(), partialsBefore + 2);
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
