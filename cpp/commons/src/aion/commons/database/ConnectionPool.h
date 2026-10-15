#pragma once

#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <exception>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

#include <fmt/format.h>

#include "aion/commons/database/SQLException.h"
#include "aion/commons/logging/LoggerFactory.h"

namespace aion::commons::database {

/**
 * Java: com.zaxxer.hikari.pool.HikariPool (the parts used by the servers), as a small generic pool so that its logic can be tested without a
 * database. DatabaseFactory instantiates it with Connection.
 * <p>
 * Behaviour:
 * <ul>
 * <li>At most maximumPoolSize connections exist. They are created lazily in the thread calling getConnection().</li>
 * <li>getConnection() prefers the most recently returned idle connection. If none is idle and the pool is full, it waits up to
 * connectionTimeout and then throws SQLTransientConnectionException with HikariCP's message, e.g. "DatabasePool - Connection is not available,
 * request timed out after 5000ms (total=5, active=5, idle=0, waiting=0)", whose cause is the last connection creation failure (if any).
 * Failed connection attempts are retried every creationRetryDelay until the timeout.</li>
 * <li>An idle connection that was not used for longer than aliveBypassWindow (HikariCP: 500 ms) is validated before it is handed out;
 * invalid connections are closed and replaced. Connections older than maxLifetime are retired when borrowed or returned.</li>
 * <li>When a handle is destroyed (or closed), the reset callback prepares the connection for reuse; if it returns false or throws, the
 * connection is closed instead.</li>
 * </ul>
 * All methods are thread safe. Handles keep the pool alive, so they may outlive shutdown(); connections returned after shutdown are closed.
 */
template <typename T>
class ConnectionPool : public std::enable_shared_from_this<ConnectionPool<T>> {
	struct Entry {
		std::unique_ptr<T> resource;
		std::chrono::steady_clock::time_point created;
		std::chrono::steady_clock::time_point lastAccess;
	};

public:
	struct Config {
		std::string poolName = "DatabasePool";
		int32_t maximumPoolSize = 5;
		std::chrono::milliseconds connectionTimeout{30000};
		std::chrono::milliseconds aliveBypassWindow{500};
		/** 0: unlimited */
		std::chrono::milliseconds maxLifetime{std::chrono::minutes(30)};
		std::chrono::milliseconds creationRetryDelay{250};
	};

	struct Callbacks {
		/** opens a new connection; may throw */
		std::function<std::unique_ptr<T>()> create;
		/** checks an idle connection before handing it out; false or an exception discards it */
		std::function<bool(T&)> isValid;
		/** prepares a returned connection for reuse; false or an exception discards it */
		std::function<bool(T&)> reset;
	};

	/** RAII handle of a borrowed connection. Returns the connection to the pool when destroyed or closed. Movable, not copyable. */
	class Handle {
	public:
		Handle() = default;
		Handle(Handle&& other) noexcept = default;
		Handle& operator=(Handle&& other) noexcept {
			if (this != &other) {
				close();
				pool = std::move(other.pool);
				entry = std::move(other.entry);
			}
			return *this;
		}
		~Handle() { close(); }

		/** @throws SQLException "Connection is closed" if the handle was closed or moved from */
		T* operator->() const { return &get(); }
		T& operator*() const { return get(); }
		T& get() const {
			if (!entry.resource)
				throw SQLException("Connection is closed", "08003");
			return *entry.resource;
		}
		explicit operator bool() const noexcept { return entry.resource != nullptr; }

		/** Java: Connection.close() - returns the connection to the pool early. Idempotent. */
		void close() noexcept {
			if (pool) {
				std::shared_ptr<ConnectionPool> p = std::move(pool);
				pool.reset();
				p->release(std::move(entry));
				entry = Entry{};
			}
		}

	private:
		friend class ConnectionPool;
		Handle(std::shared_ptr<ConnectionPool> pool, Entry entry) noexcept : pool(std::move(pool)), entry(std::move(entry)) {}

		std::shared_ptr<ConnectionPool> pool;
		Entry entry;
	};

	static std::shared_ptr<ConnectionPool> create(Config config, Callbacks callbacks) {
		if (config.maximumPoolSize < 1)
			throw utils::IllegalArgumentException("maxPoolSize cannot be less than 1");
		return std::shared_ptr<ConnectionPool>(new ConnectionPool(std::move(config), std::move(callbacks)));
	}

	~ConnectionPool() = default;
	ConnectionPool(const ConnectionPool&) = delete;
	ConnectionPool& operator=(const ConnectionPool&) = delete;

	/**
	 * Borrows a connection.
	 * @throws SQLTransientConnectionException if none became available within connectionTimeout
	 * @throws SQLException if the pool has been shut down
	 */
	Handle getConnection() {
		using Clock = std::chrono::steady_clock;
		const Clock::time_point start = Clock::now();
		const Clock::time_point deadline = start + config.connectionTimeout;
		std::unique_lock lock(mutex);
		while (true) {
			if (closed)
				throw SQLException(fmt::format("{} has been closed.", config.poolName), "08003");
			if (!idle.empty()) {
				Entry entry = std::move(idle.back());
				idle.pop_back();
				++active;
				lock.unlock();
				if (checkBorrowed(entry))
					return Handle(this->shared_from_this(), std::move(entry));
				destroy(std::move(entry));
				lock.lock();
				--active;
				--total;
				continue;
			}
			bool creationFailed = false;
			if (total < config.maximumPoolSize) {
				++total;
				++active;
				lock.unlock();
				std::unique_ptr<T> resource;
				std::exception_ptr failure;
				try {
					resource = callbacks.create();
					if (!resource)
						throw SQLException("Connection factory returned no connection");
				} catch (...) {
					failure = std::current_exception();
				}
				if (resource) {
					const Clock::time_point now = Clock::now();
					return Handle(this->shared_from_this(), Entry{std::move(resource), now, now});
				}
				logFailure("Cannot acquire connection from data source", failure);
				lock.lock();
				--total;
				--active;
				lastFailure = failure;
				creationFailed = true;
				available.notify_one(); // a slot became free for another waiter
				if (closed)
					continue;
			}
			const Clock::time_point now = Clock::now();
			if (now >= deadline)
				throw timeoutException(std::chrono::duration_cast<std::chrono::milliseconds>(now - start));
			++waiting;
			Clock::time_point wakeUp = creationFailed ? std::min(deadline, now + config.creationRetryDelay) : deadline;
			available.wait_until(lock, wakeUp);
			--waiting;
		}
	}

	/**
	 * Adds an already opened connection as idle connection (used for the fail-fast connection opened at startup). The connection is closed if
	 * the pool is full or shut down.
	 */
	void addIdleConnection(std::unique_ptr<T> resource) noexcept {
		if (!resource)
			return;
		const auto now = std::chrono::steady_clock::now();
		Entry entry{std::move(resource), now, now};
		{
			std::scoped_lock lock(mutex);
			if (!closed && total < config.maximumPoolSize) {
				++total;
				idle.push_back(std::move(entry));
			}
		}
		available.notify_one();
		destroy(std::move(entry));
	}

	/** Java: HikariDataSource.close() - closes idle connections and rejects further requests. Borrowed connections are closed when returned. */
	void shutdown() noexcept {
		std::vector<Entry> toClose;
		{
			std::scoped_lock lock(mutex);
			if (closed)
				return;
			closed = true;
			toClose = std::move(idle);
			idle.clear();
			total -= static_cast<int32_t>(toClose.size());
		}
		available.notify_all();
		for (Entry& entry : toClose)
			destroy(std::move(entry));
	}

	bool isClosed() const {
		std::scoped_lock lock(mutex);
		return closed;
	}

	int32_t getTotalConnections() const {
		std::scoped_lock lock(mutex);
		return total;
	}

	int32_t getActiveConnections() const {
		std::scoped_lock lock(mutex);
		return active;
	}

	int32_t getIdleConnections() const {
		std::scoped_lock lock(mutex);
		return static_cast<int32_t>(idle.size());
	}

	int32_t getThreadsAwaitingConnection() const {
		std::scoped_lock lock(mutex);
		return waiting;
	}

	const Config& getConfig() const noexcept { return config; }

private:
	ConnectionPool(Config config, Callbacks callbacks) : config(std::move(config)), callbacks(std::move(callbacks)) {}

	static const logging::Logger& log() {
		static const logging::Logger logger = logging::LoggerFactory::getLogger("com.zaxxer.hikari.pool.HikariPool");
		return logger;
	}

	void logFailure(std::string_view message, const std::exception_ptr& failure) const noexcept {
		try {
			if (!log().isDebugEnabled())
				return;
			try {
				std::rethrow_exception(failure);
			} catch (const std::exception& e) {
				log().debug(fmt::format("{} - {}", config.poolName, message), e);
			} catch (...) {
				log().debug("{} - {}", config.poolName, message);
			}
		} catch (...) {
		}
	}

	bool isExpired(const Entry& entry, std::chrono::steady_clock::time_point now) const noexcept {
		return config.maxLifetime.count() > 0 && now - entry.created > config.maxLifetime;
	}

	/** validation of a borrowed idle connection, outside the lock */
	bool checkBorrowed(Entry& entry) noexcept {
		const auto now = std::chrono::steady_clock::now();
		if (isExpired(entry, now))
			return false;
		if (now - entry.lastAccess <= config.aliveBypassWindow || !callbacks.isValid)
			return true;
		bool valid = false;
		try {
			valid = callbacks.isValid(*entry.resource);
		} catch (...) {
			valid = false;
		}
		if (!valid) {
			try {
				log().warn("{} - Failed to validate connection. Possibly consider using a shorter maxLifetime value.", config.poolName);
			} catch (...) {
			}
		}
		return valid;
	}

	void release(Entry entry) noexcept {
		if (!entry.resource)
			return;
		bool keep = false;
		if (!isClosed()) {
			try {
				keep = !callbacks.reset || callbacks.reset(*entry.resource);
			} catch (...) {
				logFailure("Exception while resetting a returned connection, closing it", std::current_exception());
				keep = false;
			}
		}
		const auto now = std::chrono::steady_clock::now();
		if (isExpired(entry, now))
			keep = false;
		{
			std::scoped_lock lock(mutex);
			--active;
			if (keep && !closed) {
				entry.lastAccess = now;
				idle.push_back(std::move(entry));
			} else {
				--total;
			}
		}
		available.notify_one();
		destroy(std::move(entry)); // no-op if the entry was moved to the idle list
	}

	static void destroy(Entry entry) noexcept {
		try {
			entry.resource.reset();
		} catch (...) {
		}
	}

	SQLTransientConnectionException timeoutException(std::chrono::milliseconds elapsed) const {
		std::string sqlState;
		int32_t errorCode = 0;
		if (lastFailure) {
			try {
				std::rethrow_exception(lastFailure);
			} catch (const SQLException& e) {
				sqlState = e.getSQLState();
				errorCode = e.getErrorCode();
			} catch (...) {
			}
		}
		return SQLTransientConnectionException(
			fmt::format("{} - Connection is not available, request timed out after {}ms (total={}, active={}, idle={}, waiting={})", config.poolName,
				elapsed.count(), total, active, idle.size(), waiting),
			std::move(sqlState), errorCode, lastFailure);
	}

	const Config config;
	const Callbacks callbacks;
	mutable std::mutex mutex;
	std::condition_variable available;
	std::vector<Entry> idle;
	int32_t total = 0;
	int32_t active = 0;
	int32_t waiting = 0;
	bool closed = false;
	std::exception_ptr lastFailure;
};

} // namespace aion::commons::database
