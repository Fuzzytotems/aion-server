#pragma once

#include <algorithm>
#include <atomic>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <iterator>
#include <memory>
#include <ranges>
#include <typeinfo>
#include <utility>
#include <vector>

#include "aion/gameserver/runtime/base/YieldPoint.h"
#include "aion/gameserver/runtime/collections/Iterators.h"
#include "aion/gameserver/runtime/collections/JavaEquals.h"
#include "aion/gameserver/runtime/collections/detail/ShimSupport.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/Reclaimer.h"
#include "aion/gameserver/runtime/lifetime/TaskScope.h"
#include "aion/gameserver/runtime/sync/LockClass.h"
#include "aion/gameserver/runtime/sync/Monitor.h"

namespace aion::gameserver::runtime {

namespace detail {

/** Immutable element array of a CopyOnWriteArrayList, epoch-reclaimed after replacement. */
template <class T>
struct CowArray final : RetiredNode {
	explicit CowArray(std::vector<T> items) : items(std::move(items)) {}
	size_t retiredBytes() const noexcept override { return sizeof(CowArray) + items.capacity() * sizeof(T); }

	const std::vector<T> items;
};

} // namespace detail

/**
 * Java: java.util.concurrent.CopyOnWriteArrayList (design §3.3, RR-10: hot observer lists).
 *
 * An atomic pointer to an immutable array (nullptr while empty, so empty lists allocate nothing). Readers (get, contains, indexOf, iteration,
 * snapshot, forEach) are lock-free: read barrier, load the array, read it; iteration walks the loaded array in place (a stable snapshot for the
 * whole loop, no copy). size() and isEmpty() read an atomic counter and need no TaskScope. Writers copy the array under the list's writer Monitor
 * (lock class `Owner::field`), publish the new array atomically and retire the old one with Reclaimer::retireNode.
 * removeIf predicates run under the writer Monitor on the current array and the result replaces the array, like Java (a re-entrant modification
 * made by the predicate itself is overwritten, as in Java). iterator() is a snapshot iterator whose remove() throws
 * UnsupportedOperationException, as in Java. Java equals for contains/indexOf/remove(Object)/addIfAbsent.
 * All operations except size/isEmpty need an active TaskScope (read barrier, C2). Checked builds: dereferencing an in-place iterator after
 * quiescentPoint() or in another scope than its creation throws IllegalStateException (C1), as for a stale Ptr.
 * Thread-safety: every member is thread-safe. Not copyable or movable. Destroying the list frees the current array immediately (the owner must
 * be unreachable).
 */
template <class T>
class CopyOnWriteArrayList {
	using Array = detail::CowArray<T>;

public:
	using value_type = T;
	using Element = Borrowed<T>;

	/** In-place iterator over one loaded immutable array. */
	class Iterator {
	public:
		using value_type = Element;
		using difference_type = std::ptrdiff_t;

		Iterator() = default;
		Element operator*() const {
#if AION_CHECKED
			// C1 for the iterator itself: the loaded array is protected only by the publication of the creating scope (quiescentPoint() ends it)
			if (uint64_t currentScope = TaskScope::currentScopeId(); currentScope != scopeStamp) [[unlikely]]
				detail::throwStaleBorrow(typeid(CopyOnWriteArrayList), scopeStamp, currentScope);
#endif
			return Element(array->items[index]);
		}
		Iterator& operator++() {
			++index;
			return *this;
		}
		Iterator operator++(int) {
			Iterator old = *this;
			++index;
			return old;
		}
		/** compares with the size cached at creation: never reads the (possibly retired) array */
		friend bool operator==(const Iterator& it, std::default_sentinel_t) noexcept { return it.index >= it.count; }

	private:
		friend class CopyOnWriteArrayList;
		explicit Iterator(const Array* array) noexcept : array(array), count(array != nullptr ? array->items.size() : 0) {
#if AION_CHECKED
			scopeStamp = TaskScope::currentScopeId();
#endif
		}

		const Array* array = nullptr;
		size_t count = 0;
		size_t index = 0;
#if AION_CHECKED
		uint64_t scopeStamp = 0;
#endif
	};

	CopyOnWriteArrayList() : monitor_(LockClass::named("CopyOnWriteArrayList")) {}
	explicit CopyOnWriteArrayList(const LockClass& lockClass) : monitor_(lockClass) {}
	~CopyOnWriteArrayList() { delete array_.load(std::memory_order_acquire); }
	CopyOnWriteArrayList(const CopyOnWriteArrayList&) = delete;
	CopyOnWriteArrayList& operator=(const CopyOnWriteArrayList&) = delete;

	bool add(T value) {
		detail::ShimLock lock(monitor_);
		std::vector<T> items = copyLocked(1);
		items.push_back(std::move(value));
		publish(std::move(items));
		return true;
	}
	/** @throws IndexOutOfBoundsException */
	void add(int32_t index, T value) {
		detail::ShimLock lock(monitor_);
		std::vector<T> items = copyLocked(1);
		if (index < 0 || static_cast<size_t>(index) > items.size())
			detail::throwIndexOutOfBounds(index, static_cast<int64_t>(items.size()));
		items.insert(items.begin() + index, std::move(value));
		publish(std::move(items));
	}
	/** @return true if added (no Java-equal element present) */
	bool addIfAbsent(T value) {
		TaskScope::ensurePublished();
		detail::ShimLock lock(monitor_);
		if (indexOfLocked(detail::borrowOf(value)) >= 0)
			return false;
		std::vector<T> items = copyLocked(1);
		items.push_back(std::move(value));
		publish(std::move(items));
		return true;
	}
	template <std::ranges::input_range R>
	bool addAll(R&& values) {
		std::vector<T> added;
		for (auto&& value : values)
			added.push_back(detail::toElement<T>(std::forward<decltype(value)>(value)));
		if (added.empty())
			return false;
		detail::ShimLock lock(monitor_);
		std::vector<T> items = copyLocked(added.size());
		items.insert(items.end(), std::make_move_iterator(added.begin()), std::make_move_iterator(added.end()));
		publish(std::move(items));
		return true;
	}
	/** @throws IndexOutOfBoundsException */
	Element get(int32_t index) const {
		TaskScope::ensurePublished();
		const Array* array = load();
		size_t size = array != nullptr ? array->items.size() : 0;
		if (index < 0 || static_cast<size_t>(index) >= size)
			detail::throwIndexOutOfBounds(index, static_cast<int64_t>(size));
		return Element(array->items[static_cast<size_t>(index)]);
	}
	/** @throws IndexOutOfBoundsException */
	Element set(int32_t index, T value) {
		TaskScope::ensurePublished();
		detail::ShimLock lock(monitor_);
		std::vector<T> items = copyLocked(0);
		if (index < 0 || static_cast<size_t>(index) >= items.size())
			detail::throwIndexOutOfBounds(index, static_cast<int64_t>(items.size()));
		Element previous(array_.load(std::memory_order_acquire)->items[static_cast<size_t>(index)]); // stays readable: the old array is retired
		items[static_cast<size_t>(index)] = std::move(value);
		publish(std::move(items));
		return previous;
	}
	/** @throws IndexOutOfBoundsException */
	Element removeAt(int32_t index) {
		TaskScope::ensurePublished();
		detail::ShimLock lock(monitor_);
		const Array* array = array_.load(std::memory_order_acquire);
		size_t size = array != nullptr ? array->items.size() : 0;
		if (index < 0 || static_cast<size_t>(index) >= size)
			detail::throwIndexOutOfBounds(index, static_cast<int64_t>(size));
		Element previous(array->items[static_cast<size_t>(index)]);
		std::vector<T> items = copyLocked(0);
		items.erase(items.begin() + index);
		publish(std::move(items));
		return previous;
	}
	bool remove(const Element& value) {
		TaskScope::ensurePublished();
		detail::ShimLock lock(monitor_);
		int32_t index = indexOfLocked(value);
		if (index < 0)
			return false;
		std::vector<T> items = copyLocked(0);
		items.erase(items.begin() + index);
		publish(std::move(items));
		return true;
	}
	bool contains(const Element& value) const { return indexOf(value) >= 0; }
	int32_t indexOf(const Element& value) const {
		TaskScope::ensurePublished();
		return indexIn(load(), value);
	}
	int32_t size() const { return size_.load(std::memory_order_acquire); }
	bool isEmpty() const { return size_.load(std::memory_order_acquire) == 0; }
	void clear() {
		detail::ShimLock lock(monitor_);
		publish(std::vector<T>());
	}
	template <class Predicate>
		requires std::predicate<Predicate&, const Element&>
	bool removeIf(Predicate&& predicate) {
		TaskScope::ensurePublished();
		detail::ShimLock lock(monitor_);
		const Array* array = array_.load(std::memory_order_acquire);
		if (array == nullptr)
			return false;
		std::vector<T> kept;
		kept.reserve(array->items.size());
		for (const T& item : array->items) {
			if (!std::invoke(predicate, Element(item)))
				kept.push_back(item);
		}
		if (kept.size() == array->items.size())
			return false;
		publish(std::move(kept));
		return true;
	}
	template <class Action>
		requires std::invocable<Action&, const Element&>
	void forEach(Action&& action) const {
		for (Iterator it = begin(); it != end(); ++it)
			std::invoke(action, *it);
	}

	Iterator begin() const {
		TaskScope::ensurePublished();
		return Iterator(load());
	}
	std::default_sentinel_t end() const noexcept { return {}; }
	/** snapshot iterator; remove() throws UnsupportedOperationException (Java) */
	JavaIterator<Element> iterator() const { return JavaIterator<Element>(snapshot(), typename JavaIterator<Element>::Remover()); }
	std::vector<Element> snapshot() const {
		TaskScope::ensurePublished();
		std::vector<Element> elements;
		if (const Array* array = load()) {
			elements.reserve(array->items.size());
			for (const T& item : array->items)
				elements.push_back(Element(item));
		}
		return elements;
	}

	/** `synchronized (list)` and the writer lock */
	Monitor& monitor() const noexcept { return monitor_; }

private:
	const Array* load() const noexcept {
		AION_YIELD_POINT("CopyOnWriteArrayList::load");
		return array_.load(std::memory_order_acquire);
	}
	/** under the Monitor: a mutable copy of the current elements with room for `extra` more */
	std::vector<T> copyLocked(size_t extra) const {
		std::vector<T> items;
		if (const Array* array = array_.load(std::memory_order_acquire)) {
			items.reserve(array->items.size() + extra);
			items.insert(items.end(), array->items.begin(), array->items.end());
		} else {
			items.reserve(extra);
		}
		return items;
	}
	/** under the Monitor: publishes `items` as the new array and retires the old one */
	void publish(std::vector<T> items) {
		std::unique_ptr<Array> next = items.empty() ? nullptr : std::make_unique<Array>(std::move(items));
		int32_t size = next != nullptr ? static_cast<int32_t>(next->items.size()) : 0;
		AION_YIELD_POINT("CopyOnWriteArrayList::publish");
		Array* old = array_.exchange(next.release(), std::memory_order_acq_rel);
		size_.store(size, std::memory_order_release);
		if (old != nullptr)
			Reclaimer::retireNode(std::unique_ptr<RetiredNode>(old));
	}
	int32_t indexOfLocked(const Element& value) const { return indexIn(array_.load(std::memory_order_acquire), value); }
	static int32_t indexIn(const Array* array, const Element& value) {
		if (array == nullptr)
			return -1;
		for (size_t i = 0; i < array->items.size(); ++i) {
			if (detail::javaEquals(array->items[i], value))
				return static_cast<int32_t>(i);
		}
		return -1;
	}

	mutable Monitor monitor_;
	std::atomic<Array*> array_{nullptr};
	std::atomic<int32_t> size_{0};
};

/** Java: java.util.concurrent.CopyOnWriteArraySet (WebRewardService.pendingAscension). Same model as CopyOnWriteArrayList. */
template <class T>
class CopyOnWriteArraySet {
public:
	using value_type = T;
	using Element = Borrowed<T>;

	CopyOnWriteArraySet() = default;
	explicit CopyOnWriteArraySet(const LockClass& lockClass) : list_(lockClass) {}
	CopyOnWriteArraySet(const CopyOnWriteArraySet&) = delete;
	CopyOnWriteArraySet& operator=(const CopyOnWriteArraySet&) = delete;

	bool add(T value) { return list_.addIfAbsent(std::move(value)); }
	bool remove(const Element& value) { return list_.remove(value); }
	bool contains(const Element& value) const { return list_.contains(value); }
	int32_t size() const { return list_.size(); }
	bool isEmpty() const { return list_.isEmpty(); }
	void clear() { list_.clear(); }
	template <class Predicate>
		requires std::predicate<Predicate&, const Element&>
	bool removeIf(Predicate&& predicate) {
		return list_.removeIf(std::forward<Predicate>(predicate));
	}
	typename CopyOnWriteArrayList<T>::Iterator begin() const { return list_.begin(); }
	std::default_sentinel_t end() const noexcept { return {}; }
	JavaIterator<Element> iterator() const { return list_.iterator(); }
	std::vector<Element> snapshot() const { return list_.snapshot(); }

	Monitor& monitor() const noexcept { return list_.monitor(); }

private:
	CopyOnWriteArrayList<T> list_;
};

} // namespace aion::gameserver::runtime
