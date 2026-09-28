#pragma once

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "aion/gameserver/runtime/sched/Future.h"

namespace aion::gameserver::runtime::sched_detail {

/**
 * The ScheduledPool heap (design §7.2): scheduled tasks ordered by (due, sequence). Not thread-safe: the owning backend guards it with its
 * SCHEDULER leaf mutex.
 *
 * - push() assigns the next sequence number (FIFO among equal due times; a re-armed periodic task goes behind tasks already queued for the same
 *   due time) and copies due/sequence into the entry, so ordering never depends on concurrent writes to the Future.
 * - Cancelled shells stay in the heap until they reach the top or until a purge: when the heap has grown past the purge threshold, purge()
 *   counts cancelled entries and rebuilds the heap without them if they exceed 50%. The next check happens when the heap has doubled again,
 *   so the cost is amortized O(1) per push.
 * - Removed and popped Refs are handed back to the caller, which must release them after unlocking (a release never runs user code, but the
 *   rule "nothing beyond the heap under the leaf mutex" keeps the critical sections trivially short).
 */
class TimerHeap {
public:
	static constexpr size_t MIN_PURGE_CHECK = 1024;

	void push(FutureRef task) {
		uint64_t sequence = nextSequence_++;
		task->setSequence(sequence);
		int64_t due = std::chrono::duration_cast<std::chrono::nanoseconds>(task->getDueTime().time_since_epoch()).count();
		entries_.push_back(Entry{due, sequence, std::move(task)});
		std::push_heap(entries_.begin(), entries_.end(), Later{});
	}

	bool empty() const noexcept { return entries_.empty(); }
	size_t size() const noexcept { return entries_.size(); }

	/** due time of the top entry (heap must not be empty) */
	std::chrono::steady_clock::time_point topDue() const noexcept {
		return std::chrono::steady_clock::time_point(std::chrono::nanoseconds(entries_.front().due));
	}
	const FutureRef& top() const noexcept { return entries_.front().task; }

	FutureRef pop() {
		std::pop_heap(entries_.begin(), entries_.end(), Later{});
		FutureRef task = std::move(entries_.back().task);
		entries_.pop_back();
		return task;
	}

	/** Pops cancelled shells from the top (so topDue() is meaningful) into `released`. */
	void dropCancelledTop(std::vector<FutureRef>& released) {
		while (!entries_.empty() && entries_.front().task->isCancelled())
			released.push_back(pop());
	}

	/** Rebuilds the heap without cancelled shells if the check threshold is reached and they exceed 50%. */
	void maybePurge(std::vector<FutureRef>& released) {
		if (entries_.size() < purgeCheckAt_)
			return;
		size_t cancelled = static_cast<size_t>(std::count_if(entries_.begin(), entries_.end(), [](const Entry& e) { return e.task->isCancelled(); }));
		if (cancelled * 2 > entries_.size()) {
			auto firstRemoved = std::stable_partition(entries_.begin(), entries_.end(), [](const Entry& e) { return !e.task->isCancelled(); });
			for (auto it = firstRemoved; it != entries_.end(); ++it)
				released.push_back(std::move(it->task));
			entries_.erase(firstRemoved, entries_.end());
			std::make_heap(entries_.begin(), entries_.end(), Later{});
			++purges_;
		}
		purgeCheckAt_ = std::max(MIN_PURGE_CHECK, entries_.size() * 2);
	}

	/** Removes every entry (shutdown). */
	void clear(std::vector<FutureRef>& released) {
		for (Entry& entry : entries_)
			released.push_back(std::move(entry.task));
		std::vector<Entry>().swap(entries_); // also frees the capacity (a retired backend keeps its empty heap forever)
		purgeCheckAt_ = MIN_PURGE_CHECK;
	}

	template <class F>
	void forEach(F&& visitor) const {
		for (const Entry& entry : entries_)
			visitor(entry.task);
	}

	uint64_t purgeCount() const noexcept { return purges_; }

private:
	struct Entry {
		int64_t due;
		uint64_t sequence;
		FutureRef task;
	};
	/** max-heap comparator yielding a min-heap on (due, sequence) */
	struct Later {
		bool operator()(const Entry& a, const Entry& b) const noexcept { return a.due != b.due ? a.due > b.due : a.sequence > b.sequence; }
	};

	std::vector<Entry> entries_;
	uint64_t nextSequence_ = 1;
	size_t purgeCheckAt_ = MIN_PURGE_CHECK;
	uint64_t purges_ = 0;
};

} // namespace aion::gameserver::runtime::sched_detail
