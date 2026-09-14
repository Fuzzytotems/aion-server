#include <gtest/gtest.h>

#include <algorithm>
#include <sstream>
#include <string>
#include <thread>
#include <type_traits>
#include <vector>

#include <spdlog/sinks/ostream_sink.h>

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/gameserver/runtime/base/Unported.h"

using namespace aion::gameserver::runtime;

namespace {

int32_t unportedValue(int32_t) {
	AION_UNPORTED(); // [[noreturn]]: no return statement, no warning
}

void unportedVoid() {
	AION_UNPORTED();
}

struct UnportedMember {
	int32_t compute() const { AION_UNPORTED(); }
};

template <class T>
T unportedTemplate() {
	AION_UNPORTED();
}

const UnportedHit* findHit(const std::vector<UnportedHit>& hits, std::string_view functionPart) {
	auto it = std::ranges::find_if(hits, [&](const UnportedHit& hit) { return hit.function.find(functionPart) != std::string::npos; });
	return it == hits.end() ? nullptr : &*it;
}

std::string thisFile() {
	return detail::shortenUnportedFileName(std::source_location::current().file_name());
}

} // namespace

TEST(UnportedTest, ThrowsWithFunctionAndLocation) {
	try {
		unportedValue(1);
		FAIL();
	} catch (const UnportedException& e) {
		std::string message = e.what();
		EXPECT_NE(message.find("unportedValue"), std::string::npos) << message;
		EXPECT_NE(message.find(" is not ported yet (" + thisFile() + ":20)"), std::string::npos) << message;
	}
	EXPECT_THROW(unportedVoid(), aion::commons::utils::UnsupportedOperationException);
	EXPECT_THROW(UnportedMember().compute(), UnportedException);
	EXPECT_THROW(unportedTemplate<int>(), UnportedException);
}

TEST(UnportedTest, CountsHitsPerSiteAndLogsOnce) {
	std::ostringstream logged;
	auto sink = std::make_shared<spdlog::sinks::ostream_sink_mt>(logged);
	aion::commons::logging::LoggerFactory::configure("com.aionemu.gameserver.Unported", {.sinks = {sink}, .additive = false});
	resetUnportedHitsForTests();

	for (int i = 0; i < 3; i++)
		EXPECT_THROW(unportedVoid(), UnportedException);
	EXPECT_THROW(UnportedMember().compute(), UnportedException);

	auto hits = unportedHits();
	const UnportedHit* voidHit = findHit(hits, "unportedVoid");
	const UnportedHit* memberHit = findHit(hits, "compute");
	ASSERT_NE(voidHit, nullptr);
	ASSERT_NE(memberHit, nullptr);
	EXPECT_EQ(voidHit->hits, 3u);
	EXPECT_EQ(voidHit->file, thisFile());
	EXPECT_EQ(voidHit->line, 24u);
	EXPECT_EQ(memberHit->hits, 1u);
	EXPECT_GE(unportedHitCount(), 4u);

	std::string log = logged.str();
	auto occurrences = [&](std::string_view text) {
		size_t count = 0;
		for (size_t pos = log.find(text); pos != std::string::npos; pos = log.find(text, pos + 1))
			count++;
		return count;
	};
	// a site that was first reached by an earlier test already logged; this test's sites log at most once each
	EXPECT_LE(occurrences("AION_UNPORTED reached: "), 2u);
	aion::commons::logging::LoggerFactory::removeConfig("com.aionemu.gameserver.Unported");

	resetUnportedHitsForTests();
	hits = unportedHits();
	voidHit = findHit(hits, "unportedVoid");
	ASSERT_NE(voidHit, nullptr); // stays listed
	EXPECT_EQ(voidHit->hits, 0u);
}

TEST(UnportedTest, FirstHitLogsWarningWithLocation) {
	struct LocalSite {
		static void run() { AION_UNPORTED(); }
	};
	std::ostringstream logged;
	auto sink = std::make_shared<spdlog::sinks::ostream_sink_mt>(logged);
	aion::commons::logging::LoggerFactory::configure("com.aionemu.gameserver.Unported", {.sinks = {sink}, .additive = false});
	EXPECT_THROW(LocalSite::run(), UnportedException);
	EXPECT_THROW(LocalSite::run(), UnportedException);
	aion::commons::logging::LoggerFactory::removeConfig("com.aionemu.gameserver.Unported");

	std::string log = logged.str();
	size_t first = log.find("AION_UNPORTED reached: ");
	ASSERT_NE(first, std::string::npos) << log;
	EXPECT_EQ(log.find("AION_UNPORTED reached: ", first + 1), std::string::npos) << log;
	EXPECT_NE(log.find(" at " + thisFile() + ":"), std::string::npos) << log;
	EXPECT_NE(log.find("run"), std::string::npos) << log;
}

TEST(UnportedTest, ConcurrentHitsAreCounted) {
	resetUnportedHitsForTests();
	std::vector<std::thread> threads;
	for (int t = 0; t < 8; t++) {
		threads.emplace_back([] {
			for (int i = 0; i < 500; i++) {
				try {
					unportedTemplate<double>();
				} catch (const UnportedException&) {
				}
			}
		});
	}
	for (auto& thread : threads)
		thread.join();
	auto hits = unportedHits();
	auto count = std::ranges::count_if(hits, [](const UnportedHit& hit) { return hit.function.find("unportedTemplate") != std::string::npos && hit.hits == 4000; });
	EXPECT_EQ(count, 1);
}

TEST(UnportedTest, TraceFormat) {
	resetUnportedHitsForTests();
	EXPECT_THROW(unportedValue(2), UnportedException);
	EXPECT_THROW(unportedValue(3), UnportedException);
	std::ostringstream out;
	writeUnportedTrace(out);
	std::istringstream in(out.str());
	std::vector<std::string> lines;
	for (std::string line; std::getline(in, line);)
		lines.push_back(line);
	ASSERT_FALSE(lines.empty());
	EXPECT_EQ(lines[0], "# AION_UNPORTED trace v1");
	std::string expectedPrefix = "2\t" + thisFile() + ":20\t";
	auto it = std::ranges::find_if(lines, [&](const std::string& line) { return line.starts_with(expectedPrefix); });
	ASSERT_NE(it, lines.end()) << out.str();
	EXPECT_NE(it->find("unportedValue"), std::string::npos);

	// sorted by file and line
	auto hits = unportedHits();
	EXPECT_TRUE(std::ranges::is_sorted(hits, [](const UnportedHit& a, const UnportedHit& b) {
		return a.file != b.file ? a.file < b.file : a.line < b.line;
	}));
}

TEST(UnportedTest, ShortenFileName) {
	EXPECT_EQ(detail::shortenUnportedFileName("D:\\aion-server\\cpp\\game-server\\src\\aion\\gameserver\\world\\World.cpp"), "aion/gameserver/world/World.cpp");
	EXPECT_EQ(detail::shortenUnportedFileName("/x/cpp/game-server/handlers/aion/gameserver/handlers/ai/GeneralNpcAI.cpp"),
		"aion/gameserver/handlers/ai/GeneralNpcAI.cpp");
	EXPECT_EQ(detail::shortenUnportedFileName("other/File.cpp"), "other/File.cpp");
}

namespace aion::gameserver::handlers::quest {
namespace {
// handler code: unported bodies and the wave-1 names of the API (S0a decision 1) without naming runtime
int32_t unportedHandlerHook() {
	AION_UNPORTED();
}
} // namespace
} // namespace aion::gameserver::handlers::quest

namespace wave1 = aion::gameserver::handlers;
namespace kernel = aion::gameserver::runtime;
static_assert(std::is_same_v<wave1::UnportedException, kernel::UnportedException>);
static_assert(std::is_same_v<wave1::UnportedHit, kernel::UnportedHit>);

TEST(UnportedTest, WaveOneHandlerNamesStayAvailable) {
	EXPECT_EQ(&wave1::unportedHitCount, &kernel::unportedHitCount);
	EXPECT_EQ(&wave1::unportedHits, &kernel::unportedHits);
	EXPECT_EQ(&wave1::writeUnportedTrace, &kernel::writeUnportedTrace);
	EXPECT_EQ(&wave1::resetUnportedHitsForTests, &kernel::resetUnportedHitsForTests);

	wave1::resetUnportedHitsForTests();
	EXPECT_THROW(wave1::quest::unportedHandlerHook(), wave1::UnportedException);
	EXPECT_EQ(wave1::unportedHitCount(), 1u);
}
