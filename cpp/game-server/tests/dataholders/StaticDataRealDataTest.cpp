// P4-09 on the real static data (docs/design/handlers-and-porting-plan.md §2.7 item 3 and 4, static-data.md §4 V2/V3): DataManager::loadStaticData
// loads every import of the Java tree's static_data.xml strictly, with all hooks, and logs the 90 "Loaded N ..." lines; the lines must equal
// the committed count oracle (tools/oracle/expected/static_data_counts.txt) and the element/attribute totals the totals oracle (totals.json).
// The post-processing of DataManager (item cleanup, global drop rules, buy list and motion validation, decompose ids) must run without errors,
// and its effects are checked against the XML. With AION_TEST_PYTHON set to a Python 3.12 interpreter, the test also runs
// `oracle.py compare-counts` on the captured log and `oracle.py compare-totals` on the written totals document.

#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>
#include <spdlog/sinks/ostream_sink.h>

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/GlobalDropData.h"
#include "aion/gameserver/dataholders/ItemData.h"
#include "aion/gameserver/dataholders/ItemRestrictionCleanupData.h"
#include "aion/gameserver/dataholders/NpcData.h"
#include "aion/gameserver/dataholders/QuestsData.h"
#include "aion/gameserver/dataholders/SkillData.h"
#include "aion/gameserver/dataholders/StaticData.h"
#include "aion/gameserver/dataholders/detail/JavaHashMapOrder.h"
#include "aion/gameserver/dataholders/loadingutils/LoadContext.h"
#include "aion/gameserver/model/TribeClass.h"
#include "aion/gameserver/model/templates/QuestTemplate.h"
#include "aion/gameserver/model/templates/globaldrops/GlobalDropNpcNames.h"
#include "aion/gameserver/model/templates/globaldrops/GlobalDropNpcs.h"
#include "aion/gameserver/model/templates/globaldrops/GlobalRule.h"
#include "aion/gameserver/model/templates/item/ItemTemplate.h"
#include "aion/gameserver/model/templates/npc/NpcTemplate.h"
#include "aion/gameserver/model/templates/stats/StatsTemplate.h"
#include "aion/gameserver/runtime/base/TaskInfo.h"
#include "aion/gameserver/runtime/lifetime/TaskScope.h"

namespace aion::gameserver::dataholders {
namespace {

using json = nlohmann::json;

const std::filesystem::path STATIC_DATA = std::filesystem::path(AION_GAMESERVER_JAVA_DIR) / "data/static_data";
const std::filesystem::path ORACLE = std::filesystem::path(AION_GAMESERVER_JAVA_DIR) / "../cpp/tools/oracle";

/** Captures the messages of one logger (one message per line) while it exists */
class LogCapture {
public:
	explicit LogCapture(std::string loggerName) : name(std::move(loggerName)) {
		auto sink = std::make_shared<spdlog::sinks::ostream_sink_mt>(stream);
		sink->set_pattern("%v");
		commons::logging::LoggerFactory::configure(name, {.sinks = {sink}, .additive = false});
	}
	~LogCapture() { commons::logging::LoggerFactory::removeConfig(name); }
	LogCapture(const LogCapture&) = delete;
	LogCapture& operator=(const LogCapture&) = delete;

	std::vector<std::string> lines() const {
		std::vector<std::string> result;
		std::istringstream in(stream.str());
		for (std::string line; std::getline(in, line);) {
			if (!line.empty() && line.back() == '\r')
				line.pop_back();
			result.push_back(line);
		}
		return result;
	}

	std::string text() const { return stream.str(); }

private:
	std::string name;
	std::ostringstream stream;
};

std::vector<std::string> readLines(const std::filesystem::path& file) {
	std::ifstream in(file, std::ios::binary);
	std::vector<std::string> lines;
	for (std::string line; std::getline(in, line);) {
		if (!line.empty() && line.back() == '\r')
			line.pop_back();
		if (!line.empty())
			lines.push_back(line);
	}
	return lines;
}

std::filesystem::path tempFile(const std::string& name) {
	auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
	return std::filesystem::temp_directory_path() / ("aion_p409_" + std::to_string(stamp) + "_" + name);
}

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

int runOracle(const std::string& interpreter, const std::string& arguments) {
	std::string command = "\"\"" + interpreter + "\" \"" + (ORACLE / "oracle.py").string() + "\" " + arguments + "\"";
	std::cout << "running: " << command << std::endl;
	return std::system(command.c_str());
}

class StaticDataRealDataTest : public testing::Test {
protected:
	void SetUp() override {
		if (!std::filesystem::exists(STATIC_DATA / "static_data.xml") || !std::filesystem::exists(ORACLE / "expected/static_data_counts.txt"))
			GTEST_SKIP() << "Java data tree or oracle outputs not found: " << STATIC_DATA << ", " << ORACLE;
	}
};

TEST_F(StaticDataRealDataTest, LoadsAllImportsStrictlyWithHooksAndPostProcessing) {
	runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
	xml::LoadOptions options;
	options.strict = true;
	options.collectStats = true;
	options.parallelParse = true;
	xml::LoadContext context(options);

	const int32_t treeifiedBefore = detail::javaTreeifiedBucketCount();
	auto start = std::chrono::steady_clock::now();
	std::unique_ptr<StaticData> data;
	std::vector<std::string> lines;
	{
		LogCapture capture("com.aionemu.gameserver.dataholders.StaticData");
		data = DataManager::loadStaticData(context, STATIC_DATA / "static_data.xml");
		lines = capture.lines();
	}
	auto loaded = std::chrono::steady_clock::now();

	// V2: the 90 lines in Java wording and order, carrying the 92 numbers of the count oracle
	std::vector<std::string> expected = readLines(ORACLE / "expected/static_data_counts.txt");
	ASSERT_EQ(expected.size(), 90u);
	EXPECT_EQ(lines.size(), expected.size());
	for (size_t i = 0; i < std::min(lines.size(), expected.size()); ++i)
		EXPECT_EQ(lines[i], expected[i]) << "line " << i + 1;

	// V3: every element and attribute of the imported files was bound or deliberately ignored
	std::ifstream totalsIn(ORACLE / "expected/totals.json", std::ios::binary);
	json totals = json::parse(totalsIn);
	uint64_t expectedElements = 0;
	uint64_t expectedAttributes = 0;
	for (const auto& [tag, entry] : totals.at("byTag").items()) {
		expectedElements += entry.at("count").get<uint64_t>();
		for (const auto& [attribute, count] : entry.at("attributes").items())
			expectedAttributes += count.get<uint64_t>();
	}
	const xml::BindStats& stats = context.stats();
	EXPECT_EQ(stats.totalElements().bound + stats.totalElements().ignored, expectedElements);
	EXPECT_EQ(stats.totalElements().unknown, 0u);
	EXPECT_EQ(stats.totalAttributes().bound + stats.totalAttributes().ignored, expectedAttributes);
	EXPECT_EQ(stats.totalAttributes().unknown, 0u);
	// V3 per tag, in C++ (without Python): the element count and every attribute count of each tag equal the oracle's (oracle.py
	// compare-totals rules: a tag missing on either side, a different count or a different attribute count is a difference)
	{
		std::ostringstream written;
		stats.writeTotals(written, STATIC_DATA.generic_string());
		json actual = json::parse(written.str());
		const json& expectedTags = totals.at("byTag");
		const json& actualTags = actual.at("byTag");
		std::vector<std::string> differences;
		auto attributeCount = [](const json& attributes, const std::string& name) {
			return attributes.contains(name) ? attributes.at(name).get<uint64_t>() : uint64_t{0};
		};
		for (const auto& [tag, entry] : expectedTags.items()) {
			if (!actualTags.contains(tag)) {
				differences.push_back("<" + tag + ">: missing in actual");
				continue;
			}
			const json& other = actualTags.at(tag);
			if (entry.at("count") != other.at("count"))
				differences.push_back("<" + tag + ">: " + other.at("count").dump() + " elements, expected " + entry.at("count").dump());
			for (const auto& [attribute, count] : entry.at("attributes").items()) {
				if (count.get<uint64_t>() != attributeCount(other.at("attributes"), attribute))
					differences.push_back("<" + tag + "> @" + attribute + ": " + std::to_string(attributeCount(other.at("attributes"), attribute)) +
					                      ", expected " + count.dump());
			}
			for (const auto& [attribute, count] : other.at("attributes").items()) {
				if (!entry.at("attributes").contains(attribute))
					differences.push_back("<" + tag + "> @" + attribute + ": " + count.dump() + ", expected 0");
			}
		}
		for (const auto& [tag, entry] : actualTags.items()) {
			if (!expectedTags.contains(tag))
				differences.push_back("<" + tag + ">: not expected");
		}
		EXPECT_TRUE(differences.empty()) << differences.size() << " difference(s), first: " << differences.front();
	}
	// detail::JavaHashMapOrder: no index map of the real data has a bucket that Java would turn into a tree (its order is not modelled)
	EXPECT_EQ(detail::javaTreeifiedBucketCount(), treeifiedBefore);

	// the post-processing of DataManager, on the unpublished holders
	const int32_t cleanupItemId = data->itemCleanup->getList().at(0).getId();
	const int32_t maskBefore = data->itemData->getItemTemplate(cleanupItemId)->getMask();
	EXPECT_NO_THROW(DataManager::postProcess(*data));
	auto processed = std::chrono::steady_clock::now();

	// ItemData.cleanup: each result 1 sets and 0 clears the mask bit of the cleanup entry's item (XML: item_restriction_cleanups.xml)
	{
		const auto& cleanup = data->itemCleanup->getList().at(0);
		int32_t mask = maskBefore;
		auto apply = [&mask](int8_t result, int32_t bit) {
			if (result == 1)
				mask |= bit;
			else if (result == 0)
				mask &= ~bit;
		};
		apply(cleanup.resultTrade(), 1 << 1);
		apply(cleanup.resultSell(), 1 << 2);
		apply(cleanup.resultWH(), 1 << 3);
		apply(cleanup.resultAccountWH(), 1 << 4);
		apply(cleanup.resultLegionWH(), 1 << 5);
		EXPECT_EQ(data->itemData->getItemTemplate(cleanupItemId)->getMask(), mask);
	}

	// GlobalDropData.processRules: a rule with npc names that matched npcs has an npc list and no names left
	{
		size_t replaced = 0;
		for (const auto& rule : data->globalDropData->getAllRules()) {
			if (rule.getGlobalRuleNpcNames() == nullptr)
				continue;
			if (rule.getGlobalRuleNpcNames()->getGlobalDropNpcNames().empty()) {
				ASSERT_NE(rule.getGlobalRuleNpcs(), nullptr) << rule.getRuleName();
				EXPECT_FALSE(rule.getGlobalRuleNpcs()->getGlobalDropNpcs().empty()) << rule.getRuleName();
				++replaced;
			}
		}
		EXPECT_GT(replaced, 0u) << "no global drop rule with npc names matched an npc";
	}

	// NpcData.init on real templates without attack, defense and resist attributes in npc_templates.xml: the filled values were computed
	// independently with IEEE single precision arithmetic from the XML level, rating and rank (NpcStatCalculation.java formulas; Math.round
	// ties up). Order: attack, accuracy, magical attack, macc, mresist, mdef, pdef, parry, stun-like resistance, strike resist, spell resist.
	{
		struct Expected {
			int32_t npcId;
			std::vector<int32_t> values;
		};
		const std::vector<Expected> expectedStats = {
			{203197, {55, 333, 72, 225, 157, 45, 153, 360, 0, 0, 0}},             // level 9, NORMAL NOVICE: no strike/spell resist below 50
			{203338, {1586, 1779, 877, 1202, 1138, 484, 1645, 2061, 3400, 0, 0}}, // level 43, HERO MASTER
			{203355, {1326, 2156, 812, 1456, 1338, 420, 1428, 2526, 300, 189, 56}}, // level 56, ELITE SEASONED
			{205438, {2345, 2663, 1326, 1799, 1884, 683, 2321, 3116, 2800, 700, 278}}, // level 65, HERO VETERAN: strike resist 911 capped at 700
		};
		for (const Expected& expectedNpc : expectedStats) {
			const model::templates::npc::NpcTemplate* npc = data->npcData->getNpcTemplate(expectedNpc.npcId);
			ASSERT_NE(npc, nullptr) << expectedNpc.npcId;
			const auto* npcStats = npc->getStatsTemplate();
			std::vector<int32_t> actualValues = {npcStats->getAttack(), npcStats->getAccuracy(), npcStats->getMagicalAttack(), npcStats->getMacc(),
			                                     npcStats->getMresist(), npcStats->getMdef(), npcStats->getPdef(), npcStats->getParry(),
			                                     npcStats->getStunLikeResistance(), npcStats->getStrikeResist(), npcStats->getSpellResist()};
			EXPECT_EQ(actualValues, expectedNpc.values) << expectedNpc.npcId;
			EXPECT_EQ(npcStats->getMcrit(), 50) << expectedNpc.npcId;
			EXPECT_EQ(npcStats->getPcrit(), 10) << expectedNpc.npcId;
		}
	}
	// NpcData.init: stats left at 0 by the XML are filled in (mcrit 50 and pcrit 10 for every non-pet npc without them)
	{
		size_t checked = 0;
		for (const model::templates::npc::NpcTemplate* npc : data->npcData->getNpcData()) {
			if (npc->getTribe() == model::TribeClass::PET || npc->getTribe() == model::TribeClass::PET_DARK)
				continue;
			EXPECT_NE(npc->getStatsTemplate()->getMcrit(), 0) << npc->getTemplateId();
			EXPECT_NE(npc->getStatsTemplate()->getPcrit(), 0) << npc->getTemplateId();
			if (++checked == 500)
				break;
		}
		EXPECT_EQ(checked, 500u);
	}

	// QuestsData::getQuestTemplates: every quest once
	EXPECT_EQ(data->questData->getQuestTemplates().size(), static_cast<size_t>(data->questData->size()));

	auto millis = [](auto from, auto to) { return std::chrono::duration_cast<std::chrono::milliseconds>(to - from).count(); };
	std::cout << "static data loaded in " << millis(start, loaded) << " ms, post-processed in " << millis(loaded, processed) << " ms" << std::endl;

	// the independent Python oracle over the captured log and the totals document
	std::string interpreter = python();
	if (interpreter.empty()) {
		std::cout << "AION_TEST_PYTHON is not set: oracle.py compare-counts/compare-totals skipped (the C++ comparison above uses its outputs)"
		          << std::endl;
		return;
	}
	std::filesystem::path logFile = tempFile("static_data_counts.log");
	std::filesystem::path totalsFile = tempFile("totals.json");
	{
		std::ofstream out(logFile, std::ios::binary);
		for (const std::string& line : lines)
			out << line << '\n';
	}
	{
		std::ofstream out(totalsFile, std::ios::binary);
		stats.writeTotals(out, STATIC_DATA.generic_string());
	}
	EXPECT_EQ(runOracle(interpreter, "compare-counts --log \"" + logFile.string() + "\""), 0);
	EXPECT_EQ(runOracle(interpreter, "compare-totals --actual \"" + totalsFile.string() + "\""), 0);
	std::error_code ignored;
	std::filesystem::remove(logFile, ignored);
	std::filesystem::remove(totalsFile, ignored);
}

} // namespace
} // namespace aion::gameserver::dataholders
