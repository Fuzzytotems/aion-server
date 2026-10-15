// QuestSpawnAnalyzer (P5-06a, m5d-plan.md E-08 and D8). Unit cases: run() over fabricated static data and quest npcs, with the expectations
// hand-derived from QuestSpawnAnalyzer.java:34-97 - which npcs count as spawned (the handlers' ids, SpawnsData, TownSpawnsData, EventData),
// which quests are unobtainable (minlevel 99 or a faction without a spawned npc, for quests with a handler; any start condition whose
// finished quests are all unobtainable, recursively), which alternative npcs excuse a quest (XML quests only; any spawned one does), the
// event-quest filter (its boundary, and its place before the template lookup), the grouping by quest set, the order of the log lines and
// the elapsed time. QuestSpawnAnalyzerRealDataTest: the P5-06 acceptance row of handlers-and-porting-plan.md §3, "QuestSpawnAnalyzer output
// equals the Java-regex expectation on the ported set" - the npc ids the analyzer takes as spawned by handlers against Java's own pattern
// over the ported handler sources, computed by tools/parity (spawn-analyzer). This executable links the real npcids table:
// aion_gs_registry_npcids_table compiles the generated source of the table aion_game_server links (game-server/CMakeLists.txt).

#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <regex>
#include <span>
#include <sstream>
#include <string>
#include <unordered_set>
#include <vector>

#include <spdlog/sinks/ostream_sink.h>

#include "aion/commons/logging/LoggerFactory.h"
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
#include "aion/gameserver/model/templates/quest/QuestNpc.h"
#include "aion/gameserver/questEngine/QuestSpawnAnalyzer.h"
#include "aion/gameserver/questEngine/handlers/AbstractQuestHandler.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/runtime/base/TaskInfo.h"
#include "aion/gameserver/runtime/lifetime/Reclaimer.h"
#include "aion/gameserver/runtime/lifetime/TaskScope.h"

namespace aion::gameserver::questEngine {
namespace {

using gameserver::model::templates::quest::QuestNpc;

#define ANALYZER_TEST_SCOPE runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST))

constexpr const char* ANALYZER_LOGGER = "com.aionemu.gameserver.questEngine.QuestSpawnAnalyzer";

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

	/**
	 * the captured text with "\n" line ends (spdlog ends a line with "\r\n" on Windows) and every "finished in <n> ms" as "finished in # ms" (the
	 * analysis times itself with the wall clock). Each <n> must be an elapsed time, below a minute (QuestSpawnAnalyzer.java:36, 61: the start
	 * time subtracted), not a point in time
	 */
	std::string text() const {
		static const std::regex MILLIS("finished in ([0-9]+) ms");
		static const std::regex CRLF("\r\n");
		const std::string captured = stream.str();
		for (auto match = std::sregex_iterator(captured.begin(), captured.end(), MILLIS); match != std::sregex_iterator(); ++match)
			EXPECT_LT(std::stoll((*match)[1].str()), 60000) << "not an elapsed time: " << (*match)[0].str();
		return std::regex_replace(std::regex_replace(captured, CRLF, "\n"), MILLIS, "finished in # ms");
	}

private:
	std::string name;
	std::ostringstream stream;
};

// The quests (quest_data.xml's form). With a handler (HANDLER_QUESTS): 1100 plain, 1101 minlevel_permitted 99, 1102 of a faction whose npcs
// nothing spawns, 1103 of a faction with one spawned npc, 1104 of a faction without npc ids, 1111 a Java handler (no XML quest). Without one:
// 1105 needs 1101 and 1102 (both unobtainable), 1106 needs 1101 and 1100 (one obtainable), 1107 has a condition without <finished> and one that
// needs 1105, 1108 has minlevel_permitted 99 (but no handler), 1112 has one start condition, without <finished>, 1114 has two start
// conditions with <finished>, the first needing 1100 (obtainable) and the second 1101 (unobtainable); 999, 1109, 1110, 1113 and the event
// quests 80000 (the first id the event filter drops) and 80001 are plain. 80002, which one case registers, has no template at all.
const char* const QUESTS_XML = R"(<quests>)"
							   R"(<quest id="999"/>)"
							   R"(<quest id="1100"/>)"
							   R"(<quest id="1101" minlevel_permitted="99"/>)"
							   R"(<quest id="1102" npcfaction_id="5"/>)"
							   R"(<quest id="1103" npcfaction_id="6"/>)"
							   R"(<quest id="1104" npcfaction_id="7"/>)"
							   R"(<quest id="1105"><start_conditions><finished quest_id="1101"/><finished quest_id="1102"/></start_conditions></quest>)"
							   R"(<quest id="1106"><start_conditions><finished quest_id="1101"/><finished quest_id="1100"/></start_conditions></quest>)"
							   R"(<quest id="1107"><start_conditions><unfinished>1100</unfinished></start_conditions>)"
							   R"(<start_conditions><finished quest_id="1105"/></start_conditions></quest>)"
							   R"(<quest id="1108" minlevel_permitted="99"/>)"
							   R"(<quest id="1109"/>)"
							   R"(<quest id="1110"/>)"
							   R"(<quest id="1111"/>)"
							   R"(<quest id="1112"><start_conditions><unfinished>1100</unfinished></start_conditions></quest>)"
							   R"(<quest id="1113"/>)"
							   R"(<quest id="1114"><start_conditions><finished quest_id="1100"/></start_conditions>)"
							   R"(<start_conditions><finished quest_id="1101"/></start_conditions></quest>)"
							   R"(<quest id="80000"/>)"
							   R"(<quest id="80001"/>)"
							   R"(</quests>)";

// Every quest but 1111 is an XML quest. 1109's two start npcs are alternatives to each other and 700711 is spawned (a town spawn); 1110's two
// start npcs are alternatives too, and neither is spawned; 1113 has three start npcs, so each has the other two as alternatives, and only
// 700731 is spawned. The other quests have one npc per list, so no alternative (getAlternativeNpcs null).
const char* const XML_QUESTS_XML = R"(<quest_scripts>)"
								   R"(<report_to id="999" start_npc_ids="700130" end_npc_ids="700130"/>)"
								   R"(<report_to id="1100" start_npc_ids="700100" end_npc_ids="700100"/>)"
								   R"(<report_to id="1101" start_npc_ids="700101" end_npc_ids="700101"/>)"
								   R"(<report_to id="1102" start_npc_ids="700102" end_npc_ids="700102"/>)"
								   R"(<report_to id="1103" start_npc_ids="700103" end_npc_ids="700103"/>)"
								   R"(<report_to id="1104" start_npc_ids="700104" end_npc_ids="700104"/>)"
								   R"(<report_to id="1105" start_npc_ids="700105" end_npc_ids="700105"/>)"
								   R"(<report_to id="1106" start_npc_ids="700106" end_npc_ids="700106"/>)"
								   R"(<report_to id="1107" start_npc_ids="700107" end_npc_ids="700107"/>)"
								   R"(<report_to id="1108" start_npc_ids="700108" end_npc_ids="700108"/>)"
								   R"(<report_to id="1109" start_npc_ids="700710 700711" end_npc_ids="700710"/>)"
								   R"(<report_to id="1110" start_npc_ids="700720 700721" end_npc_ids="700720"/>)"
								   R"(<report_to id="1112" start_npc_ids="700112" end_npc_ids="700112"/>)"
								   R"(<report_to id="1113" start_npc_ids="700730 700731 700732" end_npc_ids="700730"/>)"
								   R"(<report_to id="1114" start_npc_ids="700114" end_npc_ids="700114"/>)"
								   R"(<report_to id="80000" start_npc_ids="700151" end_npc_ids="700151"/>)"
								   R"(<report_to id="80001" start_npc_ids="700150" end_npc_ids="700150"/>)"
								   R"(</quest_scripts>)";

const char* const NPC_FACTIONS_XML =
	R"(<npc_factions>)"
	R"(<npc_faction id="5" name="Unspawned" npc_ids="700500" name_id="1" category="DAILY" min_level="1" race="ELYOS"/>)"
	R"(<npc_faction id="6" name="One spawned" npc_ids="700600 700601" name_id="2" category="DAILY" min_level="1" race="ELYOS"/>)"
	R"(<npc_faction id="7" name="Without npcs" name_id="3" category="MENTOR" min_level="1" race="ELYOS"/>)"
	R"(</npc_factions>)";

const char* const SPAWNS_XML = R"(<spawns><spawn_map map_id="210010000">)"
							   R"(<spawn npc_id="700140" respawn_time="295"><spot x="1" y="1" z="1" h="0"/></spawn>)"
							   R"(<spawn npc_id="700601" respawn_time="295"><spot x="2" y="2" z="2" h="0"/></spawn>)"
							   R"(<spawn npc_id="700731" respawn_time="295"><spot x="3" y="3" z="3" h="0"/></spawn>)"
							   R"(</spawn_map></spawns>)";

const char* const TOWN_SPAWNS_XML = R"(<town_spawns_data><spawn_map map_id="700010000"><town_spawn town_id="1001"><town_level level="1">)"
									R"(<spawn npc_id="700711" respawn_time="295"><spot x="1" y="1" z="1" h="0"/></spawn>)"
									R"(</town_level></town_spawn></spawn_map></town_spawns_data>)";

const char* const EVENTS_XML = R"(<timed_events><event name="Past" start="2014-03-01T00:00:00" end="2014-04-01T00:00:00"><spawns>)"
							   R"(<spawn_map map_id="210010000"><spawn npc_id="700141" respawn_time="295"><spot x="1" y="1" z="1" h="0"/></spawn>)"
							   R"(</spawn_map></spawns></event></timed_events>)";

/** A quest handler that registers nothing: the analyzer reads only its quest id */
class FabricatedQuestHandler final : public handlers::AbstractQuestHandler {
public:
	explicit FabricatedQuestHandler(int32_t questId) : AbstractQuestHandler(questId) {}

	void register_() override {}
};

std::vector<int32_t> sorted(const std::unordered_set<int32_t>& ids) {
	std::vector<int32_t> result(ids.begin(), ids.end());
	std::ranges::sort(result);
	return result;
}

/** The six holders run() reads, fabricated; the handlers of HANDLER_QUESTS; the quest npcs each case registers. */
class QuestSpawnAnalyzerTest : public ::testing::Test {
protected:
	static constexpr int32_t HANDLER_QUESTS[] = {1100, 1101, 1102, 1103, 1104, 1111};

	void SetUp() override {
		xml::LoadContext context;
		dataholders::DataManager::QUEST_DATA.publish(xml::bindString<dataholders::QuestsData>(context, QUESTS_XML));
		// QuestNpc's addOnTalkEvent and three more call QuestEngine.registerCanAct, which looks the npc up (no template: a QuestEngine warning)
		dataholders::DataManager::NPC_DATA.publish(xml::bindString<dataholders::NpcData>(context, "<npc_templates/>"));
		dataholders::DataManager::XML_QUESTS.publish(xml::bindString<dataholders::XMLQuests>(context, XML_QUESTS_XML));
		dataholders::DataManager::NPC_FACTIONS_DATA.publish(xml::bindString<dataholders::NpcFactionsData>(context, NPC_FACTIONS_XML));
		dataholders::DataManager::SPAWNS_DATA.publish(xml::bindString<dataholders::SpawnsData>(context, SPAWNS_XML));
		dataholders::DataManager::TOWN_SPAWNS_DATA.publish(xml::bindString<dataholders::TownSpawnsData>(context, TOWN_SPAWNS_XML));
		dataholders::DataManager::EVENT_DATA.publish(xml::bindString<dataholders::EventData>(context, EVENTS_XML));
		for (int32_t questId : HANDLER_QUESTS)
			handlerStorage.push_back(std::make_unique<FabricatedQuestHandler>(questId));
		for (const std::unique_ptr<FabricatedQuestHandler>& handler : handlerStorage)
			questHandlers.push_back(handler.get());
		std::unordered_set<int32_t> spawnedByHandlers = QuestSpawnAnalyzer::loadNpcIdsSpawnedByHandlers();
		ASSERT_FALSE(spawnedByHandlers.empty()) << "this executable links the real npcids table (game-server/CMakeLists.txt)";
		handlerSpawnedNpc = sorted(spawnedByHandlers).front();
		for (int32_t fixtureNpc : {99001, 700100, 700101, 700102, 700103, 700104, 700105, 700106, 700107, 700108, 700112, 700114, 700130, 700131,
				 700140, 700141, 700150, 700151, 700152, 700500, 700600, 700601, 700710, 700711, 700720, 700721, 700730, 700731, 700732, 1000000})
			ASSERT_FALSE(spawnedByHandlers.contains(fixtureNpc)) << "a handler now spawns the fixture's npc " << fixtureNpc << ": pick another id";
	}

	void TearDown() override {
		{
			ANALYZER_TEST_SCOPE;
			npcRefs.clear();
			questNpcs.clear();
		}
		questHandlers.clear();
		handlerStorage.clear();
		dataholders::DataManager::EVENT_DATA.resetForTests();
		dataholders::DataManager::TOWN_SPAWNS_DATA.resetForTests();
		dataholders::DataManager::SPAWNS_DATA.resetForTests();
		dataholders::DataManager::NPC_FACTIONS_DATA.resetForTests();
		dataholders::DataManager::XML_QUESTS.resetForTests();
		dataholders::DataManager::NPC_DATA.resetForTests();
		dataholders::DataManager::QUEST_DATA.resetForTests();
		runtime::Reclaimer::getInstance().drain();
	}

	/** a quest npc with the given quests in one of its six lists (findAllRegisteredQuestIds reads them all) */
	QuestNpc& questNpc(int32_t npcId) {
		for (const runtime::Ref<QuestNpc>& npc : npcRefs) {
			if (npc->getNpcId() == npcId)
				return *npc;
		}
		npcRefs.push_back(QuestNpc::create(npcId));
		questNpcs.push_back(runtime::Ptr<QuestNpc>(npcRefs.back()));
		return *npcRefs.back();
	}

	/** Registers the quest npcs of the full case (the npcs are registered in reverse order where the order of an output matters). */
	void registerEveryCase() {
		questNpc(1000000).addOnQuestStart(1100);   // a 7-digit npc id: sorted numerically after 700100 inside its line
		questNpc(700100).addOnQuestStart(1100);    // reported: nothing spawns it and 1100 is obtainable
		questNpc(700101).addOnTalkEvent(1101);     // minlevel_permitted 99 with a handler: unobtainable
		questNpc(700102).addOnQuestStart(1102);    // its faction's only npc is not spawned: unobtainable
		questNpc(700103).addOnTalkEvent(1103);     // its faction has a spawned npc: reported
		questNpc(700104).addOnKillEvent(1104);     // its faction has no npc ids (null): reported
		questNpc(700105).addOnAtDistanceEvent(1105); // every finished precondition unobtainable
		questNpc(700106).addOnAddAggroListEvent(1106); // one finished precondition obtainable: reported
		questNpc(700107).addOnAttackEvent(1107);   // its second condition needs 1105, unobtainable through its own conditions
		questNpc(700108).addOnQuestStart(1108);    // minlevel_permitted 99 but no handler: not in the unobtainable set, reported
		questNpc(700112).addOnQuestStart(1112);    // a start condition without <finished> (Java: a null list) is skipped: reported
		questNpc(700114).addOnQuestStart(1114);    // its second condition's finished quests are all unobtainable (the first's are not)
		questNpc(700710).addOnQuestStart(1109);    // the alternative start npc 700711 is spawned (town): not reported
		questNpc(700721).addOnQuestStart(1110);    // no alternative spawned: both reported, in one line
		questNpc(700720).addOnQuestStart(1110);
		questNpc(700730).addOnQuestStart(1113);    // one of its two alternatives (700731) is spawned: not reported (anyMatch)
		questNpc(700130).addOnTalkEvent(1103);     // three quests at one npc: one line, the quest ids in numeric order
		questNpc(700130).addOnTalkEvent(1100);
		questNpc(700130).addOnKillEvent(999);
		questNpc(700131).addOnTalkEvent(1111);     // a Java handler's quest (no XML quest: assumed spawned) and an unobtainable one
		questNpc(700131).addOnTalkEvent(1101);
		questNpc(700140).addOnQuestStart(1100);    // spawned (spawns)
		questNpc(700141).addOnQuestStart(1100);    // spawned (an event's spawns)
		questNpc(handlerSpawnedNpc).addOnQuestStart(1100); // spawned by a handler
		questNpc(700150).addOnQuestStart(80001);   // an event quest
		questNpc(700151).addOnQuestStart(80000);   // the lowest event quest id (id < 80000 lets it through only when events are not ignored)
		questNpc(99001).addOnTalkEvent(1106);      // a 5-digit npc id: its line sorts after the "Npc 7..." lines, as strings do
		questNpc(99001).addOnTalkEvent(1104);
	}

	LogCapture questEngineWarnings{"com.aionemu.gameserver.questEngine.QuestEngine"}; // registerCanAct's "No such NPC template" lines
	std::vector<std::unique_ptr<FabricatedQuestHandler>> handlerStorage;
	std::vector<handlers::AbstractQuestHandler*> questHandlers;
	std::vector<runtime::Ref<QuestNpc>> npcRefs;
	std::vector<runtime::Ptr<QuestNpc>> questNpcs;
	int32_t handlerSpawnedNpc = 0;
};

TEST_F(QuestSpawnAnalyzerTest, TheHandlerSpawnedNpcIdsAreTheLinkedTable) {
	std::span<const int32_t> table = gameserver::handlers::npcIdsSpawnedByHandlers();
	EXPECT_EQ(sorted(QuestSpawnAnalyzer::loadNpcIdsSpawnedByHandlers()), std::vector<int32_t>(table.begin(), table.end()));
}

TEST_F(QuestSpawnAnalyzerTest, ReportsTheUnspawnedNpcsOfObtainableQuestsGroupedByTheirQuests) {
	ANALYZER_TEST_SCOPE;
	registerEveryCase();
	LogCapture capture(ANALYZER_LOGGER);
	QuestSpawnAnalyzer::run(questHandlers, questNpcs, true);
	// QuestSpawnAnalyzer.java:35, 63-69: nine quest sets; per line the npc ids ascending ("/"), the quest ids ascending (", "); the lines
	// sorted as strings, so "Npc 99001" follows "Npc 700720" and 1000000 follows 700100 inside its line. Not reported: 700114 (1114 is
	// unobtainable through its second start condition, :77-81), 700730 (1113's alternative 700731 is spawned, :96) and the event quests
	EXPECT_EQ(capture.text(), "info|Analyzing quest handlers (ignoreEventQuests=true)...\n"
							  "warning|Quest handler analysis finished in # ms. Found 9 missing quest npc spawns:"
							  "\n\tNpc 700100/1000000 (quests: 1100)"
							  "\n\tNpc 700103 (quests: 1103)"
							  "\n\tNpc 700104 (quests: 1104)"
							  "\n\tNpc 700106 (quests: 1106)"
							  "\n\tNpc 700108 (quests: 1108)"
							  "\n\tNpc 700112 (quests: 1112)"
							  "\n\tNpc 700130 (quests: 999, 1100, 1103)"
							  "\n\tNpc 700720/700721 (quests: 1110)"
							  "\n\tNpc 99001 (quests: 1104, 1106)\n");
}

TEST_F(QuestSpawnAnalyzerTest, EventQuestsAreReportedOnlyWhenNotIgnored) {
	ANALYZER_TEST_SCOPE;
	registerEveryCase();
	LogCapture capture(ANALYZER_LOGGER);
	QuestSpawnAnalyzer::run(questHandlers, questNpcs, false);
	// quest ids from 80000 on pass the filter only with ignoreEventQuests = false (QuestSpawnAnalyzer.java:56): 80000 and 80001 are reported
	EXPECT_EQ(capture.text(), "info|Analyzing quest handlers (ignoreEventQuests=false)...\n"
							  "warning|Quest handler analysis finished in # ms. Found 11 missing quest npc spawns:"
							  "\n\tNpc 700100/1000000 (quests: 1100)"
							  "\n\tNpc 700103 (quests: 1103)"
							  "\n\tNpc 700104 (quests: 1104)"
							  "\n\tNpc 700106 (quests: 1106)"
							  "\n\tNpc 700108 (quests: 1108)"
							  "\n\tNpc 700112 (quests: 1112)"
							  "\n\tNpc 700130 (quests: 999, 1100, 1103)"
							  "\n\tNpc 700150 (quests: 80001)"
							  "\n\tNpc 700151 (quests: 80000)"
							  "\n\tNpc 700720/700721 (quests: 1110)"
							  "\n\tNpc 99001 (quests: 1104, 1106)\n");
}

TEST_F(QuestSpawnAnalyzerTest, NothingMissingIsAnInfoLine) {
	ANALYZER_TEST_SCOPE;
	questNpc(700140).addOnQuestStart(1100);          // spawned
	questNpc(700101).addOnTalkEvent(1101);           // unobtainable
	questNpc(700710).addOnQuestStart(1109);          // a spawned alternative
	questNpc(700131).addOnTalkEvent(1111);           // not an XML quest
	questNpc(handlerSpawnedNpc).addOnKillEvent(1104); // spawned by a handler
	LogCapture capture(ANALYZER_LOGGER);
	QuestSpawnAnalyzer::run(questHandlers, questNpcs, true);
	EXPECT_EQ(capture.text(), "info|Analyzing quest handlers (ignoreEventQuests=true)...\n"
							  "info|Quest handler analysis finished in # ms without errors\n");
}

TEST_F(QuestSpawnAnalyzerTest, AQuestWithoutATemplateThrowsNullPointerException) {
	ANALYZER_TEST_SCOPE;
	// Java dereferences DataManager.QUEST_DATA.getQuestById's null: for a handler's quest (:48-49) and for a registered quest (:76-77)
	questHandlers.push_back(handlerStorage.emplace_back(std::make_unique<FabricatedQuestHandler>(4242)).get());
	EXPECT_THROW(QuestSpawnAnalyzer::run(questHandlers, questNpcs, true), runtime::NullPointerException);
	questHandlers.pop_back();
	questNpc(700100).addOnTalkEvent(4243);
	EXPECT_THROW(QuestSpawnAnalyzer::run(questHandlers, questNpcs, true), runtime::NullPointerException);
}

TEST_F(QuestSpawnAnalyzerTest, TheEventFilterComesBeforeTheTemplateLookup) {
	ANALYZER_TEST_SCOPE;
	// QuestSpawnAnalyzer.java:56 evaluates (!ignoreEventQuests || id < 80000) first, so with ignoreEventQuests = true isUnobtainable never reads
	// an event quest's template. The kept java-bug of isUnobtainable (no cycle guard; 80313 and 80314 name themselves under <finished>) relies
	// on it. 80002 has no template: dropped by the filter it is harmless, and past the filter isUnobtainable dereferences the null (:76-77)
	questNpc(700152).addOnQuestStart(80002);
	LogCapture capture(ANALYZER_LOGGER);
	EXPECT_NO_THROW(QuestSpawnAnalyzer::run(questHandlers, questNpcs, true));
	EXPECT_EQ(capture.text(), "info|Analyzing quest handlers (ignoreEventQuests=true)...\n"
							  "info|Quest handler analysis finished in # ms without errors\n");
	EXPECT_THROW(QuestSpawnAnalyzer::run(questHandlers, questNpcs, false), runtime::NullPointerException);
}

// ---- the acceptance row: the handlers' npc ids against Java's pattern over the ported handler set -----------------------------------------

const std::filesystem::path CPP_TREE = std::filesystem::path(AION_GAMESERVER_JAVA_DIR) / "../cpp";

/** the Python interpreter of AION_TEST_PYTHON, empty if the variable is not set */
std::string python() {
	char* value = nullptr;
	size_t length = 0;
	if (_dupenv_s(&value, &length, "AION_TEST_PYTHON") != 0 || value == nullptr)
		return {};
	std::string result(value);
	free(value);
	return result;
}

TEST(QuestSpawnAnalyzerRealDataTest, TheHandlerSpawnedNpcIdsAreJavasPatternOverThePortedHandlers) {
	const std::string interpreter = python();
	ASSERT_FALSE(interpreter.empty()) << "set AION_TEST_PYTHON to a Python 3.12 interpreter (ctest sets the configure-time one)";
	const std::filesystem::path handlers = CPP_TREE / "game-server/handlers/aion/gameserver/handlers";
	const std::filesystem::path parity = CPP_TREE / "tools/parity/parity.py";
	ASSERT_TRUE(std::filesystem::is_directory(handlers)) << handlers;
	const std::filesystem::path out = std::filesystem::temp_directory_path() /
		("aion_quest_spawn_analyzer_" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + ".txt");
	// QuestSpawnAnalyzer.loadNpcIdsSpawnedByHandlers reads the instance, quest and ai handler directories; the ported handlers are the .cpp and
	// .h files below the same three directories of the C++ handler tree (what aion_gs_regscan scans for the table)
	std::string command = "\"\"" + interpreter + "\" \"" + parity.string() + "\" spawn-analyzer --suffix .cpp --suffix .h \"" +
		(handlers / "instance").string() + "\" \"" + (handlers / "quest").string() + "\" \"" + (handlers / "ai").string() + "\" > \"" +
		out.string() + "\"\"";
	ASSERT_EQ(std::system(command.c_str()), 0) << command;
	std::ifstream in(out);
	std::string header;
	ASSERT_TRUE(std::getline(in, header)) << out;
	std::vector<int32_t> expected;
	for (std::string line; std::getline(in, line);)
		expected.push_back(std::stoi(line));
	in.close();
	std::error_code ignored;
	std::filesystem::remove(out, ignored);

	std::cout << "tools/parity spawn-analyzer over the ported handlers: " << header << std::endl;
	EXPECT_FALSE(expected.empty()) << header << ": the ported handlers spawn npcs with literal ids (QuestSpawnAnalyzerTest asserts one)";
	EXPECT_EQ(sorted(QuestSpawnAnalyzer::loadNpcIdsSpawnedByHandlers()), expected)
		<< "the analyzer's handler npc ids (the npcids table of aion_gs_regscan) against Java's pattern over the ported handlers; a stale build "
		   "(a handler changed after the last build) fails here too";
}

} // namespace
} // namespace aion::gameserver::questEngine
