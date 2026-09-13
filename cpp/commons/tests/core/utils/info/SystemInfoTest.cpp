#include <gtest/gtest.h>

#include <cmath>
#include <sstream>
#include <thread>

#include <spdlog/sinks/ostream_sink.h>

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/commons/utils/info/SystemInfo.h"

using namespace aion::commons;
using namespace aion::commons::utils::info;

TEST(SystemInfoTest, MemoryFormatLikeJava) {
	EXPECT_EQ(SystemInfo::detail::formatMiB(0), "0 MiB");
	EXPECT_EQ(SystemInfo::detail::formatMiB(2560), "2,560 MiB");
	EXPECT_EQ(SystemInfo::detail::formatMiB(1234567), "1,234,567 MiB");
	EXPECT_EQ(SystemInfo::detail::formatPercent(0.25), "(25 %)");
	EXPECT_EQ(SystemInfo::detail::formatPercent(0.125), "(12 %)"); // half even
	EXPECT_EQ(SystemInfo::detail::formatPercent(0.135), "(14 %)");
	EXPECT_EQ(SystemInfo::detail::formatPercent(std::nan("")), "NaN");

	auto lines = SystemInfo::detail::formatMemoryInfo(2560, 1024, 300);
	ASSERT_EQ(lines.size(), 3u);
	EXPECT_EQ(lines[0], "Max. memory allowed: 2,560 MiB");
	EXPECT_EQ(lines[1], "├ Allocated memory:  1,024 MiB  (40 %)");
	EXPECT_EQ(lines[2], "└ Used memory:         300 MiB  (12 %)");
}

TEST(SystemInfoTest, RealValues) {
	auto system = SystemInfo::getSystemInfo();
	ASSERT_EQ(system.size(), 3u);
	EXPECT_TRUE(system[0].starts_with("OS:  ")) << system[0];
#ifdef _WIN32
	EXPECT_TRUE(system[0].starts_with("OS:  Windows ")) << system[0];
	EXPECT_NE(system[0].find(" version 10.0"), std::string::npos) << system[0];
#endif
	EXPECT_TRUE(system[1].starts_with("C++: ")) << system[1];
	EXPECT_NE(system[1].find(SystemInfo::getLanguageStandard()), std::string::npos);
	EXPECT_TRUE(system[2].starts_with("Available CPUs:     ")) << system[2];

	auto memory = SystemInfo::getMemoryInfo();
	ASSERT_EQ(memory.size(), 3u);
	EXPECT_TRUE(memory[0].starts_with("Max. memory allowed: ")) << memory[0];
	EXPECT_EQ(memory[0].find("NaN"), std::string::npos) << memory[0];
	EXPECT_EQ(memory[0].find(" 0 MiB"), std::string::npos) << memory[0];
	EXPECT_TRUE(memory[2].ends_with("%)")) << memory[2];
}

TEST(SystemInfoTest, LogAll) {
	std::ostringstream stream;
	auto sink = std::make_shared<spdlog::sinks::ostream_sink_mt>(stream);
	sink->set_pattern("%l|%v");
	logging::LoggerFactory::configure("com.aionemu.commons.utils.info.SystemInfo", {.sinks = {sink}, .additive = false});
	SystemInfo::logAll();
	std::string out = stream.str();
	EXPECT_TRUE(out.starts_with("info|OS:  ")) << out;
	EXPECT_NE(out.find("info|└ Used memory:"), std::string::npos) << out;
	logging::LoggerFactory::removeConfig("com.aionemu.commons.utils.info.SystemInfo");
}

TEST(SystemInfoTest, ProcessInformation) {
	auto start = SystemInfo::getProcessStartTime();
	auto now = std::chrono::system_clock::now();
	EXPECT_LE(start, now);
	EXPECT_GT(start, now - std::chrono::hours(1));
	EXPECT_FALSE(SystemInfo::getExecutablePath().empty());
	EXPECT_FALSE(SystemInfo::getCompilerVersion().empty());
	EXPECT_EQ(SystemInfo::getLanguageStandard(), "C++23");
}
