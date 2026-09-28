#include <gtest/gtest.h>

#include <sstream>
#include <string>
#include <vector>

#include <spdlog/sinks/ostream_sink.h>

#ifdef _WIN32
#include <windows.h>
#endif

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/commons/utils/Exception.h"
#include "aion/commons/utils/concurrent/PriorityThreadFactory.h"
#include "aion/commons/utils/concurrent/ThreadName.h"

using namespace aion::commons;
using namespace aion::commons::utils::concurrent;

TEST(PriorityThreadFactoryTest, NamesAndPriorities) {
	PriorityThreadFactory factory("InstantPool", 7);
	std::vector<std::string> names(3);
	std::vector<int> priorities(3);
	{
		std::vector<std::jthread> threads;
		for (size_t i = 0; i < names.size(); i++) {
			threads.push_back(factory.newThread([&names, &priorities, i] {
				names[i] = getCurrentThreadName();
#ifdef _WIN32
				priorities[i] = GetThreadPriority(GetCurrentThread());
#endif
			}));
		}
	} // jthreads join
	EXPECT_EQ(names, (std::vector<std::string>{"InstantPool-1", "InstantPool-2", "InstantPool-3"}));
#ifdef _WIN32
	for (int priority : priorities)
		EXPECT_EQ(priority, THREAD_PRIORITY_ABOVE_NORMAL);
#endif
	EXPECT_EQ(factory.getName(), "InstantPool");
	EXPECT_EQ(factory.getPriority(), 7);
}

TEST(PriorityThreadFactoryTest, InvalidPriorityThrows) {
	PriorityThreadFactory factory("Invalid", 11);
	EXPECT_THROW(static_cast<void>(factory.newThread([] {})), utils::IllegalArgumentException);
	EXPECT_THROW(setCurrentThreadPriority(0), utils::IllegalArgumentException);
	std::jthread([] { EXPECT_NO_THROW(setCurrentThreadPriority(MIN_PRIORITY)); }).join();
}

TEST(PriorityThreadFactoryTest, UncaughtExceptionsAreLoggedAndEndOnlyTheThread) {
	std::ostringstream stream;
	auto sink = std::make_shared<spdlog::sinks::ostream_sink_mt>(stream);
	sink->set_pattern("%l|%v");
	logging::LoggerFactory::configure("com.aionemu.commons.utils.concurrent.UncaughtExceptionHandler", {.sinks = {sink}, .additive = false});

	PriorityThreadFactory factory("Crashing", NORM_PRIORITY);
	factory.newThread([] { throw utils::IllegalStateException("thread failure"); }).join();

	std::string out = stream.str();
	EXPECT_TRUE(out.starts_with("error|Critical Error - Thread [Crashing-1] terminated abnormally:\n"
															"aion::commons::utils::IllegalStateException: thread failure"))
		<< out;
	logging::LoggerFactory::removeConfig("com.aionemu.commons.utils.concurrent.UncaughtExceptionHandler");
}
