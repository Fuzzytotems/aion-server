// The Java handler tree of the original server (game-server/data/handlers) and AionClientPacketFactory.java: the registry report must show the
// counts the design expects at M6 (docs/design/handlers-and-porting-plan.md §1.5): 457 AI names, 73 instance maps, 5 zone names, 1,035 quest
// handlers, 152 commands (101/16/35), 186 client packets. The npc id count was computed independently with Python's re over the same files.

#include "RegscanTestSupport.h"

#include <algorithm>
#include <filesystem>
#include <numeric>

#include "Emitter.h"

using namespace aion::gameserver::tools::regscan;
using namespace aion::gameserver::tools::regscan::test;

namespace fs = std::filesystem;

namespace {

const ReportSection& section(const RegistryModel& model, std::string_view title) {
	auto it = std::ranges::find(model.report, title, &ReportSection::title);
	if (it == model.report.end())
		throw std::logic_error("no report section " + std::string(title));
	return *it;
}

bool containsLine(const std::vector<std::string>& lines, std::string_view line) {
	return std::ranges::find(lines, line) != lines.end();
}

} // namespace

TEST(RealJavaTreeTest, JavaHandlerKeysAndCounts) {
	fs::path javaDir = fs::path(AION_GAMESERVER_JAVA_DIR);
	if (!fs::is_directory(javaDir / "data" / "handlers"))
		GTEST_SKIP() << "Java game server tree not found at " << javaDir.string();

	Options options;
	options.javaHandlers = javaDir / "data" / "handlers";
	options.javaClientPacketFactory = javaDir / "src/com/aionemu/gameserver/network/aion/AionClientPacketFactory.java";
	std::vector<Diagnostic> errors;
	ScanInput input = readInput(options, errors);
	ASSERT_NO_FATAL_FAILURE(EXPECT_NO_ERRORS(errors));
	EXPECT_EQ(input.javaHandlerFiles.size(), 1729u);

	RegistryModel model = buildRegistry(input);
	EXPECT_NO_ERRORS(model.errors);
	EXPECT_EQ(section(model, "ai").java, 457u);
	EXPECT_EQ(section(model, "instance").java, 73u);
	EXPECT_EQ(section(model, "zone names").java, 5u);
	EXPECT_EQ(section(model, "quest").java, 1035u);
	EXPECT_EQ(section(model, "admin commands").java, 101u);
	EXPECT_EQ(section(model, "player commands").java, 16u);
	EXPECT_EQ(section(model, "console commands").java, 35u);
	EXPECT_EQ(section(model, "client packets").java, 186u);
	EXPECT_EQ(section(model, "npc ids spawned by handlers").java, 1093u);

	// nothing is ported in the Java-only run, so every Java key is missing
	EXPECT_EQ(section(model, "ai").missing.size(), 457u);
	EXPECT_TRUE(containsLine(section(model, "ai").missing, "aggressive\tai.AggressiveNpcAI"));
	EXPECT_TRUE(containsLine(section(model, "ai").missing, "general\tai.GeneralNpcAI"));
	EXPECT_TRUE(containsLine(section(model, "instance").missing, "300110000\tinstance.dredgion.BaranathDredgionInstance"));
	EXPECT_TRUE(containsLine(section(model, "zone names").missing, "LC1_PVP_SUB_C_110010000\tzone.pvpZones.PvPAreaZone"));
	EXPECT_TRUE(containsLine(section(model, "zone names").missing, "LF1A_SENSORYAREA_Q1012_1_206004_8_210030000\tzone._1012SensoryArea"));
	EXPECT_TRUE(containsLine(section(model, "quest").missing, "1000\tquest.poeta._1000Prologue"));
	EXPECT_TRUE(containsLine(section(model, "quest").missing, "18600\tquest.heiron._18600ScoringSomeBadStigma")); // super(_questId)
	EXPECT_TRUE(containsLine(section(model, "console commands").missing, "consolecommands.Attrbonus"));
	EXPECT_TRUE(containsLine(section(model, "client packets").missing, "CM_MOVE"));

	// the npc id set of QuestSpawnAnalyzer.loadNpcIdsSpawnedByHandlers over the Java sources (Python re: 1093 ids, sum 448069139)
	std::vector<std::string> npcIds = section(model, "npc ids spawned by handlers").missing;
	int64_t sum = 0;
	for (const std::string& id : npcIds)
		sum += std::stoll(id);
	EXPECT_EQ(sum, 448069139);
	EXPECT_EQ(npcIds.front(), "203550");
	EXPECT_EQ(npcIds.back(), "856486");
}
