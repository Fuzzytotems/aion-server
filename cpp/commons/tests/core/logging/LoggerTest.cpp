#include <gtest/gtest.h>

#include <atomic>
#include <mutex>
#include <sstream>
#include <stdexcept>
#include <thread>
#include <vector>

#include <spdlog/sinks/ostream_sink.h>

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/commons/utils/Exception.h"

using namespace aion::commons;
using namespace aion::commons::logging;

namespace {

struct CapturingSink {
	std::ostringstream stream;
	std::shared_ptr<spdlog::sinks::ostream_sink_mt> sink = std::make_shared<spdlog::sinks::ostream_sink_mt>(stream);
	CapturingSink() { sink->set_pattern("%l|%n|%v"); }
};

/** Removes a test logger configuration again, so no logger keeps a sink that writes to a destroyed stream. */
struct ConfigGuard {
	std::string name;
	~ConfigGuard() { LoggerFactory::removeConfig(name); }
};

void failingFunction() {
	throw utils::IllegalStateException("inner problem");
}

} // namespace

TEST(LoggerTest, CallFormsCompileAndFormat) {
	CapturingSink capture;
	ConfigGuard guard{"test.callforms"};
	LoggerFactory::configure("test.callforms", {.level = spdlog::level::debug, .sinks = {capture.sink}, .additive = false});
	auto log = LoggerFactory::getLogger("test.callforms.Child");

	std::string name = "Bob";
	log.info("plain");
	log.info("Hello {} #{}", name, 5);
	log.debug(std::string("runtime ") + name);
	log.trace("filtered out");
	try {
		try {
			failingFunction();
		} catch (...) {
			throw utils::Exception("outer problem", std::current_exception());
		}
	} catch (const utils::Exception& e) {
		log.error("Error for " + name, e);
		log.warn("", e);
	}

	std::string out = capture.stream.str();
	EXPECT_NE(out.find("info|test.callforms.Child|plain"), std::string::npos);
	EXPECT_NE(out.find("Hello Bob #5"), std::string::npos);
	EXPECT_NE(out.find("debug|test.callforms.Child|runtime Bob"), std::string::npos);
	EXPECT_EQ(out.find("filtered out"), std::string::npos);
	EXPECT_NE(out.find("Error for Bob\naion::commons::utils::Exception: outer problem"), std::string::npos);
	EXPECT_NE(out.find("Caused by: aion::commons::utils::IllegalStateException: inner problem"), std::string::npos);
	EXPECT_NE(out.find("\tat "), std::string::npos); // stack trace frames
}

TEST(LoggerTest, MostSpecificConfigWins) {
	CapturingSink parent;
	CapturingSink child;
	ConfigGuard parentGuard{"test.hierarchy"};
	ConfigGuard childGuard{"test.hierarchy.a"};
	LoggerFactory::configure("test.hierarchy", {.level = spdlog::level::warn, .sinks = {parent.sink}, .additive = false});
	auto log = LoggerFactory::getLogger("test.hierarchy.a.b.C");
	log.info("not logged");
	log.warn("logged to parent");
	LoggerFactory::configure("test.hierarchy.a", {.level = spdlog::level::info, .sinks = {child.sink}, .additive = false});
	log.info("logged to child"); // existing logger is reconfigured

	EXPECT_EQ(parent.stream.str(), "warning|test.hierarchy.a.b.C|logged to parent" + std::string(spdlog::details::os::default_eol));
	EXPECT_NE(child.stream.str().find("logged to child"), std::string::npos);
}

TEST(LoggerTest, TrailingExceptionAndCurrentExceptionForms) {
	CapturingSink capture;
	ConfigGuard guard{"test.exceptionforms"};
	LoggerFactory::configure("test.exceptionforms", {.level = spdlog::level::trace, .sinks = {capture.sink}, .additive = false});
	auto log = LoggerFactory::getLogger("test.exceptionforms");
	std::runtime_error e("disk full");

	// slf4j: log.error("Could not save script data for houseId: {}", houseId, e)
	log.error("Could not save house {} of {}", 42, "Bob", e);
	log.warn("Retry {}", 3, e);
	log.info("Info {}", "x", e);
	log.debug("Debug {}", 1.5, e);
	log.trace("Trace {}", true, e);
	try {
		throw 7;
	} catch (...) {
		log.warnCurrentException("Unknown failure");
	}
	try {
		throw e;
	} catch (...) {
		log.infoCurrentException("Known failure");
		log.debugCurrentException("");
		log.traceCurrentException("t");
	}

	std::string out = capture.stream.str();
	EXPECT_NE(out.find("error|test.exceptionforms|Could not save house 42 of Bob\nstd::runtime_error: disk full"), std::string::npos) << out;
	EXPECT_NE(out.find("warning|test.exceptionforms|Retry 3\nstd::runtime_error: disk full"), std::string::npos) << out;
	EXPECT_NE(out.find("info|test.exceptionforms|Info x\nstd::runtime_error: disk full"), std::string::npos) << out;
	EXPECT_NE(out.find("debug|test.exceptionforms|Debug 1.5\nstd::runtime_error: disk full"), std::string::npos) << out;
	EXPECT_NE(out.find("trace|test.exceptionforms|Trace true\nstd::runtime_error: disk full"), std::string::npos) << out;
	EXPECT_NE(out.find("warning|test.exceptionforms|Unknown failure\n<unknown exception type>"), std::string::npos) << out;
	EXPECT_NE(out.find("info|test.exceptionforms|Known failure\nstd::runtime_error: disk full"), std::string::npos) << out;
	EXPECT_NE(out.find("debug|test.exceptionforms|std::runtime_error: disk full"), std::string::npos) << out;
	EXPECT_NE(out.find("trace|test.exceptionforms|t\nstd::runtime_error: disk full"), std::string::npos) << out;
}

TEST(LoggerTest, HierarchyLikeLogback) {
	CapturingSink services;
	CapturingSink mail;
	CapturingSink isolated;
	ConfigGuard servicesGuard{"test.logback.services"};
	ConfigGuard mailGuard{"test.logback.services.mail"};
	ConfigGuard isolatedGuard{"test.logback.services.mail.isolated"};
	LoggerFactory::configure("test.logback.services", {.level = spdlog::level::debug, .sinks = {services.sink}, .additive = false});
	LoggerFactory::configure("test.logback.services.mail", {.sinks = {mail.sink}});
	auto log = LoggerFactory::getLogger("test.logback.services.mail.MailService");

	// the level is inherited from the nearest ancestor with a level, the sinks of all ancestors up to additivity="false" are used
	log.debug("debug message");
	EXPECT_NE(mail.stream.str().find("debug|test.logback.services.mail.MailService|debug message"), std::string::npos);
	EXPECT_NE(services.stream.str().find("debug message"), std::string::npos);

	// a non-additive config stops at itself, and its own level applies
	LoggerFactory::configure("test.logback.services.mail.isolated", {.level = spdlog::level::warn, .sinks = {isolated.sink}, .additive = false});
	auto isolatedLog = LoggerFactory::getLogger("test.logback.services.mail.isolated.X");
	isolatedLog.info("filtered");
	isolatedLog.warn("isolated warning");
	EXPECT_NE(isolated.stream.str().find("isolated warning"), std::string::npos);
	EXPECT_EQ(isolated.stream.str().find("filtered"), std::string::npos);
	EXPECT_EQ(mail.stream.str().find("isolated warning"), std::string::npos);
	EXPECT_EQ(services.stream.str().find("isolated warning"), std::string::npos);

	// an empty config has no effect, removing a config restores the inherited configuration
	LoggerFactory::configure("test.logback.services.mail.MailService", {});
	log.debug("still inherited");
	EXPECT_NE(services.stream.str().find("still inherited"), std::string::npos);
	EXPECT_NE(mail.stream.str().find("still inherited"), std::string::npos);
	LoggerFactory::removeConfig("test.logback.services.mail.MailService");
	LoggerFactory::removeConfig("test.logback.services.mail");
	log.debug("after removal");
	EXPECT_EQ(mail.stream.str().find("after removal"), std::string::npos);
	EXPECT_NE(services.stream.str().find("after removal"), std::string::npos);
}

TEST(LoggerTest, ConfigureWhileOtherThreadsLog) {
	// configure() used to rebuild the sink vector of every spdlog::logger, which spdlog iterates without a lock
	CapturingSink capture;
	ConfigGuard guard{"test.concurrent"};
	ConfigGuard otherGuard{"test.concurrent.other"};
	LoggerFactory::configure("test.concurrent", {.level = spdlog::level::info, .sinks = {capture.sink}, .additive = false});
	auto log = LoggerFactory::getLogger("test.concurrent.Worker");
	std::atomic<bool> stop{false};
	std::vector<std::jthread> threads;
	for (int i = 0; i < 4; i++) {
		threads.emplace_back([&] {
			while (!stop)
				log.info("message");
		});
	}
	for (int i = 0; i < 2000; i++) {
		LoggerFactory::configure("test.concurrent.other", {.sinks = {std::make_shared<spdlog::sinks::ostream_sink_mt>(capture.stream)}});
		LoggerFactory::configure("test.concurrent", {.level = spdlog::level::info, .sinks = {capture.sink}, .additive = false});
		LoggerFactory::setRootLevel(spdlog::level::info);
	}
	stop = true;
	threads.clear();
	EXPECT_NE(capture.stream.str().find("info|test.concurrent.Worker|message"), std::string::npos);
}
