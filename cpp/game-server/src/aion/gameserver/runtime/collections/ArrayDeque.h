#pragma once

#include <algorithm>
#include <atomic>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <functional>
#include <iterator>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "aion/gameserver/runtime/collections/Iterators.h"
#include "aion/gameserver/runtime/collections/JavaEquals.h"
#include "aion/gameserver/runtime/collections/detail/ShimSupport.h"
#include "aion/gameserver/runtime/sync/LockClass.h"
#include "aion/gameserver/runtime/sync/Monitor.h"

namespace aion::gameserver::runtime {

namespace detail {

/**
 * Common implementation of the Monitor-guarded sequence shims (ArrayDeque, and the Monitor fallback of the concurrent linked queues):
 * a std::deque under one reentrant Monitor, atomic size for lock-free isEmpty(), snapshot iteration, identity JavaIterator::remove, Java equals
 * for contains/remove(Object), null elements rejected with NullPointerException (Java ArrayDeque/ConcurrentLinkedQueue behaviour).
 */
template <class T>
class MonitorDeque {
public:
	using value_type = T;
	using Element = Borrowed<T>;

	explicit MonitorDeque(const LockClass& lockClass) : monitor_(lockClass) {}
	MonitorDeque(const MonitorDeque&) = delete;
	MonitorDeque& operator=(const MonitorDeque&) = delete;

	bool add(T value) {
		addLast(std::move(value));
		return true;
	}
	void addFirst(T value) { insert(std::move(value), true); }
	void addLast(T value) { insert(std::move(value), false); }
	bool offer(T value) {
		addLast(std::move(value));
		return true;
	}
	bool offerFirst(T value) {
		addFirst(std::move(value));
		return true;
	}
	bool offerLast(T value) {
		addLast(std::move(value));
		return true;
	}
	void push(T value) { addFirst(std::move(value)); }
	Element pop() { return removeEnd(true); }
	Element element() const { return getEnd(true); }
	Element getFirst() const { return getEnd(true); }
	Element getLast() const { return getEnd(false); }
	Element removeFirst() { return removeEnd(true); }
	Element removeLast() { return removeEnd(false); }
	Nullable<T> peek() const { return peekEnd(true); }
	Nullable<T> peekFirst() const { return peekEnd(true); }
	Nullable<T> peekLast() const { return peekEnd(false); }
	Nullable<T> poll() { return pollEnd(true); }
	Nullable<T> pollFirst() { return pollEnd(true); }
	Nullable<T> pollLast() { return pollEnd(false); }

	/** Java remove(Object): the first Java-equal element */
	bool remove(const Element& value) {
		ShimLock lock(monitor_);
		checkNotBusy(busy_);
		std::optional<size_t> index = find(value);
		if (!index)
			return false;
		items_.erase(items_.begin() + static_cast<std::ptrdiff_t>(*index));
		changed();
		return true;
	}
	bool contains(const Element& value) const {
		ShimLock lock(monitor_);
		return find(value).has_value();
	}
	int32_t size() const { return size_.load(std::memory_order_acquire); }
	bool isEmpty() const noexcept { return size_.load(std::memory_order_acquire) == 0; }
	void clear() {
		std::deque<T> removed;
		ShimLock lock(monitor_);
		checkNotBusy(busy_);
		removed.swap(items_);
		changed();
	}
	/** Predicate evaluated under the Monitor for every element in order, then matching elements are removed by identity. */
	template <class Predicate>
		requires std::predicate<Predicate&, const Element&>
	bool removeIf(Predicate&& predicate) {
		readBarrier<T>();
		ShimLock lock(monitor_);
		checkNotBusy(busy_);
		std::vector<Element> elements = snapshotLocked();
		bool any = false;
		for (const Element& element : elements) {
			if (std::invoke(predicate, element)) {
				any = true;
				eraseIdentical(element, std::nullopt);
			}
		}
		if (any)
			changed();
		return any;
	}

	JavaIterator<Element> iterator() const {
		auto* self = const_cast<MonitorDeque*>(this);
		typename JavaIterator<Element>::IndexedRemover remover = [self, removed = size_t{0}](const Element& element, size_t index) mutable {
			ShimLock lock(self->monitor_);
			checkNotBusy(self->busy_);
			if (!self->eraseIdentical(element, index >= removed ? index - removed : 0))
				return false;
			++removed;
			self->changed();
			return true;
		};
		return JavaIterator<Element>(snapshot(), std::move(remover));
	}
	SnapshotIterator<Element> begin() const { return SnapshotIterator<Element>(std::make_shared<const std::vector<Element>>(snapshot())); }
	std::default_sentinel_t end() const noexcept { return {}; }
	std::vector<Element> snapshot() const {
		readBarrier<T>();
		ShimLock lock(monitor_);
		return snapshotLocked();
	}

	Monitor& monitor() const noexcept { return monitor_; }

private:
	void insert(T value, bool first) {
		if (isNull(value))
			throwNullElement("null elements are not permitted");
		ShimLock lock(monitor_);
		checkNotBusy(busy_);
		if (first)
			items_.push_front(std::move(value));
		else
			items_.push_back(std::move(value));
		changed();
	}
	Element getEnd(bool first) const {
		readBarrier<T>();
		ShimLock lock(monitor_);
		if (items_.empty())
			throwNoSuchElement("collection is empty");
		return Element(first ? items_.front() : items_.back());
	}
	Element removeEnd(bool first) {
		readBarrier<T>();
		ShimLock lock(monitor_);
		checkNotBusy(busy_);
		if (items_.empty())
			throwNoSuchElement("collection is empty");
		Element result(first ? items_.front() : items_.back());
		if (first)
			items_.pop_front();
		else
			items_.pop_back();
		changed();
		return result;
	}
	Nullable<T> peekEnd(bool first) const {
		readBarrier<T>();
		ShimLock lock(monitor_);
		if (items_.empty())
			return Nullable<T>{};
		return Nullable<T>(first ? items_.front() : items_.back());
	}
	Nullable<T> pollEnd(bool first) {
		readBarrier<T>();
		ShimLock lock(monitor_);
		checkNotBusy(busy_);
		if (items_.empty())
			return Nullable<T>{};
		Nullable<T> result(first ? items_.front() : items_.back());
		if (first)
			items_.pop_front();
		else
			items_.pop_back();
		changed();
		return result;
	}
	std::optional<size_t> find(const Element& value) const {
		BusyScope busy(busy_);
		for (size_t i = 0; i < items_.size(); ++i) {
			if (javaEquals(items_[i], value))
				return i;
		}
		return std::nullopt;
	}
	bool eraseIdentical(const Element& element, std::optional<size_t> position) {
		if (position && *position < items_.size() && identical(borrowOf(items_[*position]), element)) {
			items_.erase(items_.begin() + static_cast<std::ptrdiff_t>(*position));
			return true;
		}
		for (size_t i = 0; i < items_.size(); ++i) {
			if (identical(borrowOf(items_[i]), element)) {
				items_.erase(items_.begin() + static_cast<std::ptrdiff_t>(i));
				return true;
			}
		}
		return false;
	}
	std::vector<Element> snapshotLocked() const {
		std::vector<Element> elements;
		elements.reserve(items_.size());
		for (const T& item : items_)
			elements.push_back(Element(item));
		return elements;
	}
	void changed() noexcept { size_.store(static_cast<int32_t>(items_.size()), std::memory_order_release); }

	mutable Monitor monitor_;
	std::atomic<int32_t> size_{0};
	mutable int32_t busy_ = 0;
	std::deque<T> items_;
};

} // namespace detail

/**
 * Java: java.util.ArrayDeque (design §3.3). Same synchronization rules as ArrayList: one reentrant collection Monitor for every operation and
 * callback, SYNCHRONIZED(deque) locks it, lock-free isEmpty(), snapshot iteration (head to tail), JavaIterator::remove by identity, Java equals
 * for contains/remove(Object). Null elements are rejected like Java (NullPointerException for null Refs).
 * getFirst/getLast/removeFirst/removeLast/element/pop throw NoSuchElementException when empty; peek/poll variants return an empty
 * Nullable. Thread-safety: every member is thread-safe.
 */
template <class T>
class ArrayDeque : public detail::MonitorDeque<T> {
public:
	ArrayDeque() : detail::MonitorDeque<T>(LockClass::named("ArrayDeque")) {}
	explicit ArrayDeque(const LockClass& lockClass) : detail::MonitorDeque<T>(lockClass) {}
	explicit ArrayDeque(int32_t) : ArrayDeque() {}
};

/**
 * Java: java.util.PriorityQueue (listed by design §3.3; unused by the current Java sources). Comparator (called under the Monitor) or natural
 * ordering; iteration order of snapshots is unspecified like Java (heap order). Same synchronization rules as ArrayDeque. The comparator must not
 * structurally modify the same queue (IllegalStateException).
 * Exception safety (review finding): the binary heap is sifted with element swaps, like Java's siftUp/siftDown keep elements in array slots, so
 * a throwing comparator never loses an element or leaves a null: offer keeps the new element (possibly out of heap order), poll has removed
 * the head (the exception replaces the result) and keeps all others, remove keeps all others.
 */
template <class T>
class PriorityQueue {
public:
	using value_type = T;
	using Element = Borrowed<T>;

	PriorityQueue() : monitor_(LockClass::named("PriorityQueue")) {}
	explicit PriorityQueue(const LockClass& lockClass) : monitor_(lockClass) {}
	explicit PriorityQueue(JavaComparator<T> comparator) : PriorityQueue() { comparator_ = std::move(comparator); }
	PriorityQueue(const LockClass& lockClass, JavaComparator<T> comparator) : PriorityQueue(lockClass) { comparator_ = std::move(comparator); }
	PriorityQueue(const PriorityQueue&) = delete;
	PriorityQueue& operator=(const PriorityQueue&) = delete;

	bool add(T value) { return offer(std::move(value)); }
	bool offer(T value) {
		if (detail::isNull(value))
			detail::throwNullElement("PriorityQueue does not permit null elements");
		detail::ShimLock lock(monitor_);
		detail::checkNotBusy(busy_);
		detail::BusyScope busy(busy_);
		heap_.push_back(std::move(value));
		changed();
		siftUp(heap_.size() - 1); // a throwing comparator leaves the element in an unordered position (Java too)
		return true;
	}
	Nullable<T> peek() const {
		detail::readBarrier<T>();
		detail::ShimLock lock(monitor_);
		if (heap_.empty())
			return Nullable<T>{};
		return Nullable<T>(heap_.front());
	}
	Nullable<T> poll() {
		detail::readBarrier<T>();
		detail::ShimLock lock(monitor_);
		detail::checkNotBusy(busy_);
		if (heap_.empty())
			return Nullable<T>{};
		Nullable<T> result(heap_.front());
		removeAtLocked(0);
		return result;
	}
	/** @throws NoSuchElementException if empty */
	Element element() const {
		detail::readBarrier<T>();
		detail::ShimLock lock(monitor_);
		if (heap_.empty())
			detail::throwNoSuchElement("PriorityQueue is empty");
		return Element(heap_.front());
	}
	bool remove(const Element& value) {
		detail::ShimLock lock(monitor_);
		detail::checkNotBusy(busy_);
		std::optional<size_t> found;
		{
			detail::BusyScope busy(busy_);
			for (size_t i = 0; i < heap_.size() && !found; ++i) {
				if (detail::javaEquals(heap_[i], value))
					found = i;
			}
		}
		if (!found)
			return false;
		removeAtLocked(*found);
		return true;
	}
	bool contains(const Element& value) const {
		detail::ShimLock lock(monitor_);
		detail::BusyScope busy(busy_);
		for (const T& item : heap_) {
			if (detail::javaEquals(item, value))
				return true;
		}
		return false;
	}
	int32_t size() const { return size_.load(std::memory_order_acquire); }
	bool isEmpty() const noexcept { return size_.load(std::memory_order_acquire) == 0; }
	void clear() {
		std::vector<T> removed;
		detail::ShimLock lock(monitor_);
		detail::checkNotBusy(busy_);
		removed.swap(heap_);
		changed();
	}

	JavaIterator<Element> iterator() const {
		auto* self = const_cast<PriorityQueue*>(this);
		typename JavaIterator<Element>::Remover remover = [self](const Element& element) {
			detail::ShimLock lock(self->monitor_);
			detail::checkNotBusy(self->busy_);
			for (size_t i = 0; i < self->heap_.size(); ++i) {
				if (detail::identical(detail::borrowOf(self->heap_[i]), element)) {
					self->removeAtLocked(i);
					return true;
				}
			}
			return false;
		};
		return JavaIterator<Element>(snapshot(), std::move(remover));
	}
	SnapshotIterator<Element> begin() const { return SnapshotIterator<Element>(std::make_shared<const std::vector<Element>>(snapshot())); }
	std::default_sentinel_t end() const noexcept { return {}; }
	std::vector<Element> snapshot() const {
		detail::readBarrier<T>();
		detail::ShimLock lock(monitor_);
		std::vector<Element> elements;
		elements.reserve(heap_.size());
		for (const T& item : heap_)
			elements.push_back(Element(item));
		return elements;
	}

	Monitor& monitor() const noexcept { return monitor_; }

private:
	/** under the Monitor and the busy guard: comparator result for the elements at two heap slots */
	int32_t compareAt(size_t a, size_t b) const { return detail::compareElements<T>(comparator_, detail::borrowOf(heap_[a]), detail::borrowOf(heap_[b])); }
	/** min-heap sift up by swaps (every element stays in a slot if the comparator throws) */
	void siftUp(size_t index) {
		while (index > 0) {
			size_t parent = (index - 1) / 2;
			if (compareAt(index, parent) >= 0)
				break;
			std::swap(heap_[index], heap_[parent]);
			index = parent;
		}
	}
	/** min-heap sift down by swaps */
	void siftDown(size_t index) {
		for (;;) {
			size_t smallest = index;
			size_t left = 2 * index + 1;
			if (left >= heap_.size())
				return;
			if (compareAt(left, smallest) < 0)
				smallest = left;
			if (size_t right = left + 1; right < heap_.size() && compareAt(right, smallest) < 0)
				smallest = right;
			if (smallest == index)
				return;
			std::swap(heap_[index], heap_[smallest]);
			index = smallest;
		}
	}
	/** Java PriorityQueue.removeAt under the Monitor: the last element fills the slot and is sifted down, then up */
	void removeAtLocked(size_t index) {
		if (index + 1 != heap_.size())
			std::swap(heap_[index], heap_.back());
		[[maybe_unused]] T removed = std::move(heap_.back()); // a Ref release never runs user code, so it may happen under the Monitor
		heap_.pop_back();
		changed();
		if (index < heap_.size()) {
			detail::BusyScope busy(busy_);
			siftDown(index);
			siftUp(index);
		}
	}
	void changed() noexcept { size_.store(static_cast<int32_t>(heap_.size()), std::memory_order_release); }

	mutable Monitor monitor_;
	std::atomic<int32_t> size_{0};
	mutable int32_t busy_ = 0;
	JavaComparator<T> comparator_;
	std::vector<T> heap_;
};

} // namespace aion::gameserver::runtime
