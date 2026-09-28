// ShutdownHook (m5a-plan.md F-02): the countdown of ShutdownHook.java with Java's announcement intervals (nextInterval), the delay updates of
// initShutdown, the fast exit without players, the C++ hook thread (exit, the added steps in order, the exit function) and its once-only
// start. Expectations derived by hand from ShutdownHook.java; the game operations are replaced, so no runtime or world is needed.

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include "aion/commons/utils/ExitCode.h"
#include "aion/gameserver/ShutdownHook.h"
#include "aion/gameserver/configs/main/ShutdownConfig.h"

namespace aion::gameserver {
namespace {

using namespace std::chrono_literals;

/** What the replaced operations and steps did, in order */
struct Recorder {
	std::mutex mutex;
	std::vector<std::string> events;
	std::vector<int32_t> announcements;
	int32_t sleeps = 0;

	void add(std::string event) {
		std::scoped_lock lock(mutex);
		events.push_back(std::move(event));
	}
};

Recorder& recorder() {
	static Recorder instance;
	return instance;
}

std::atomic<int32_t> exitFunctionCode{-1};

void recordExit(int32_t exitCode) {
	recorder().add("exit");
	exitFunctionCode.store(exitCode);
}

ShutdownHook::Operations recordingOperations(std::function<bool()> worldHasPlayers) {
	ShutdownHook::Operations ops;
	ops.worldHasPlayers = std::move(worldHasPlayers);
	ops.announceShutdown = [](int32_t seconds) {
		std::scoped_lock lock(recorder().mutex);
		recorder().announcements.push_back(seconds);
	};
	ops.shutdownNetwork = [] { recorder().add("network"); };
	ops.dumpStats = [] { recorder().add("stats"); };
	ops.saveData = [] { recorder().add("save"); };
	ops.shutdownRuntime = [] { recorder().add("runtime"); };
	ops.sleep = [](std::chrono::milliseconds duration) {
		EXPECT_EQ(duration, 1000ms);
		std::scoped_lock lock(recorder().mutex);
		recorder().sleeps++;
	};
	return ops;
}

/** waits until the hook thread ran its exit function (after awaitCompletion) */
bool awaitExitFunction(ShutdownHook& hook) {
	if (!hook.awaitCompletion(10s))
		return false;
	for (int i = 0; i < 1000 && exitFunctionCode.load() < 0; i++)
		std::this_thread::sleep_for(10ms);
	return exitFunctionCode.load() >= 0;
}

class ShutdownHookTest : public testing::Test {
protected:
	void SetUp() override {
		ShutdownHook::resetForTests();
		ShutdownHook::setExitFunctionForTests(&recordExit);
		exitFunctionCode.store(-1);
		std::scoped_lock lock(recorder().mutex);
		recorder().events.clear();
		recorder().announcements.clear();
		recorder().sleeps = 0;
		delay = configs::main::ShutdownConfig::DELAY.load();
	}

	void TearDown() override {
		ShutdownHook::resetForTests();
		configs::main::ShutdownConfig::DELAY.store(delay);
	}

	int32_t delay = 0;
};

TEST_F(ShutdownHookTest, NextIntervalFollowsJava) {
	// nextInterval(remaining, 5, 60): half the remaining time rounded down to a multiple of 5, between 5 (or the remaining time) and 60
	EXPECT_EQ(ShutdownHook::nextInterval(200, 5, 60), 60);
	EXPECT_EQ(ShutdownHook::nextInterval(120, 5, 60), 60);
	EXPECT_EQ(ShutdownHook::nextInterval(60, 5, 60), 30);
	EXPECT_EQ(ShutdownHook::nextInterval(37, 5, 60), 15);
	EXPECT_EQ(ShutdownHook::nextInterval(30, 5, 60), 15);
	EXPECT_EQ(ShutdownHook::nextInterval(15, 5, 60), 5);
	EXPECT_EQ(ShutdownHook::nextInterval(12, 5, 60), 5);
	EXPECT_EQ(ShutdownHook::nextInterval(10, 5, 60), 5);
	EXPECT_EQ(ShutdownHook::nextInterval(5, 5, 60), 5);
	EXPECT_EQ(ShutdownHook::nextInterval(4, 5, 60), 4);
	EXPECT_EQ(ShutdownHook::nextInterval(1, 5, 60), 1);
	EXPECT_EQ(ShutdownHook::nextInterval(0, 5, 60), 1);
}

TEST_F(ShutdownHookTest, InitShutdownUpdatesTheDelayLikeJava) {
	ShutdownHook& hook = ShutdownHook::getInstance();
	ShutdownHook::setOperationsForTests(recordingOperations([] { return false; }));
	EXPECT_FALSE(hook.isRunning());
	hook.initShutdown(commons::utils::ExitCode::RESTART, -1); // a negative delay does nothing
	EXPECT_FALSE(hook.isRunning());
	EXPECT_FALSE(hook.isExitRequested());

	// hold the hook thread's first player check until the delay updates below are done
	std::atomic<bool> release{false};
	ShutdownHook::setOperationsForTests(recordingOperations([&release] {
		while (!release.load())
			std::this_thread::sleep_for(1ms);
		return false;
	}));
	hook.initShutdown(commons::utils::ExitCode::RESTART, 10); // unset: the delay is set and System.exit starts the hook
	EXPECT_TRUE(hook.isRunning());
	EXPECT_TRUE(hook.isExitRequested());
	EXPECT_EQ(hook.getRemainingSeconds(), 10);
	hook.initShutdown(commons::utils::ExitCode::NORMAL, 3); // more than one second left: updated, no second exit
	EXPECT_EQ(hook.getRemainingSeconds(), 3);
	release.store(true);
	ASSERT_TRUE(awaitExitFunction(hook));
	EXPECT_EQ(exitFunctionCode.load(), commons::utils::ExitCode::RESTART); // the first exit code ends the process
}

TEST_F(ShutdownHookTest, CountdownAnnouncesAtJavasIntervalsWhilePlayersAreOnline) {
	configs::main::ShutdownConfig::DELAY.store(12);
	ShutdownHook::setOperationsForTests(recordingOperations([] { return true; }));
	ShutdownHook& hook = ShutdownHook::getInstance();
	hook.exit(commons::utils::ExitCode::NORMAL);
	ASSERT_TRUE(awaitExitFunction(hook));
	std::scoped_lock lock(recorder().mutex);
	// 12 (interval 1 -> nextInterval(12) = 5), 10 (-> 5), 5 (-> 5); 11, 9..6 and 4..1 are not multiples of the interval
	EXPECT_EQ(recorder().announcements, (std::vector<int32_t>{12, 10, 5}));
	EXPECT_EQ(recorder().sleeps, 12);
	EXPECT_EQ(hook.getRemainingSeconds(), 0);
	EXPECT_EQ(recorder().events, (std::vector<std::string>{"network", "stats", "save", "runtime", "exit"}));
}

TEST_F(ShutdownHookTest, TheScenariosTwoSecondCountdownAnnouncesOnce) {
	// The gate's case 7 (m5a-plan.md §5.7 Q7) stops the server with gameserver.shutdown.delay=2 while a player is online and expects exactly
	// one SM_SYSTEM_MESSAGE STR_SERVER_SHUTDOWN before the socket closes: second 2 is announced (interval 1), nextInterval(2, 5, 60) is 2, so
	// second 1 is not a multiple of it and the loop ends at 0 without a second announcement.
	EXPECT_EQ(ShutdownHook::nextInterval(2, 5, 60), 2);
	configs::main::ShutdownConfig::DELAY.store(2);
	ShutdownHook::setOperationsForTests(recordingOperations([] { return true; }));
	ShutdownHook& hook = ShutdownHook::getInstance();
	hook.exit(commons::utils::ExitCode::NORMAL);
	ASSERT_TRUE(awaitExitFunction(hook));
	std::scoped_lock lock(recorder().mutex);
	EXPECT_EQ(recorder().announcements, (std::vector<int32_t>{2}));
	EXPECT_EQ(recorder().sleeps, 2);
	// the network (and with it the player's connection and the logout that saves the row of Q7) goes down after the countdown, not before
	EXPECT_EQ(recorder().events, (std::vector<std::string>{"network", "stats", "save", "runtime", "exit"}));
}

TEST_F(ShutdownHookTest, WithoutPlayersTheHookTakesTheFastExit) {
	configs::main::ShutdownConfig::DELAY.store(120);
	ShutdownHook::setOperationsForTests(recordingOperations([] { return false; }));
	ShutdownHook& hook = ShutdownHook::getInstance();
	hook.setBeforeRuntimeShutdown([] { recorder().add("before runtime"); });
	hook.setAfterRuntimeShutdown([] { recorder().add("after runtime"); });
	hook.exit(commons::utils::ExitCode::NORMAL);
	ASSERT_TRUE(awaitExitFunction(hook));
	EXPECT_EQ(exitFunctionCode.load(), commons::utils::ExitCode::NORMAL);
	std::scoped_lock lock(recorder().mutex);
	EXPECT_TRUE(recorder().announcements.empty());
	EXPECT_EQ(recorder().sleeps, 0);
	EXPECT_EQ(hook.getRemainingSeconds(), 120); // Java: the delay is set, then the loop breaks at once
	EXPECT_EQ(recorder().events, (std::vector<std::string>{"network", "stats", "save", "before runtime", "runtime", "after runtime", "exit"}));
}

TEST_F(ShutdownHookTest, ExitStartsTheHookOnce) {
	std::atomic<int32_t> networkShutdowns{0};
	ShutdownHook::Operations ops = recordingOperations([] { return false; });
	ops.shutdownNetwork = [&networkShutdowns] { networkShutdowns.fetch_add(1); };
	ShutdownHook::setOperationsForTests(ops);
	ShutdownHook& hook = ShutdownHook::getInstance();
	hook.exit(commons::utils::ExitCode::ERROR_);
	hook.exit(commons::utils::ExitCode::NORMAL);
	ASSERT_TRUE(awaitExitFunction(hook));
	EXPECT_EQ(networkShutdowns.load(), 1);
	EXPECT_EQ(exitFunctionCode.load(), commons::utils::ExitCode::ERROR_);
}

TEST_F(ShutdownHookTest, TheReportedExitCodeIsTheOneTheProcessEndsWith) {
	// m5a_summary.txt reports getExitCode() (main.cpp), so a run that ends with RESTART must not be summarized as exit code 0
	ShutdownHook& hook = ShutdownHook::getInstance();
	EXPECT_EQ(hook.getExitCode(), commons::utils::ExitCode::NORMAL); // no shutdown requested yet
	std::atomic<int32_t> codeSeenByTheStep{-1};
	ShutdownHook::setOperationsForTests(recordingOperations([] { return false; }));
	hook.setAfterRuntimeShutdown([&codeSeenByTheStep] {
		recorder().add("after runtime");
		codeSeenByTheStep.store(ShutdownHook::getInstance().getExitCode());
	});
	hook.exit(commons::utils::ExitCode::RESTART);
	hook.exit(commons::utils::ExitCode::NORMAL); // the first exit code wins (Java: the first System.exit ends the process)
	ASSERT_TRUE(awaitExitFunction(hook));
	EXPECT_EQ(codeSeenByTheStep.load(), commons::utils::ExitCode::RESTART);
	EXPECT_EQ(hook.getExitCode(), commons::utils::ExitCode::RESTART);
	EXPECT_EQ(exitFunctionCode.load(), commons::utils::ExitCode::RESTART);
}

TEST_F(ShutdownHookTest, ExceptionsOfACountdownSecondAreLoggedAndTheCountdownGoesOn) {
	configs::main::ShutdownConfig::DELAY.store(3);
	std::atomic<int32_t> checks{0};
	ShutdownHook::setOperationsForTests(recordingOperations([&checks]() -> bool {
		if (checks.fetch_add(1) == 0)
			throw std::runtime_error("world not available");
		return true;
	}));
	ShutdownHook& hook = ShutdownHook::getInstance();
	hook.exit(commons::utils::ExitCode::NORMAL);
	ASSERT_TRUE(hook.awaitCompletion(10s));
	std::scoped_lock lock(recorder().mutex);
	// the first second threw before its sleep and did not count down; then 3 (interval 1 -> nextInterval(3) = 3), 2, 1 without announcements
	EXPECT_EQ(recorder().announcements, (std::vector<int32_t>{3}));
	EXPECT_EQ(recorder().sleeps, 3);
	EXPECT_EQ(checks.load(), 4);
}

} // namespace
} // namespace aion::gameserver
