#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <regex>
#include <thread>
#include <vector>

#include "aion/commons/utils/concurrent/RunnableStatsManager.h"

using namespace aion::commons::utils::concurrent;

namespace {

// every test uses its own types, since the statistics are global
struct AlphaTask {};
struct BetaTask {};
struct GammaTask {};
struct ConcurrentTask {};
struct DumpTask {};

std::vector<std::string> linesContaining(const std::vector<std::string>& lines, std::string_view text) {
	std::vector<std::string> result;
	for (const auto& line : lines) {
		if (line.find(text) != std::string::npos)
			result.push_back(line);
	}
	return result;
}

} // namespace

namespace aion::gameserver::taskmanager {
struct PrefixedTask {};
} // namespace aion::gameserver::taskmanager

TEST(RunnableStatsManagerTest, XmlLayout) {
	RunnableStatsManager::clear(); // statistics are global and survive --gtest_repeat
	RunnableStatsManager::handleStats(typeid(AlphaTask), 1500);
	RunnableStatsManager::handleStats(typeid(AlphaTask), 2500);
	RunnableStatsManager::handleStats(typeid(AlphaTask), "runImpl()", 1234567);

	auto lines = RunnableStatsManager::getClassStatsLines(RunnableStatsManager::SortBy::AVG);
	ASSERT_GE(lines.size(), 7u);
	EXPECT_EQ(lines[0], R"(<?xml version="1.0" encoding="UTF-8" standalone="no"?>)");
	EXPECT_EQ(lines[1], "<entries>");
	EXPECT_EQ(lines[2], "\t<!-- This XML contains statistics about execution times. -->");
	EXPECT_EQ(lines[3], "\t<!-- Submitted results will help the developers to optimize the server. -->");
	EXPECT_EQ(lines.back(), "</entries>");

	auto alpha = linesContaining(lines, "AlphaTask\"");
	ASSERT_EQ(alpha.size(), 2u);
	// sorted by AVG: the sort attribute comes first, then all others in enum order; numbers are right aligned, names left aligned
	std::regex runEntry(R"re(^\t<entry average= *"2,000" count= *"2" total= *"4,000" class="[^"]*AlphaTask" +method="run\(\)" +min= *"1,500" max= *"2,500" />$)re");
	std::regex runImplEntry(R"re(^\t<entry average= *"1,234,567" count= *"1" total= *"1,234,567" class="[^"]*AlphaTask" +method="runImpl\(\)" +min= *"1,234,567" max= *"1,234,567" />$)re");
	int matched = 0;
	for (const auto& line : alpha)
		matched += std::regex_match(line, runEntry) + std::regex_match(line, runImplEntry) * 2;
	EXPECT_EQ(matched, 3) << alpha[0] << "\n" << alpha[1];
}

TEST(RunnableStatsManagerTest, NameAndMethodSortAttributes) {
	RunnableStatsManager::clear(); // statistics are global and survive --gtest_repeat
	RunnableStatsManager::handleStats(typeid(BetaTask), "b()", 10);
	auto lines = RunnableStatsManager::getClassStatsLines(RunnableStatsManager::SortBy::METHOD);
	auto beta = linesContaining(lines, "BetaTask\"");
	ASSERT_EQ(beta.size(), 1u);
	EXPECT_TRUE(std::regex_match(beta[0], std::regex(R"re(^\t<entry class="[^"]*BetaTask" +method="b\(\)" +average= *"10" count=.*/>$)re"))) << beta[0];

	lines = RunnableStatsManager::getClassStatsLines(std::nullopt);
	beta = linesContaining(lines, "BetaTask\"");
	ASSERT_EQ(beta.size(), 1u);
	EXPECT_TRUE(beta[0].starts_with("\t<entry average=")) << beta[0];
}

TEST(RunnableStatsManagerTest, SortingByNumbersIsDescending) {
	RunnableStatsManager::clear(); // statistics are global and survive --gtest_repeat
	RunnableStatsManager::handleStats(typeid(GammaTask), "slow()", 900'000'000'000);
	RunnableStatsManager::handleStats(typeid(GammaTask), "fast()", 800'000'000'000);
	auto lines = RunnableStatsManager::getClassStatsLines(RunnableStatsManager::SortBy::MAX);
	auto slow = std::ranges::find_if(lines, [](const auto& l) { return l.find("\"slow()\"") != std::string::npos; });
	auto fast = std::ranges::find_if(lines, [](const auto& l) { return l.find("\"fast()\"") != std::string::npos; });
	ASSERT_NE(slow, lines.end());
	ASSERT_NE(fast, lines.end());
	EXPECT_LT(slow, fast);
	EXPECT_EQ(slow, lines.begin() + 4); // the largest value of all tests
}

TEST(RunnableStatsManagerTest, NameSortOrder) {
	RunnableStatsManager::clear(); // statistics are global and survive --gtest_repeat
	RunnableStatsManager::handleStats(typeid(GammaTask), "zeta()", 1);
	RunnableStatsManager::handleStats(typeid(GammaTask), "Zeta()", 1);
	RunnableStatsManager::handleStats(typeid(GammaTask), "zet()", 1);
	auto lines = RunnableStatsManager::getClassStatsLines(RunnableStatsManager::SortBy::METHOD);
	auto position = [&](std::string_view method) {
		return std::ranges::find_if(lines, [&](const auto& l) { return l.find(method) != std::string::npos; }) - lines.begin();
	};
	// lower case before upper case, prefixes first
	EXPECT_LT(position("\"zet()\""), position("\"zeta()\""));
	EXPECT_LT(position("\"zeta()\""), position("\"Zeta()\""));
}

TEST(RunnableStatsManagerTest, GameServerPrefixIsRemoved) {
	RunnableStatsManager::clear(); // statistics are global and survive --gtest_repeat
	RunnableStatsManager::handleStats(typeid(aion::gameserver::taskmanager::PrefixedTask), 5);
	auto lines = RunnableStatsManager::getClassStatsLines(std::nullopt);
	EXPECT_EQ(linesContaining(lines, "class=\"taskmanager::PrefixedTask\"").size(), 1u);
}

TEST(RunnableStatsManagerTest, ConcurrentUpdates) {
	RunnableStatsManager::clear(); // statistics are global and survive --gtest_repeat
	std::vector<std::thread> threads;
	for (int t = 0; t < 8; t++) {
		threads.emplace_back([] {
			for (int i = 0; i < 1000; i++) {
				RunnableStatsManager::handleStats(typeid(ConcurrentTask), 1);
				RunnableStatsManager::handleStats(typeid(ConcurrentTask), "m" + std::to_string(i % 5) + "()", 2);
			}
		});
	}
	for (auto& thread : threads)
		thread.join();
	auto lines = linesContaining(RunnableStatsManager::getClassStatsLines(RunnableStatsManager::SortBy::NAME), "ConcurrentTask\"");
	ASSERT_EQ(lines.size(), 6u);
	auto countOf = [&](const char* pattern) { return std::ranges::count_if(lines, [&](const auto& l) { return std::regex_search(l, std::regex(pattern)); }); };
	EXPECT_EQ(countOf(R"(count= *"8,000")"), 1);
	EXPECT_EQ(countOf(R"(count= *"1,600")"), 5);
}

TEST(RunnableStatsManagerTest, DumpWritesFile) {
	RunnableStatsManager::clear(); // statistics are global and survive --gtest_repeat
	RunnableStatsManager::handleStats(typeid(DumpTask), 42);
	auto folder = std::filesystem::temp_directory_path() / "aion_RunnableStatsManagerTest";
	std::filesystem::remove_all(folder);
	auto file = folder / "stats" / "MethodStats.log";
	RunnableStatsManager::dumpClassStats(RunnableStatsManager::SortBy::COUNT, file);

	std::ifstream in(file);
	ASSERT_TRUE(in.is_open());
	std::vector<std::string> lines;
	for (std::string line; std::getline(in, line);)
		lines.push_back(line);
	EXPECT_EQ(lines, RunnableStatsManager::getClassStatsLines(RunnableStatsManager::SortBy::COUNT));
	in.close();
	std::filesystem::remove_all(folder);
}
