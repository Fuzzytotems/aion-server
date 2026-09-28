#include "RegscanTestSupport.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
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

Options fixtureOptions() {
	fs::path valid = fs::path(AION_REGSCAN_FIXTURES_DIR) / "valid";
	Options options;
	options.handlersRoot = valid / "handlers";
	options.clientPacketsRoot = valid / "src";
	options.javaHandlers = valid / "java" / "handlers";
	options.javaClientPacketFactory = valid / "java" / "AionClientPacketFactory.java";
	return options;
}

} // namespace

TEST(FixtureTreeTest, ValidFixtureTree) {
	std::vector<Diagnostic> errors;
	ScanInput input = readInput(fixtureOptions(), errors);
	EXPECT_NO_ERRORS(errors);
	EXPECT_EQ(input.handlerFiles.size(), 16u);
	EXPECT_EQ(input.clientPacketFiles.size(), 3u);
	EXPECT_EQ(input.javaHandlerFiles.size(), 17u);
	EXPECT_TRUE(std::ranges::is_sorted(input.handlerFiles, {}, &SourceFile::relPath));
	EXPECT_EQ(input.handlerFiles.front().relPath, "aion/gameserver/handlers/admincommands/Add.cpp");

	RegistryModel model = buildRegistry(input);
	EXPECT_NO_ERRORS(model.errors);
	std::vector<std::string> aiNames;
	for (const RegistryEntry& entry : model.ai)
		aiNames.push_back(entry.key);
	EXPECT_EQ(aiNames, (std::vector<std::string>{"aggressive", "calindi_flamelord", "calindi_flamelord_drl", "general"}));
	EXPECT_EQ(model.instances.size(), 1u);
	EXPECT_EQ(model.zones.size(), 2u);
	EXPECT_EQ(model.quests.size(), 2u);
	EXPECT_EQ(model.commands.size(), 3u);
	EXPECT_EQ(model.clientPackets.size(), 2u);
	EXPECT_EQ(model.npcIds, (std::set<int32_t>{206001, 206002, 215074, 299999, 700001, 900001}));

	EXPECT_EQ(summaryLine(model),
		"ai 4/5, instance 1/2, zone names 5/5, quest 2/2, admin commands 1/2, player commands 1/1, console commands 1/1, client packets 2/3, npc ids spawned by handlers 6/6");
	EXPECT_EQ(section(model, "ai").missing, (std::vector<std::string>{"not_ported\tai.NotPortedAI"}));
	EXPECT_EQ(section(model, "quest").missing, (std::vector<std::string>{"1719\tquest.reshanta._1719ConfrontAsmodianOfficers"}));
	EXPECT_EQ(section(model, "quest").unknownToJava, (std::vector<std::string>{"9001\tquest.template._9001KeywordPackage"}));
	EXPECT_EQ(section(model, "admin commands").missing, (std::vector<std::string>{"admincommands.Ban"}));
	EXPECT_EQ(section(model, "client packets").missing, (std::vector<std::string>{"CM_LEVEL_READY"}));
	EXPECT_EQ(section(model, "npc ids spawned by handlers").missing, (std::vector<std::string>{"800001"}));
	EXPECT_EQ(section(model, "npc ids spawned by handlers").unknownToJava, (std::vector<std::string>{"900001"}));
}

TEST(FixtureTreeTest, InvalidFixtureTree) {
	Options options;
	options.handlersRoot = fs::path(AION_REGSCAN_FIXTURES_DIR) / "invalid" / "handlers";
	std::vector<Diagnostic> errors;
	ScanInput input = readInput(options, errors);
	EXPECT_NO_ERRORS(errors);
	RegistryModel model = buildRegistry(input);
	EXPECT_ERROR(model.errors, 10, "AION_AI must be in namespace aion::gameserver::handlers::ai");
	EXPECT_ERROR(model.errors, 3, "does not match the file's directory");
	// quest/QuestPrelude.h line 5 (another directive) and quest/_1000DialogActionDirective.cpp line 3 (the directive outside the prelude)
	EXPECT_ERROR(model.errors, 5, "'using namespace' is not allowed");
	EXPECT_ERROR(model.errors, 3, "'using namespace' is not allowed");
	EXPECT_EQ(std::ranges::count_if(model.errors, [](const Diagnostic& d) { return d.message.find("'using namespace'") != std::string::npos; }), 2)
		<< describe(model.errors);
	EXPECT_TRUE(model.ai.empty());
}

TEST(FixtureTreeTest, InputErrors) {
	fs::path temp = fs::path(AION_REGSCAN_TEMP_DIR) / "FixtureTreeTest";
	fs::remove_all(temp);
	fs::create_directories(temp / "aion/gameserver/handlers/ai");
	std::ofstream(temp / "aion/gameserver/handlers/ai/Bad.hpp") << "// unsupported extension\n";
	std::ofstream(temp / "aion/gameserver/handlers/ai/notes.txt") << "ignored\n";

	Options options;
	options.handlersRoot = temp;
	options.clientPacketsRoot = temp / "missing";
	options.javaClientPacketFactory = temp / "missing.java";
	std::vector<Diagnostic> errors;
	ScanInput input = readInput(options, errors);
	EXPECT_ERROR(errors, 0, "unsupported C++ file extension .hpp");
	EXPECT_ERROR(errors, 0, "directory not found");
	EXPECT_ERROR(errors, 0, "cannot read file");
	EXPECT_TRUE(input.handlerFiles.empty());
	fs::remove_all(temp);
}
