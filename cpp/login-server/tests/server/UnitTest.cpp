// Unit tests of login server classes that need neither the network nor the database.

#include <atomic>
#include <chrono>
#include <cstdint>
#include <string>
#include <thread>
#include <vector>

#include <gtest/gtest.h>

#include "ServerTestUtils.h"
#include "aion/commons/utils/Exception.h"
#include "aion/commons/utils/TimeUtils.h"
#include "aion/loginserver/LoginServer.h"
#include "aion/loginserver/configs/Config.h"
#include "aion/loginserver/controller/AccountTimeController.h"
#include "aion/loginserver/model/Account.h"
#include "aion/loginserver/network/aion/AionAuthResponse.h"
#include "aion/loginserver/network/aion/SessionKey.h"
#include "aion/loginserver/network/aion/clientpackets/CM_LOGIN.h"
#include "aion/loginserver/utils/BruteForceProtector.h"
#include "aion/loginserver/utils/ExternalAuth.h"
#include "aion/loginserver/utils/ScheduledExecutor.h"

namespace aion::loginserver::test {
namespace {

using namespace std::chrono_literals;
using commons::database::Timestamp;
using configs::Config;
using network::aion::AionAuthResponse;
using network::aion::clientpackets::CM_LOGIN;

Timestamp timestampOf(int64_t millis) {
	return Timestamp(std::chrono::milliseconds(millis));
}

// ---- ScheduledExecutor ----

TEST(ScheduledExecutorTest, RunsAtFixedRateUntilCancelled) {
	utils::ScheduledExecutor executor("TestScheduler");
	std::atomic<int> runs = 0;
	auto start = std::chrono::steady_clock::now();
	std::atomic<int64_t> firstRunMillis = -1;
	auto future = executor.scheduleAtFixedRate(
		[&] {
			if (runs++ == 0)
				firstRunMillis = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count();
		},
		50ms, 20ms);
	ASSERT_TRUE(waitUntil([&] { return runs >= 5; }, 5s));
	EXPECT_GE(firstRunMillis.load(), 45);
	future->cancel();
	EXPECT_TRUE(future->isCancelled());
	std::this_thread::sleep_for(60ms); // a run in progress may still finish
	int runsAfterCancel = runs;
	std::this_thread::sleep_for(100ms);
	EXPECT_EQ(runs.load(), runsAfterCancel);
}

TEST(ScheduledExecutorTest, TaskCanCancelItselfAndExceptionsStopOnlyTheFailingTask) {
	LogCapture logs("com.aionemu.loginserver.utils.ScheduledExecutor");
	utils::ScheduledExecutor executor("TestScheduler");
	std::atomic<int> selfCancelling = 0, failing = 0, healthy = 0;
	executor.scheduleAtFixedRate(std::function<void(utils::ScheduledExecutor::ScheduledFuture&)>([&](utils::ScheduledExecutor::ScheduledFuture& future) {
		if (++selfCancelling == 2)
			future.cancel();
	}),
		0ms, 10ms);
	executor.scheduleAtFixedRate([&] {
		failing++;
		throw commons::utils::IllegalStateException("task failure");
	},
		0ms, 10ms);
	executor.scheduleAtFixedRate([&] { healthy++; }, 0ms, 10ms);
	ASSERT_TRUE(waitUntil([&] { return healthy >= 10; }));
	EXPECT_EQ(selfCancelling.load(), 2);
	EXPECT_EQ(failing.load(), 1);
	EXPECT_TRUE(logs.contains("Scheduled task of TestScheduler failed and will not run again"));
	EXPECT_TRUE(logs.contains("task failure"));
}

TEST(ScheduledExecutorTest, ShutdownStopsTasksAndRejectsNewOnes) {
	utils::ScheduledExecutor executor("TestScheduler");
	std::atomic<bool> running = false;
	std::atomic<bool> finished = false;
	executor.scheduleAtFixedRate([&] {
		running = true;
		std::this_thread::sleep_for(200ms);
		finished = true;
	},
		0ms, 1000ms);
	ASSERT_TRUE(waitUntil([&] { return running.load(); }));
	executor.shutdown();
	EXPECT_FALSE(executor.isTerminated()); // still running the task
	EXPECT_TRUE(executor.awaitTermination(5s));
	EXPECT_TRUE(finished.load());
	EXPECT_TRUE(executor.isTerminated());
	EXPECT_THROW(executor.scheduleAtFixedRate([] {}, 0ms, 10ms), commons::utils::IllegalStateException);
	executor.shutdown(); // again: nothing happens
}

TEST(ScheduledExecutorTest, InvalidArguments) {
	utils::ScheduledExecutor executor("TestScheduler");
	EXPECT_THROW(executor.scheduleAtFixedRate([] {}, 0ms, 0ms), commons::utils::IllegalArgumentException);
	EXPECT_THROW(executor.scheduleAtFixedRate(std::function<void()>(), 0ms, 10ms), commons::utils::IllegalArgumentException);
}

// ---- SessionKey / AionAuthResponse ----

TEST(SessionKeyTest, ChecksAndRandomKeys) {
	network::aion::SessionKey key(5, 10, 20, 30);
	EXPECT_TRUE(key.checkLogin(5, 10));
	EXPECT_FALSE(key.checkLogin(5, 11));
	EXPECT_FALSE(key.checkLogin(6, 10));
	EXPECT_TRUE(key.checkSessionKey({5, 10, 20, 30}));
	EXPECT_FALSE(key.checkSessionKey({5, 10, 20, 31}));
	EXPECT_FALSE(key.checkSessionKey({5, 10, 21, 30}));
	EXPECT_FALSE(key.checkSessionKey({5, 11, 20, 30}));
	EXPECT_FALSE(key.checkSessionKey({4, 10, 20, 30}));

	model::Account account;
	account.setId(42);
	network::aion::SessionKey random1(account), random2(account);
	EXPECT_EQ(random1.accountId, 42);
	EXPECT_FALSE(random1.checkSessionKey(random2)); // 96 random bits

	model::Account withoutId;
	EXPECT_THROW(network::aion::SessionKey{withoutId}, std::bad_optional_access);
}

TEST(AionAuthResponseTest, IdsAndLookup) {
	EXPECT_EQ(network::aion::getId(AionAuthResponse::STR_L2AUTH_S_ALL_OK), 0);
	EXPECT_EQ(network::aion::getId(AionAuthResponse::STR_L2AUTH_S_INCORRECT_PWD), 3);
	EXPECT_EQ(network::aion::getId(AionAuthResponse::STR_L2AUTH_S_BLOCKED_IP), 22);
	EXPECT_EQ(network::aion::getId(AionAuthResponse::STR_L2AUTH_S_AGREE_GAME), 37);
	EXPECT_EQ(network::aion::getId(AionAuthResponse::STR_L2AUTH_UNKNOWN4), 60);
	EXPECT_EQ(network::aion::getId(AionAuthResponse::STR_L2AUTH_S_ACCOUNTCACHESERVER_DOWN), 62);
	for (int32_t id = 0; id <= 62; id++)
		EXPECT_EQ(network::aion::getId(network::aion::getByIdOrDefault(id, AionAuthResponse::STR_L2AUTH_UNKNOWN4)), id);
	EXPECT_EQ(network::aion::getByIdOrDefault(63, AionAuthResponse::STR_L2AUTH_UNKNOWN4), AionAuthResponse::STR_L2AUTH_UNKNOWN4);
	EXPECT_EQ(network::aion::getByIdOrDefault(-1, AionAuthResponse::STR_L2AUTH_S_SYSTEM_ERROR), AionAuthResponse::STR_L2AUTH_S_SYSTEM_ERROR);
}

// ---- CM_LOGIN login data ----

TEST(CmLoginTest, ParsesNormalLoginDataLikeTheJavadocExample) {
	std::vector<uint8_t> data = AionLoginClientCrypto::buildLoginData("abcdefghijklmn", "abcdefghijklmnop", -1, false);
	CM_LOGIN::LoginData loginData = CM_LOGIN::parseLoginData(data);
	EXPECT_EQ(loginData.username, "abcdefghijklmn");
	EXPECT_EQ(loginData.password, "abcdefghijklmnop");
	EXPECT_EQ(loginData.otp, -1);

	loginData = CM_LOGIN::parseLoginData(AionLoginClientCrypto::buildLoginData("user", "pw", 123456, false));
	EXPECT_EQ(loginData.username, "user");
	EXPECT_EQ(loginData.password, "pw");
	EXPECT_EQ(loginData.otp, 123456);

	// excessive characters are truncated by the client, so the fields are full and not zero terminated
	loginData = CM_LOGIN::parseLoginData(AionLoginClientCrypto::buildLoginData("abcdefghijklmnopq", "abcdefghijklmnopqrst", -1, false));
	EXPECT_EQ(loginData.username, "abcdefghijklmn");
	EXPECT_EQ(loginData.password, "abcdefghijklmnop");
}

TEST(CmLoginTest, ParsesLoginExData) {
	const std::string user = "abcdefghijklmnopqrsabcdefghijklmnopqrsabcdefghijklmno3432432pqrs";
	const std::string password = "11111111111111111111111111111111";
	CM_LOGIN::LoginData loginData = CM_LOGIN::parseLoginData(AionLoginClientCrypto::buildLoginData(user, password, -1, true));
	EXPECT_EQ(loginData.username, user);
	EXPECT_EQ(loginData.password, password);
	EXPECT_EQ(loginData.otp, -1);

	loginData = CM_LOGIN::parseLoginData(AionLoginClientCrypto::buildLoginData("short", "pw", 7, true));
	EXPECT_EQ(loginData.username, "short");
	EXPECT_EQ(loginData.password, "pw");
	EXPECT_EQ(loginData.otp, 7);
}

TEST(CmLoginTest, ShortDataThrowsLikeJava) {
	EXPECT_THROW(CM_LOGIN::parseLoginData({}), commons::utils::IndexOutOfBoundsException);
	EXPECT_THROW(CM_LOGIN::parseLoginData(std::vector<uint8_t>(20)), commons::utils::IndexOutOfBoundsException);
}

TEST(CmLoginTest, DecodesCp1252) {
	std::vector<uint8_t> bytes = {'A', 0x80, 0x81, 0x9F, 0xA0, 0xE9, 0xFF};
	EXPECT_EQ(CM_LOGIN::decodeCp1252(bytes), "A€�Ÿ éÿ");
	std::vector<uint8_t> login = AionLoginClientCrypto::buildLoginData("j\xE9r\xF4me", "p\x80ss", -1, false);
	CM_LOGIN::LoginData loginData = CM_LOGIN::parseLoginData(login);
	EXPECT_EQ(loginData.username, "jérôme");
	EXPECT_EQ(loginData.password, "p€ss");
}

// ---- BruteForceProtector ----

TEST(BruteForceProtectorTest, BansAfterTheAllowedTries) {
	int32_t oldTries = Config::LOGIN_TRY_BEFORE_BAN;
	int32_t oldBanTime = Config::WRONG_LOGIN_BAN_TIME;
	Config::LOGIN_TRY_BEFORE_BAN = 3;
	Config::WRONG_LOGIN_BAN_TIME = 15;
	auto& protector = utils::BruteForceProtector::getInstance();
	static int run = 0; // the protector is a singleton: unique keys per test run (--gtest_repeat)
	const std::string suffix = "-" + std::to_string(++run);
	const std::string ip = "unit-test-ip-1" + suffix;
	EXPECT_FALSE(protector.addFailedConnect(ip)); // 1
	EXPECT_FALSE(protector.addFailedConnect(ip)); // 2
	EXPECT_FALSE(protector.addFailedConnect(ip)); // 3
	EXPECT_TRUE(protector.addFailedConnect(ip)); // count reached: ban, series removed
	EXPECT_FALSE(protector.addFailedConnect(ip)); // new series
	EXPECT_FALSE(protector.addFailedConnect("unit-test-ip-2" + suffix));

	// a series older than the ban time starts again
	Config::WRONG_LOGIN_BAN_TIME = 0;
	const std::string ip3 = "unit-test-ip-3" + suffix;
	EXPECT_FALSE(protector.addFailedConnect(ip3));
	std::this_thread::sleep_for(5ms);
	EXPECT_FALSE(protector.addFailedConnect(ip3));
	std::this_thread::sleep_for(5ms);
	EXPECT_FALSE(protector.addFailedConnect(ip3));
	std::this_thread::sleep_for(5ms);
	EXPECT_FALSE(protector.addFailedConnect(ip3));
	std::this_thread::sleep_for(5ms);
	EXPECT_FALSE(protector.addFailedConnect(ip3));

	Config::LOGIN_TRY_BEFORE_BAN = oldTries;
	Config::WRONG_LOGIN_BAN_TIME = oldBanTime;
}

TEST(BruteForceProtectorTest, ConcurrentFailures) {
	int32_t oldTries = Config::LOGIN_TRY_BEFORE_BAN;
	int32_t oldBanTime = Config::WRONG_LOGIN_BAN_TIME;
	Config::LOGIN_TRY_BEFORE_BAN = 9;
	Config::WRONG_LOGIN_BAN_TIME = 15;
	std::atomic<int> bans = 0;
	static int run = 0;
	const std::string ip = "unit-test-concurrent-" + std::to_string(++run);
	std::vector<std::thread> threads;
	for (int t = 0; t < 8; t++)
		threads.emplace_back([&] {
			for (int i = 0; i < 1000; i++)
				if (utils::BruteForceProtector::getInstance().addFailedConnect(ip))
					bans++;
		});
	for (auto& thread : threads)
		thread.join();
	EXPECT_EQ(bans.load(), 800); // every 10th failure bans
	Config::LOGIN_TRY_BEFORE_BAN = oldTries;
	Config::WRONG_LOGIN_BAN_TIME = oldBanTime;
}

// ---- AccountTimeController ----

TEST(AccountTimeControllerTest, ExpirationAndPenalty) {
	using controller::AccountTimeController::isAccountExpired;
	using controller::AccountTimeController::isAccountPenaltyActive;
	int64_t now = commons::utils::currentTimeMillis();
	model::Account account;
	EXPECT_THROW(isAccountExpired(account), commons::utils::IllegalStateException);
	EXPECT_THROW(isAccountPenaltyActive(account), commons::utils::IllegalStateException);

	model::AccountTime time;
	account.setAccountTime(time);
	EXPECT_FALSE(isAccountExpired(account));
	EXPECT_FALSE(isAccountPenaltyActive(account));

	time.setExpirationTime(timestampOf(now - 1000));
	time.setPenaltyEnd(timestampOf(now + 60000));
	account.setAccountTime(time);
	EXPECT_TRUE(isAccountExpired(account));
	EXPECT_TRUE(isAccountPenaltyActive(account));

	time.setExpirationTime(timestampOf(now + 60000));
	time.setPenaltyEnd(timestampOf(now - 1000));
	account.setAccountTime(time);
	EXPECT_FALSE(isAccountExpired(account));
	EXPECT_FALSE(isAccountPenaltyActive(account));

	time.setPenaltyEnd(timestampOf(1000)); // infinite
	account.setAccountTime(time);
	EXPECT_TRUE(isAccountPenaltyActive(account));

	EXPECT_EQ(controller::AccountTimeController::getDays(0), 0);
	EXPECT_EQ(controller::AccountTimeController::getDays(86399999), 0);
	EXPECT_EQ(controller::AccountTimeController::getDays(86400000), 1);
	EXPECT_EQ(controller::AccountTimeController::getDays(1757700000000), 20343);
}

// ---- ExternalAuth ----

TEST(ExternalAuthTest, ParsesResponsesLikeFastjson) {
	using utils::ExternalAuth::parseResponse;
	using utils::ExternalAuth::Response;
	EXPECT_EQ(parseResponse(R"({"accountId":"admin","aionAuthResponseId":0})"), (Response{"admin", 0}));
	EXPECT_EQ(parseResponse(R"({"aionAuthResponseId":3, "unknown": [1, 2]})"), (Response{std::nullopt, 3}));
	EXPECT_EQ(parseResponse(R"({"accountId":null})"), (Response{std::nullopt, 0}));
	EXPECT_EQ(parseResponse(R"({"accountId":12345,"aionAuthResponseId":"22"})"), (Response{"12345", 22}));
	EXPECT_EQ(parseResponse(R"({"accountId":"x","aionAuthResponseId":7.0})"), (Response{"x", 7}));
	EXPECT_EQ(parseResponse("{}"), (Response{}));
	EXPECT_EQ(parseResponse(""), std::nullopt);
	EXPECT_EQ(parseResponse("  "), std::nullopt);
	EXPECT_EQ(parseResponse("null"), std::nullopt);
	EXPECT_ANY_THROW(parseResponse("[1]"));
	EXPECT_ANY_THROW(parseResponse("{"));
	EXPECT_ANY_THROW(parseResponse(R"({"accountId":true})"));
	EXPECT_ANY_THROW(parseResponse(R"({"aionAuthResponseId":"abc"})"));
	EXPECT_ANY_THROW(parseResponse(R"({"aionAuthResponseId":4294967296})"));
}

// ---- LoginServer ----

TEST(LoginServerTest, ParsesOverrideArguments) {
	std::vector<std::string_view> args = {"-Ddatabase.url=jdbc:mysql://localhost:3306/aion_ls_run", "-Dloginserver.network.nio.threads=2", "-Dx==y",
		"--help", "-D=novalue", "-Dmissing"};
	std::vector<std::string> unknown;
	commons::configuration::Properties overrides = LoginServer::parseOverrides(args, unknown);
	EXPECT_EQ(overrides.size(), 3u);
	EXPECT_EQ(overrides.getProperty("database.url"), "jdbc:mysql://localhost:3306/aion_ls_run");
	EXPECT_EQ(overrides.getProperty("loginserver.network.nio.threads"), "2");
	EXPECT_EQ(overrides.getProperty("x"), "=y");
	EXPECT_EQ(unknown, (std::vector<std::string>{"--help", "-D=novalue", "-Dmissing"}));
}

} // namespace
} // namespace aion::loginserver::test
