#include <gtest/gtest.h>

#include <sstream>

#include <spdlog/sinks/ostream_sink.h>

#include "aion/BuildInfo.h"
#include "aion/commons/logging/LoggerFactory.h"
#include "aion/commons/utils/info/VersionInfo.h"

using namespace aion::commons;
using namespace aion::commons::utils::info;
using namespace std::chrono;

namespace {

const time_zone* utc() {
	try {
		return locate_zone("UTC");
	} catch (const std::exception&) {
		return nullptr;
	}
}

} // namespace

TEST(VersionInfoTest, BuildInfoFormatLikeJava) {
	VersionInfo info("game-server.jar", "abc123-DIRTY", "develop", sys_days(2026y / September / 12) + 13h + 42min + 30s, "C++23 (MSVC 19.51.1)");
	if (!utc())
		GTEST_SKIP() << "no time zone database available";
	EXPECT_EQ(info.getBuildInfo(utc()), "revision abc123-DIRTY (develop) built on 2026-09-12 13:42 for C++23 (MSVC 19.51.1)");
	EXPECT_EQ(info.toString(utc()), "game-server.jar revision abc123-DIRTY (develop) built on 2026-09-12 13:42 for C++23 (MSVC 19.51.1)");

	VersionInfo noBranch("x", "abc", std::nullopt, sys_days(2026y / September / 12) + 0s, std::nullopt);
	EXPECT_EQ(noBranch.getBuildInfo(utc()), "revision abc built on 2026-09-12 00:00");
	VersionInfo noRevision("x", std::nullopt, "main", std::nullopt, std::nullopt);
	EXPECT_EQ(noRevision.getBuildInfo(utc()), "built on unknown date");
}

TEST(VersionInfoTest, FromBuildInfo) {
	VersionInfo info = VersionInfo::fromBuildInfo("server.exe", "unknown", "", "2026-09-12T15:42:30Z");
	EXPECT_EQ(info.getSource(), "server.exe");
	EXPECT_FALSE(info.getRevision().has_value());
	EXPECT_FALSE(info.getBranch().has_value());
	ASSERT_TRUE(info.getBuildDate().has_value());
	EXPECT_EQ(*info.getBuildDate(), sys_days(2026y / September / 12) + 15h + 42min + 30s);
	ASSERT_TRUE(info.getBuildTarget().has_value());
	EXPECT_TRUE(info.getBuildTarget()->starts_with("C++2")) << *info.getBuildTarget();

	VersionInfo local = VersionInfo::fromBuildInfo("server.exe", "rev", "branch", "2026-01-15T12:00:00");
	ASSERT_TRUE(local.getBuildDate().has_value());
	EXPECT_EQ(local.toString(nullptr).find("built on 2026-01-15 12:00"), std::string("server.exe revision rev (branch) ").size())
		<< local.toString(nullptr); // local build time formatted in the local zone again
	EXPECT_FALSE(VersionInfo::fromBuildInfo("x", "", "", "garbage").getBuildDate().has_value());
	EXPECT_FALSE(VersionInfo::fromBuildInfo("x", "", "", "2026-13-01T00:00:00").getBuildDate().has_value());
}

TEST(VersionInfoTest, CommonsAndExecutable) {
	const VersionInfo& commons = VersionInfo::commons();
	EXPECT_EQ(commons.getSource(), "aion_commons");
	EXPECT_TRUE(commons.getBuildDate().has_value());
	EXPECT_EQ(commons.getRevision().value_or("unknown"), aion::build::REVISION);

	VersionInfo executable = VersionInfo::ofExecutable();
	EXPECT_TRUE(executable.getSource().starts_with("aion_commons_core_tests")) << executable.getSource();
}

TEST(VersionInfoTest, LogAllAlignsSources) {
	std::ostringstream stream;
	auto sink = std::make_shared<spdlog::sinks::ostream_sink_mt>(stream);
	sink->set_pattern("%v");
	logging::LoggerFactory::configure("com.aionemu.commons.utils.info.VersionInfo", {.sinks = {sink}, .additive = false});

	VersionInfo::logAll(VersionInfo("a-much-longer-source.exe", "r", std::nullopt, sys_days(2026y / September / 12) + 0s, std::nullopt), nullptr);

	std::istringstream lines(stream.str());
	std::string first, second;
	std::getline(lines, first);
	std::getline(lines, second);
	EXPECT_TRUE(first.starts_with("            aion_commons revision ")) << first;
	EXPECT_TRUE(second.starts_with("a-much-longer-source.exe revision r built on 2026-09-1")) << second;
	logging::LoggerFactory::removeConfig("com.aionemu.commons.utils.info.VersionInfo");
}
