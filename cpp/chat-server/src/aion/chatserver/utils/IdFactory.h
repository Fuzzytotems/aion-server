#pragma once

#include <atomic>
#include <cstdint>

namespace aion::chatserver::utils {

/**
 * Simplified version of idfactory: hands out the channel ids, counting up from 1. Thread safe.
 * <p>
 * Java: com.aionemu.chatserver.utils.IdFactory
 *
 * @author ATracer
 */
class IdFactory {
public:
	static IdFactory& getInstance();

	/** Java: AtomicInteger.incrementAndGet() - the next id; after Integer.MAX_VALUE it wraps around to Integer.MIN_VALUE like Java. */
	int32_t nextId() noexcept { return static_cast<int32_t>(nextId_.fetch_add(1u) + 1u); }

private:
	IdFactory() = default;

	/** the last id handed out, unsigned so the increment wraps without undefined behaviour */
	std::atomic<uint32_t> nextId_ = 0;
};

} // namespace aion::chatserver::utils
