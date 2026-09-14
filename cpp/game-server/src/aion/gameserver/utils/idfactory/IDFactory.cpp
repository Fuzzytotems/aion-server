// IDFactory with a monotone allocation cursor, wrap and release quarantine (design §6, RR-10/RR-16). See IDFactory.h.

#include "aion/gameserver/utils/idfactory/IDFactory.h"

#include <algorithm>
#include <bit>
#include <deque>
#include <format>
#include <mutex>
#include <unordered_map>
#include <vector>

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/gameserver/runtime/base/Checked.h"
#include "aion/gameserver/runtime/sync/RankedMutex.h"
#include "aion/gameserver/utils/ThreadPoolManager.h"

namespace aion::gameserver::utils::idfactory {

namespace {

using runtime::LockRank;
using TimePoint = std::chrono::steady_clock::time_point;

const commons::logging::Logger& log() {
	static const auto* instance = new commons::logging::Logger(commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.utils.idfactory.IDFactory"));
	return *instance;
}

/** exclusive upper bound of all ids (Integer.MAX_VALUE is the largest id) */
constexpr int64_t ID_LIMIT = int64_t{1} << 31;

/** Growable bit set (Java BitSet): bits beyond the allocated words are clear. */
class BitSet {
public:
	bool test(int64_t index) const noexcept {
		auto word = static_cast<size_t>(index >> 6);
		return word < words.size() && ((words[word] >> (index & 63)) & 1u) != 0;
	}

	void set(int64_t index) {
		auto word = static_cast<size_t>(index >> 6);
		if (word >= words.size())
			words.resize(std::min<size_t>(std::max(word + 1, words.size() * 2), static_cast<size_t>(ID_LIMIT >> 6)));
		words[word] |= uint64_t{1} << (index & 63);
	}

	void clear(int64_t index) noexcept {
		auto word = static_cast<size_t>(index >> 6);
		if (word < words.size())
			words[word] &= ~(uint64_t{1} << (index & 63));
	}

	/** Java nextClearBit: the first clear bit >= from */
	int64_t nextClear(int64_t from) const noexcept {
		auto word = static_cast<size_t>(from >> 6);
		if (word >= words.size())
			return from;
		uint64_t bits = ~words[word] & (~uint64_t{0} << (from & 63));
		while (bits == 0) {
			if (++word >= words.size())
				return static_cast<int64_t>(word) * 64;
			bits = ~words[word];
		}
		return static_cast<int64_t>(word) * 64 + std::countr_zero(bits);
	}

	void reset() noexcept {
		words.clear();
		words.shrink_to_fit();
	}

private:
	std::vector<uint64_t> words;
};

struct Quarantined {
	int32_t id;
	TimePoint freeAt;
};

struct IDFactoryState {
	runtime::RankedMutex<LockRank::IDFACTORY> mutex;
	/** taken ids, including quarantined ones */
	BitSet used;
	/** ids in the release FIFO */
	BitSet quarantined;
	int32_t usedCount = 0;
	/** Java nextMinId, but monotone (int64: may reach 2^31) */
	int64_t cursor = 1;
	IDFactory::Config config;
	std::deque<Quarantined> quarantine;
#if AION_CHECKED
	struct Released {
		int32_t id;
		TimePoint releasedAt;
	};
	std::deque<Released> recentOrder;
	std::unordered_map<int32_t, std::pair<const char*, TimePoint>> recent;
#endif
};

IDFactoryState& state() {
	static auto* instance = new IDFactoryState();
	return *instance;
}

/** read before taking the IDFACTORY leaf mutex (ThreadPoolManager may take its own locks) */
TimePoint now() {
	return ThreadPoolManager::clock().now(); // never creates the default pools
}

int32_t drainExpired(IDFactoryState& s, TimePoint time) noexcept {
	int32_t freed = 0;
	while (!s.quarantine.empty() && s.quarantine.front().freeAt <= time) {
		int32_t id = s.quarantine.front().id;
		s.quarantine.pop_front();
		s.quarantined.clear(id);
		s.used.clear(id);
		--s.usedCount;
		++freed;
	}
	return freed;
}

#if AION_CHECKED
void pruneRecent(IDFactoryState& s, TimePoint time) {
	while (!s.recentOrder.empty() && s.recentOrder.front().releasedAt + s.config.reuseWarningWindow < time) {
		auto it = s.recent.find(s.recentOrder.front().id);
		if (it != s.recent.end() && it->second.second == s.recentOrder.front().releasedAt)
			s.recent.erase(it);
		s.recentOrder.pop_front();
	}
}
#endif

/** lowest free valid id in [from, to), or -1 */
int64_t findFree(const IDFactoryState& s, int64_t from, int64_t to) noexcept {
	int64_t id = std::max<int64_t>(from, 0);
	while (id < to) {
		id = s.used.nextClear(id);
		if (id >= to)
			return -1;
		if (!IDFactory::isInvalidId(static_cast<int32_t>(id)))
			return id;
		++id;
	}
	return -1;
}

} // namespace

IDFactory& IDFactory::getInstance() {
	static IDFactory* instance = [] {
		auto* factory = new IDFactory();
		factory->lockIds({0});
		return factory;
	}();
	return *instance;
}

void IDFactory::configure(const Config& config) {
	if (config.wrapAt < 2)
		throw runtime::IllegalArgumentException(std::format("gameserver.idfactory.wrap_at must be at least 2: {}", config.wrapAt));
	if (config.releaseDelay.count() < 0 || config.reuseWarningWindow.count() < 0)
		throw runtime::IllegalArgumentException("IDFactory delays must not be negative");
	IDFactoryState& s = state();
	std::scoped_lock lock(s.mutex);
	s.config = config;
}

IDFactory::Config IDFactory::getConfig() const {
	IDFactoryState& s = state();
	std::scoped_lock lock(s.mutex);
	return s.config;
}

void IDFactory::lockIds(std::span<const int32_t> ids) {
	IDFactoryState& s = state();
	std::scoped_lock lock(s.mutex);
	for (int32_t id : ids) {
		if (id < 0 || s.used.test(id))
			throw IDFactoryError("ID " + std::to_string(id) + " is already taken, fatal error!!!");
		s.used.set(id);
		++s.usedCount;
	}
}

void IDFactory::logUsedCount() const {
	log().info("IDFactory: {} IDs used.", getUsedCount());
}

int32_t IDFactory::nextId() {
	IDFactoryState& s = state();
	TimePoint time = now();
	int64_t id = -1;
	bool wrapped = false;
	int64_t wrapAt = 0;
	{
		std::scoped_lock lock(s.mutex);
		drainExpired(s, time);
		wrapAt = s.config.wrapAt;
		if (s.cursor < wrapAt)
			id = findFree(s, s.cursor, wrapAt);
		if (id < 0) {
			id = findFree(s, 1, wrapAt);
			wrapped = id >= 0;
		}
		if (id < 0)
			id = findFree(s, wrapAt, ID_LIMIT);
		if (id < 0)
			throw IDFactoryError("All IDs are used, please clear your database");
		s.used.set(id);
		++s.usedCount;
		s.cursor = id + 1;
	}
	if (wrapped)
		log().info("IDFactory: the allocation cursor passed {} and wrapped to ID {}", wrapAt, id);
	return static_cast<int32_t>(id);
}

void IDFactory::releaseId(int32_t id, const char* className) {
	IDFactoryState& s = state();
	TimePoint time = now();
	bool notTaken = false;
	{
		std::scoped_lock lock(s.mutex);
		drainExpired(s, time);
		if (id < 0 || !s.used.test(id) || s.quarantined.test(id)) {
			notTaken = true;
		} else {
#if AION_CHECKED
			pruneRecent(s, time);
			s.recent[id] = {className != nullptr ? className : "unknown", time};
			s.recentOrder.push_back({id, time});
#else
			(void)className;
#endif
			if (s.config.releaseDelay.count() == 0) {
				s.used.clear(id);
				--s.usedCount;
			} else {
				s.quarantined.set(id);
				s.quarantine.push_back({id, time + s.config.releaseDelay});
			}
		}
	}
	if (notTaken)
		log().warn("Couldn't release ID {} because it wasn't taken", id, runtime::IllegalArgumentException("Couldn't release ID " + std::to_string(id)));
}

void IDFactory::releaseObjectIds(std::span<const int32_t> ids, const char* className) {
	for (int32_t id : ids)
		releaseId(id, className);
}

int32_t IDFactory::getUsedCount() const {
	IDFactoryState& s = state();
	std::scoped_lock lock(s.mutex);
	return s.usedCount;
}

int32_t IDFactory::getQuarantinedCount() const {
	IDFactoryState& s = state();
	std::scoped_lock lock(s.mutex);
	return static_cast<int32_t>(s.quarantine.size());
}

int32_t IDFactory::drainQuarantine() {
	IDFactoryState& s = state();
	TimePoint time = now();
	std::scoped_lock lock(s.mutex);
	return drainExpired(s, time);
}

const char* IDFactory::recentlyReleased(int32_t id) const {
#if AION_CHECKED
	IDFactoryState& s = state();
	TimePoint time = now();
	std::scoped_lock lock(s.mutex);
	pruneRecent(s, time);
	auto it = s.recent.find(id);
	return it != s.recent.end() ? it->second.first : nullptr;
#else
	(void)id;
	return nullptr;
#endif
}

int64_t IDFactory::getCursor() const {
	IDFactoryState& s = state();
	std::scoped_lock lock(s.mutex);
	return s.cursor;
}

void IDFactory::resetForTests() {
	IDFactoryState& s = state();
	std::scoped_lock lock(s.mutex);
	s.used.reset();
	s.quarantined.reset();
	s.quarantine.clear();
#if AION_CHECKED
	s.recent.clear();
	s.recentOrder.clear();
#endif
	s.config = Config{};
	s.cursor = 1;
	s.used.set(0);
	s.usedCount = 1;
}

} // namespace aion::gameserver::utils::idfactory
