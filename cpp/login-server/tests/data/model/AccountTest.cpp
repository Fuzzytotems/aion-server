#include <atomic>
#include <chrono>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

#include <gtest/gtest.h>

#include "DataTestUtils.h"
#include "aion/commons/utils/Exception.h"
#include "aion/loginserver/model/Account.h"
#include "aion/loginserver/model/ReconnectingAccount.h"

using namespace aion::loginserver::model;
using namespace aion::loginserver::test;
using namespace std::chrono_literals;

TEST(AccountTimeTest, DefaultValues) {
	int64_t before = aion::commons::utils::currentTimeMillis();
	AccountTime time;
	int64_t after = aion::commons::utils::currentTimeMillis();
	EXPECT_GE(millis(time.getLastLoginTime()), before);
	EXPECT_LE(millis(time.getLastLoginTime()), after);
	EXPECT_EQ(time.getSessionDuration(), 0);
	EXPECT_EQ(time.getAccumulatedOnlineTime(), 0);
	EXPECT_EQ(time.getAccumulatedRestTime(), 0);
	EXPECT_FALSE(time.getExpirationTime());
	EXPECT_FALSE(time.getPenaltyEnd());
}

TEST(AccountTimeTest, SettersAndGetters) {
	AccountTime time;
	Timestamp t(123456789ms);
	time.setLastLoginTime(t);
	time.setSessionDuration(1);
	time.setAccumulatedOnlineTime(2);
	time.setAccumulatedRestTime(3);
	time.setExpirationTime(t + 1s);
	time.setPenaltyEnd(Timestamp(1000ms));
	EXPECT_EQ(time.getLastLoginTime(), t);
	EXPECT_EQ(time.getSessionDuration(), 1);
	EXPECT_EQ(time.getAccumulatedOnlineTime(), 2);
	EXPECT_EQ(time.getAccumulatedRestTime(), 3);
	EXPECT_EQ(time.getExpirationTime(), t + 1s);
	EXPECT_EQ(time.getPenaltyEnd(), Timestamp(1000ms));
	time.setPenaltyEnd(std::nullopt);
	EXPECT_FALSE(time.getPenaltyEnd());
}

TEST(AccountTest, DefaultValues) {
	Account account;
	EXPECT_FALSE(account.getId());
	EXPECT_EQ(account.getName(), "");
	EXPECT_EQ(account.getPasswordHash(), "");
	EXPECT_FALSE(account.getCreationDate());
	EXPECT_EQ(account.getAccessLevel(), 0);
	EXPECT_EQ(account.getMembership(), 0);
	EXPECT_EQ(account.getActivated(), 0);
	EXPECT_EQ(account.getLastServer(), 0);
	EXPECT_FALSE(account.getLastIp());
	EXPECT_EQ(account.getLastMac(), "xx-xx-xx-xx-xx-xx");
	EXPECT_FALSE(account.getIpForce());
	EXPECT_FALSE(account.getAllowedHddSerial());
	EXPECT_FALSE(account.getAccountTime());
}

TEST(AccountTest, SettersAndGetters) {
	Account account;
	account.setId(42);
	account.setName("admin");
	account.setPasswordHash("hash");
	account.setCreationDate(Timestamp(5000ms));
	account.setAccessLevel(-3);
	account.setMembership(2);
	account.setActivated(1);
	account.setLastServer(-1);
	account.setLastIp("10.0.0.1");
	account.setLastMac("aa-bb-cc-dd-ee-ff");
	account.setIpForce("10.0.0.*");
	account.setAllowedHddSerial("HDD1");
	AccountTime time;
	time.setAccumulatedRestTime(77);
	account.setAccountTime(time);

	EXPECT_EQ(account.getId(), 42);
	EXPECT_EQ(account.getName(), "admin");
	EXPECT_EQ(account.getPasswordHash(), "hash");
	EXPECT_EQ(account.getCreationDate(), Timestamp(5000ms));
	EXPECT_EQ(account.getAccessLevel(), -3);
	EXPECT_EQ(account.getMembership(), 2);
	EXPECT_EQ(account.getActivated(), 1);
	EXPECT_EQ(account.getLastServer(), -1);
	EXPECT_EQ(account.getLastIp(), "10.0.0.1");
	EXPECT_EQ(account.getLastMac(), "aa-bb-cc-dd-ee-ff");
	EXPECT_EQ(account.getIpForce(), "10.0.0.*");
	EXPECT_EQ(account.getAllowedHddSerial(), "HDD1");
	EXPECT_EQ(account.getAccountTime(), time);

	// getAccountTime returns a copy
	account.getAccountTime()->setAccumulatedRestTime(1);
	EXPECT_EQ(account.getAccountTime()->getAccumulatedRestTime(), 77);

	account.setLastIp(std::nullopt);
	account.setIpForce(std::nullopt);
	account.setAllowedHddSerial(std::nullopt);
	account.setId(std::nullopt);
	EXPECT_FALSE(account.getLastIp());
	EXPECT_FALSE(account.getIpForce());
	EXPECT_FALSE(account.getAllowedHddSerial());
	EXPECT_FALSE(account.getId());
}

TEST(AccountTest, EqualsAndHashCodeUseNameAndPasswordHash) {
	Account a;
	a.setName("admin");
	a.setPasswordHash("W6ph5Mm5Pz8GgiULbPgzG37mj9g=");
	a.setId(1);
	Account b;
	b.setName("admin");
	b.setPasswordHash("W6ph5Mm5Pz8GgiULbPgzG37mj9g=");
	b.setId(2);
	b.setAccessLevel(9);
	EXPECT_TRUE(a == b);
	EXPECT_TRUE(a == a);
	EXPECT_EQ(a.hashCode(), b.hashCode());
	// Java: 31 * "admin".hashCode() + "W6ph5Mm5Pz8GgiULbPgzG37mj9g=".hashCode()
	EXPECT_EQ(static_cast<uint32_t>(a.hashCode()), 0xd3edd562u);
	EXPECT_EQ(a.toString(), "com.aionemu.loginserver.model.Account@d3edd562");

	b.setPasswordHash("other");
	EXPECT_FALSE(a == b);
	b.setPasswordHash(a.getPasswordHash());
	b.setName("Admin");
	EXPECT_FALSE(a == b);

	Account empty;
	EXPECT_EQ(empty.hashCode(), 0);
	EXPECT_EQ(empty.toString(), "com.aionemu.loginserver.model.Account@0");
}

TEST(AccountTest, ModifyAccountTime) {
	Account account;
	EXPECT_THROW(account.modifyAccountTime([](AccountTime&) {}), aion::commons::utils::IllegalStateException);

	account.setAccountTime(AccountTime());
	AccountTime modified = account.modifyAccountTime([](AccountTime& time) { time.setPenaltyEnd(Timestamp(1000ms)); });
	EXPECT_EQ(modified.getPenaltyEnd(), Timestamp(1000ms));
	EXPECT_EQ(account.getAccountTime()->getPenaltyEnd(), Timestamp(1000ms));
}

TEST(AccountTest, ModifyAndStoreAccountTimeStoresTheLatestState) {
	Account account;
	EXPECT_THROW(account.modifyAndStoreAccountTime([](AccountTime&) {}, [](const AccountTime&) { ADD_FAILURE() << "stored"; }),
		aion::commons::utils::IllegalStateException);
	account.setAccountTime(AccountTime());

	// a slow store (e.g. waiting for a database connection) of a logout must not be overtaken by a penalty that is modified and stored meanwhile
	std::vector<AccountTime> stored;
	std::mutex storedMutex;
	std::atomic<bool> firstStoreStarted{false};
	std::thread logout([&] {
		EXPECT_TRUE(account.modifyAndStoreAccountTime([](AccountTime& time) { time.setSessionDuration(42); }, [&](const AccountTime& time) {
			firstStoreStarted = true;
			std::this_thread::sleep_for(200ms);
			std::lock_guard lock(storedMutex);
			stored.push_back(time);
			return true;
		}));
	});
	while (!firstStoreStarted)
		std::this_thread::yield();
	EXPECT_EQ(account.getAccountTime()->getSessionDuration(), 42); // the fields stay readable while a store runs
	int result = account.modifyAndStoreAccountTime([](AccountTime& time) { time.setPenaltyEnd(Timestamp(1000ms)); }, [&](const AccountTime& time) {
		std::lock_guard lock(storedMutex);
		stored.push_back(time);
		return 7;
	});
	logout.join();
	EXPECT_EQ(result, 7);
	ASSERT_EQ(stored.size(), 2u);
	EXPECT_FALSE(stored[0].getPenaltyEnd());
	EXPECT_EQ(stored[1].getPenaltyEnd(), Timestamp(1000ms)); // stored last, with both changes
	EXPECT_EQ(stored[1].getSessionDuration(), 42);
}

TEST(AccountTest, ConcurrentAccessIsSafe) {
	auto account = std::make_shared<Account>();
	account->setAccountTime(AccountTime());
	constexpr int THREADS = 8;
	constexpr int ITERATIONS = 5000;
	std::vector<std::thread> threads;
	std::atomic<bool> start{false};
	for (int t = 0; t < THREADS; t++) {
		threads.emplace_back([&, t] {
			while (!start)
				std::this_thread::yield();
			for (int i = 0; i < ITERATIONS; i++) {
				account->setName(t % 2 == 0 ? std::string(40, 'a') : std::string(3, 'b'));
				account->setLastIp(i % 2 == 0 ? std::optional<std::string>("192.168.100.100") : std::nullopt);
				std::string name = account->getName();
				EXPECT_TRUE(name == std::string(40, 'a') || name == std::string(3, 'b'));
				account->modifyAccountTime([](AccountTime& time) { time.setAccumulatedOnlineTime(time.getAccumulatedOnlineTime() + 1); });
				(void)account->getLastIp();
				(void)account->toString();
			}
		});
	}
	start = true;
	for (auto& thread : threads)
		thread.join();
	EXPECT_EQ(account->getAccountTime()->getAccumulatedOnlineTime(), THREADS * ITERATIONS);
}

TEST(ReconnectingAccountTest, HoldsAccountAndKey) {
	auto account = std::make_shared<Account>();
	account->setId(5);
	ReconnectingAccount reconnecting(account, -123456);
	EXPECT_EQ(reconnecting.getAccount(), account);
	EXPECT_EQ(reconnecting.getReconnectionKey(), -123456);
	EXPECT_EQ(reconnecting.getAccount()->getId(), 5);
}
