#include <atomic>
#include <regex>
#include <thread>

#include <gtest/gtest.h>

#include "aion/commons/database/ConnectionPool.h"
#include "aion/commons/utils/Exception.h"

using namespace aion::commons::database;
using namespace std::chrono_literals;

namespace {

struct Counters {
	std::atomic<int> created{0};
	std::atomic<int> destroyed{0};
	std::atomic<int> validated{0};
	std::atomic<int> resets{0};
	std::atomic<int> failuresLeft{0};
};

struct FakeConnection {
	int id;
	Counters* counters;
	bool valid = true;
	bool resettable = true;
	bool throwOnReset = false;
	FakeConnection(int id, Counters* counters) : id(id), counters(counters) {}
	~FakeConnection() { ++counters->destroyed; }
};

using Pool = ConnectionPool<FakeConnection>;

std::shared_ptr<Pool> makePool(Counters& counters, Pool::Config config) {
	Pool::Callbacks callbacks;
	callbacks.create = [&counters]() -> std::unique_ptr<FakeConnection> {
		if (counters.failuresLeft > 0) {
			--counters.failuresLeft;
			throw SQLException("Connection refused", "08S01", 2002);
		}
		return std::make_unique<FakeConnection>(++counters.created, &counters);
	};
	callbacks.isValid = [&counters](FakeConnection& c) {
		++counters.validated;
		return c.valid;
	};
	callbacks.reset = [&counters](FakeConnection& c) {
		++counters.resets;
		if (c.throwOnReset)
			throw SQLException("reset failed");
		return c.resettable;
	};
	return Pool::create(std::move(config), std::move(callbacks));
}

Pool::Config config(int32_t maxSize, std::chrono::milliseconds timeout) {
	Pool::Config c;
	c.maximumPoolSize = maxSize;
	c.connectionTimeout = timeout;
	c.creationRetryDelay = 20ms;
	return c;
}

} // namespace

TEST(ConnectionPoolTest, CreatesLazilyAndReusesMostRecentlyReturned) {
	Counters counters;
	auto pool = makePool(counters, config(2, 1000ms));
	EXPECT_EQ(counters.created, 0);
	{
		auto a = pool->getConnection();
		auto b = pool->getConnection();
		EXPECT_EQ(counters.created, 2);
		EXPECT_EQ(pool->getTotalConnections(), 2);
		EXPECT_EQ(pool->getActiveConnections(), 2);
		EXPECT_EQ(pool->getIdleConnections(), 0);
		EXPECT_NE(a->id, b->id);
		b.close();
		EXPECT_EQ(pool->getIdleConnections(), 1);
		auto c = pool->getConnection();
		EXPECT_EQ(c->id, 2);
	}
	EXPECT_EQ(counters.created, 2);
	EXPECT_EQ(counters.resets, 3);
	EXPECT_EQ(pool->getIdleConnections(), 2);
	EXPECT_EQ(pool->getActiveConnections(), 0);
	EXPECT_EQ(counters.validated, 0); // reused within the alive bypass window
}

TEST(ConnectionPoolTest, TimesOutWithHikariMessage) {
	Counters counters;
	auto pool = makePool(counters, config(1, 150ms));
	auto held = pool->getConnection();
	auto start = std::chrono::steady_clock::now();
	try {
		pool->getConnection();
		FAIL() << "expected timeout";
	} catch (const SQLTransientConnectionException& e) {
		EXPECT_GE(std::chrono::steady_clock::now() - start, 140ms);
		std::string message = e.what();
		EXPECT_TRUE(std::regex_match(message,
			std::regex(R"(DatabasePool - Connection is not available, request timed out after \d+ms \(total=1, active=1, idle=0, waiting=0\))")))
			<< message;
		EXPECT_FALSE(e.cause());
	}
	EXPECT_EQ(pool->getThreadsAwaitingConnection(), 0);
}

TEST(ConnectionPoolTest, WaiterReceivesReturnedConnection) {
	Counters counters;
	auto pool = makePool(counters, config(1, 5000ms));
	auto held = pool->getConnection();
	std::thread releaser([&] {
		while (pool->getThreadsAwaitingConnection() == 0)
			std::this_thread::sleep_for(1ms);
		held.close();
	});
	auto start = std::chrono::steady_clock::now();
	auto con = pool->getConnection();
	releaser.join();
	EXPECT_LT(std::chrono::steady_clock::now() - start, 4000ms);
	EXPECT_EQ(con->id, 1);
	EXPECT_EQ(counters.created, 1);
}

TEST(ConnectionPoolTest, InvalidIdleConnectionIsReplaced) {
	Counters counters;
	Pool::Config c = config(1, 1000ms);
	c.aliveBypassWindow = 0ms;
	auto pool = makePool(counters, c);
	{
		auto con = pool->getConnection();
		con->valid = false;
	}
	std::this_thread::sleep_for(5ms);
	auto con = pool->getConnection();
	EXPECT_EQ(con->id, 2);
	EXPECT_EQ(counters.validated, 1);
	EXPECT_EQ(counters.destroyed, 1);
	EXPECT_EQ(pool->getTotalConnections(), 1);
}

TEST(ConnectionPoolTest, FailedResetDiscardsConnection) {
	Counters counters;
	auto pool = makePool(counters, config(2, 1000ms));
	{
		auto a = pool->getConnection();
		auto b = pool->getConnection();
		a->resettable = false;
		b->throwOnReset = true;
	}
	EXPECT_EQ(counters.destroyed, 2);
	EXPECT_EQ(pool->getTotalConnections(), 0);
	EXPECT_EQ(pool->getIdleConnections(), 0);
	auto con = pool->getConnection();
	EXPECT_EQ(con->id, 3);
}

TEST(ConnectionPoolTest, CreationFailuresAreRetriedUntilTimeout) {
	Counters counters;
	counters.failuresLeft = 1000;
	auto pool = makePool(counters, config(2, 300ms));
	try {
		pool->getConnection();
		FAIL() << "expected timeout";
	} catch (const SQLTransientConnectionException& e) {
		EXPECT_EQ(e.getSQLState(), "08S01");
		EXPECT_EQ(e.getErrorCode(), 2002);
		ASSERT_TRUE(e.cause());
		EXPECT_NE(std::string(e.what()).find("(total=0, active=0, idle=0, waiting=0)"), std::string::npos) << e.what();
		std::string trace = aion::commons::utils::toStackTraceString(e);
		EXPECT_NE(trace.find("Caused by: aion::commons::database::SQLException: Connection refused"), std::string::npos);
	}
	EXPECT_GT(1000 - counters.failuresLeft, 3);
	EXPECT_EQ(pool->getTotalConnections(), 0);

	counters.failuresLeft = 2;
	auto con = pool->getConnection(); // recovers within the timeout
	EXPECT_EQ(con->id, 1);
}

TEST(ConnectionPoolTest, MaxLifetimeRetiresConnections) {
	Counters counters;
	Pool::Config c = config(1, 1000ms);
	c.maxLifetime = 30ms;
	auto pool = makePool(counters, c);
	pool->getConnection().close();
	EXPECT_EQ(pool->getIdleConnections(), 1);
	std::this_thread::sleep_for(50ms);
	auto con = pool->getConnection();
	EXPECT_EQ(con->id, 2);
	EXPECT_EQ(counters.destroyed, 1);
}

TEST(ConnectionPoolTest, ShutdownClosesIdleAndRejectsRequests) {
	Counters counters;
	auto pool = makePool(counters, config(2, 5000ms));
	auto borrowed = pool->getConnection();
	pool->getConnection().close();
	EXPECT_EQ(pool->getIdleConnections(), 1);

	std::atomic<bool> waiterFailed{false};
	auto fullPool = makePool(counters, config(1, 5000ms));
	auto blocker = fullPool->getConnection();
	std::thread waiter([&] {
		try {
			fullPool->getConnection();
		} catch (const SQLException& e) {
			waiterFailed = std::string(e.what()) == "DatabasePool has been closed.";
		}
	});
	while (fullPool->getThreadsAwaitingConnection() == 0)
		std::this_thread::sleep_for(1ms);
	fullPool->shutdown();
	waiter.join();
	EXPECT_TRUE(waiterFailed);

	pool->shutdown();
	EXPECT_TRUE(pool->isClosed());
	EXPECT_EQ(counters.destroyed, 1);
	EXPECT_THROW(pool->getConnection(), SQLException);
	int resetsBefore = counters.resets;
	borrowed.close();
	EXPECT_EQ(counters.destroyed, 2);
	EXPECT_EQ(counters.resets, resetsBefore); // not reset, just closed
	EXPECT_EQ(pool->getTotalConnections(), 0);
	pool->shutdown(); // idempotent
}

TEST(ConnectionPoolTest, HandleSemantics) {
	Counters counters;
	auto pool = makePool(counters, config(2, 1000ms));
	Pool::Handle empty;
	EXPECT_FALSE(empty);
	EXPECT_THROW((void)empty->id, SQLException);

	auto a = pool->getConnection();
	Pool::Handle moved = std::move(a);
	EXPECT_FALSE(a);
	EXPECT_TRUE(moved);
	EXPECT_EQ(pool->getActiveConnections(), 1);
	auto b = pool->getConnection();
	moved = std::move(b); // returns the first connection
	EXPECT_EQ(pool->getActiveConnections(), 1);
	EXPECT_EQ(pool->getIdleConnections(), 1);
	moved.close();
	moved.close();
	EXPECT_EQ(pool->getActiveConnections(), 0);
	try {
		(void)moved->id;
		FAIL();
	} catch (const SQLException& e) {
		EXPECT_EQ(std::string(e.what()), "Connection is closed");
	}
}

TEST(ConnectionPoolTest, AddIdleConnection) {
	Counters counters;
	auto pool = makePool(counters, config(1, 1000ms));
	pool->addIdleConnection(std::make_unique<FakeConnection>(100, &counters));
	EXPECT_EQ(pool->getTotalConnections(), 1);
	pool->addIdleConnection(std::make_unique<FakeConnection>(101, &counters)); // pool is full
	EXPECT_EQ(counters.destroyed, 1);
	EXPECT_EQ(pool->getConnection()->id, 100);
}

TEST(ConnectionPoolTest, RejectsInvalidSize) {
	Counters counters;
	EXPECT_THROW(makePool(counters, config(0, 1000ms)), aion::commons::utils::IllegalArgumentException);
}

TEST(ConnectionPoolTest, ConcurrentBorrowersNeverExceedMaximum) {
	Counters counters;
	auto pool = makePool(counters, config(3, 10000ms));
	std::atomic<int> inUse{0}, maxInUse{0}, borrows{0};
	std::vector<std::thread> threads;
	for (int t = 0; t < 8; ++t) {
		threads.emplace_back([&] {
			for (int i = 0; i < 200; ++i) {
				auto con = pool->getConnection();
				int now = ++inUse;
				int seen = maxInUse;
				while (now > seen && !maxInUse.compare_exchange_weak(seen, now)) {
				}
				if (i % 10 == 0)
					std::this_thread::yield();
				--inUse;
				++borrows;
			}
		});
	}
	for (auto& thread : threads)
		thread.join();
	EXPECT_EQ(borrows, 1600);
	EXPECT_LE(maxInUse, 3);
	EXPECT_LE(counters.created, 3);
	EXPECT_EQ(pool->getActiveConnections(), 0);
	EXPECT_EQ(pool->getThreadsAwaitingConnection(), 0);
}
