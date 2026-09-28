#include <gtest/gtest.h>

#include <thread>

#include <spdlog/details/os.h>

#include "aion/commons/logging/ConsoleAppender.h"
#include "aion/commons/utils/concurrent/ThreadName.h"

using namespace aion::commons;
using namespace aion::commons::logging;

namespace {

void log(spdlog::sinks::sink& sink, std::string_view text, spdlog::level::level_enum level) {
	spdlog::details::log_msg msg(spdlog::source_loc{}, "test.Logger", level, spdlog::string_view_t(text.data(), text.size()));
	sink.log(msg);
}

} // namespace

TEST(ConsoleAppenderTest, WritesPlainTextWhenRedirected) {
	std::string output;
	std::thread([&] {
		utils::concurrent::setCurrentThreadName("main");
		testing::internal::CaptureStderr(); // redirected output is not a terminal
		{
			ConsoleAppender appender("%highlight(%-5level) %gray([%thread]) - %message%n", nullptr, LevelFilter::threshold(spdlog::level::info),
				ConsoleAppender::Target::STDERR);
			EXPECT_FALSE(appender.isColorEnabled());
			log(appender, "hidden", spdlog::level::debug);
			log(appender, "Grüße", spdlog::level::info);
			appender.flush();
		}
		output = testing::internal::GetCapturedStderr();
	}).join();
	EXPECT_EQ(output, std::string("INFO  [main] - Grüße") + spdlog::details::os::default_eol);
}
