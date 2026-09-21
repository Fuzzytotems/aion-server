// Debug live-instance counters by dynamic type (see LiveInstanceCounters.h).

#include "aion/gameserver/runtime/lifetime/LiveInstanceCounters.h"

#include <algorithm>
#include <mutex>
#include <ostream>
#include <shared_mutex>
#include <unordered_map>

#include "aion/commons/utils/ClassName.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"

namespace aion::gameserver::runtime {

namespace {

struct Registry {
	/** guards `byType`; exclusive only while a type registers (once per type) */
	std::shared_mutex mutex;
	/** type_info address -> counter. MSVC emits one type descriptor per type in an executable; a second address of an equal type is added on its first lookup */
	std::unordered_map<const std::type_info*, detail::LiveTypeCounter*> byType;
	/** every registered counter (push only, written under `mutex`, read lock-free) */
	std::atomic<detail::LiveTypeCounter*> head{nullptr};
};

Registry& registry() {
	static auto* instance = new Registry(); // leaked: counting may run during static initialization and destruction
	return *instance;
}

/** the scanning thread's last lookup (destruction runs in bursts of one type) */
thread_local const std::type_info* cachedType = nullptr;
thread_local detail::LiveTypeCounter* cachedCounter = nullptr;

detail::LiveTypeCounter* findCounter(const std::type_info& type) noexcept {
	Registry& r = registry();
	{
		std::shared_lock lock(r.mutex);
		if (auto it = r.byType.find(&type); it != r.byType.end())
			return it->second;
	}
	// another address of an equal type (not expected with one executable): compare by type, then remember the address
	for (detail::LiveTypeCounter* counter = r.head.load(std::memory_order_acquire); counter != nullptr; counter = counter->next) {
		if (*counter->type == type) {
			try {
				std::unique_lock lock(r.mutex);
				r.byType.try_emplace(&type, counter);
			} catch (...) {
				// only a slower next lookup
			}
			return counter;
		}
	}
	return nullptr;
}

std::vector<LiveCount> snapshot() {
	std::vector<LiveCount> counts;
	for (detail::LiveTypeCounter* counter = registry().head.load(std::memory_order_acquire); counter != nullptr; counter = counter->next) {
		counts.push_back(LiveCount{.className = commons::utils::getClassName(*counter->type),
			.live = counter->live.load(std::memory_order_relaxed),
			.created = counter->created.load(std::memory_order_relaxed)});
	}
	std::ranges::sort(counts, {}, &LiveCount::className);
	return counts;
}

} // namespace

namespace detail {

bool registerLiveType(LiveTypeCounter& counter, const std::type_info& type) noexcept {
	Registry& r = registry();
	try {
		std::unique_lock lock(r.mutex);
		if (counter.registered.load(std::memory_order_acquire))
			return true;
		r.byType.try_emplace(&type, &counter);
		counter.type = &type;
		counter.next = r.head.load(std::memory_order_acquire);
		r.head.store(&counter, std::memory_order_release);
		counter.registered.store(true, std::memory_order_release);
		return true;
	} catch (...) {
		return false; // bad_alloc: this instance is not counted (the next creation tries again)
	}
}

void countLiveInstanceDestroyed(const RefCounted& object) noexcept {
	const std::type_info& type = typeid(object);
	LiveTypeCounter* counter = cachedType == &type ? cachedCounter : findCounter(type);
	if (counter == nullptr)
		return; // not counted at creation (registration failed)
	cachedType = &type;
	cachedCounter = counter;
	counter->live.fetch_sub(1, std::memory_order_relaxed);
}

} // namespace detail

std::vector<LiveCount> liveCounts() {
	return snapshot();
}

int64_t liveCountOf(const std::type_info& type) {
	for (detail::LiveTypeCounter* counter = registry().head.load(std::memory_order_acquire); counter != nullptr; counter = counter->next) {
		if (*counter->type == type)
			return counter->live.load(std::memory_order_relaxed);
	}
	return 0;
}

std::vector<LiveCount> liveInstancesOf(const std::vector<std::string>& classNames) {
	return liveInstancesOf(liveCounts(), classNames);
}

std::vector<LiveCount> liveInstancesOf(const std::vector<LiveCount>& counts, const std::vector<std::string>& classNames) {
	std::vector<LiveCount> leaks;
	for (const LiveCount& count : counts) {
		if (count.live == 0)
			continue;
		const bool matches = std::ranges::any_of(classNames, [&](const std::string& name) {
			return count.className == name || (count.className.size() > name.size() + 2 && count.className.ends_with(name) &&
												  count.className.compare(count.className.size() - name.size() - 2, 2, "::") == 0);
		});
		if (matches)
			leaks.push_back(count);
	}
	std::ranges::sort(leaks, {}, &LiveCount::className);
	return leaks;
}

void writeLiveCounts(std::ostream& out) {
	out << "# live instance counts v1\n";
	for (const LiveCount& count : liveCounts())
		out << count.live << '\t' << count.created << '\t' << count.className << '\n';
}

} // namespace aion::gameserver::runtime
