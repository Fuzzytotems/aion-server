#pragma once

#include <algorithm>
#include <atomic>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <initializer_list>
#include <iterator>
#include <memory>
#include <numeric>
#include <optional>
#include <ranges>
#include <type_traits>
#include <utility>
#include <vector>

#include "aion/commons/utils/Rnd.h"
#include "aion/gameserver/runtime/collections/Iterators.h"
#include "aion/gameserver/runtime/collections/JavaEquals.h"
#include "aion/gameserver/runtime/collections/detail/ShimSupport.h"
#include "aion/gameserver/runtime/lifetime/TaskScope.h"
#include "aion/gameserver/runtime/sync/LockClass.h"
#include "aion/gameserver/runtime/sync/Monitor.h"

namespace aion::gameserver::runtime {

/**
 * Shared-object replacement of java.util.ArrayList (design §3.3). Collection FIELDS of K4 classes use it; locals stay std::vector.
 *
 * Synchronization
 * - One reentrant collection Monitor (lock class `Owner::field`, pass AION_LOCK_CLASS; default "ArrayList") guards every operation, including
 *   user callbacks (removeIf predicates, sort comparators, replaceAll functions), exactly like Java code wrapped in `synchronized (list)`.
 *   Callbacks may take other Monitors (lockdep-tracked), call back into this list (reentrant), schedule, send packets or call DAOs.
 * - `SYNCHRONIZED(list)` (monitorOf) locks the same Monitor, so Java blocks that synchronize on the list are atomic with its operations.
 * - isEmpty() and size() are lock-free reads of an atomic size (fast path for hot notifiers, design §3.3).
 *
 * Elements
 * - T is the stored type: Ref<X> for objects (design §3.2), values otherwise. Reads return Borrowed<T> (Ptr<X> for Ref elements): reading
 *   element references is a pointer load, so it calls TaskScope::ensurePublished() (read barrier, checked C2). Value lists need no scope.
 * - remove(Object)/contains/indexOf use Java equals (JavaEquality<T>, RR-7); JavaIterator::remove uses identity.
 *
 * Iteration
 * - `for (Ptr<X> x : list)` and snapshot() iterate a copy taken under the Monitor. Re-entrant modification is memory-safe and never throws
 *   ConcurrentModificationException (deviation 16).
 * - iterator() returns a JavaIterator over a snapshot whose remove() deletes the last returned element by identity: at its snapshot position
 *   (adjusted for removals made through the iterator) if it is still there, otherwise its first identical occurrence; no-op if absent.
 *
 * Callbacks that modify the list (implementation choices, all memory-safe)
 * - removeIf evaluates the predicate for every element in order (like Java, before removing anything). If the predicate itself modified the
 *   list, the matching elements are removed by identity instead of by position.
 * - replaceAll: an element removed by the function itself is skipped.
 * - sort comparators and element equals()/compareTo() must not structurally modify the same list: that throws IllegalStateException (Java
 *   throws ConcurrentModificationException). Reads are allowed.
 *
 * Exceptions: index errors throw IndexOutOfBoundsException with Java's message; exceptions from callbacks propagate after the Monitor is released
 * (the list keeps the state reached so far, like Java).
 * Thread-safety: every member is thread-safe. Not copyable or movable (object state).
 */
template <class T>
class ArrayList {
public:
	using value_type = T;
	using Element = Borrowed<T>;

	ArrayList() : monitor_(LockClass::named("ArrayList")) {}
	explicit ArrayList(const LockClass& lockClass) : monitor_(lockClass) {}
	/** Java `new ArrayList<>(initialCapacity)` */
	explicit ArrayList(int32_t initialCapacity) : ArrayList() { items_.reserve(static_cast<size_t>(initialCapacity > 0 ? initialCapacity : 0)); }
	/** Java `new ArrayList<>(collection)` / `List.of(...)` copied into a mutable list */
	ArrayList(std::initializer_list<T> values) : ArrayList() {
		for (const T& value : values)
			add(value);
	}
	ArrayList(const ArrayList&) = delete;
	ArrayList& operator=(const ArrayList&) = delete;

	void add(T value);
	/** @throws IndexOutOfBoundsException */
	void add(int32_t index, T value);
	/** Appends all elements of a range of T or Borrowed<T> (another shim's snapshot(), a std::vector). @return true if changed */
	template <std::ranges::input_range R>
	bool addAll(R&& values);
	/** @throws IndexOutOfBoundsException */
	Element get(int32_t index) const;
	/** @return the previous element. @throws IndexOutOfBoundsException */
	Element set(int32_t index, T value);
	/** Java remove(int). @throws IndexOutOfBoundsException */
	Element removeAt(int32_t index);
	/** Java remove(Object): removes the first element Java-equal to `value`. */
	bool remove(const Element& value);
	bool contains(const Element& value) const;
	int32_t indexOf(const Element& value) const;
	int32_t lastIndexOf(const Element& value) const;
	int32_t size() const;
	/** lock-free */
	bool isEmpty() const noexcept { return size_.load(std::memory_order_acquire) == 0; }
	void clear();

	/** Java removeIf: evaluated under the Monitor on the live list, in order. @return true if any element was removed */
	template <class Predicate>
		requires std::predicate<Predicate&, const Element&>
	bool removeIf(Predicate&& predicate) {
		detail::readBarrier<T>();
		detail::ShimLock lock(monitor_);
		detail::checkNotBusy(busy_);
		std::vector<Element> elements = snapshotLocked();
		const uint32_t expectedModCount = modCount_;
		std::vector<bool> matches(elements.size(), false);
		bool any = false;
		for (size_t i = 0; i < elements.size(); ++i) {
			if (std::invoke(predicate, std::as_const(elements[i]))) {
				matches[i] = true;
				any = true;
			}
		}
		if (!any)
			return false;
		if (modCount_ == expectedModCount && items_.size() == elements.size()) {
			size_t kept = 0;
			for (size_t i = 0; i < items_.size(); ++i) {
				if (!matches[i]) {
					if (kept != i)
						items_[kept] = std::move(items_[i]);
					++kept;
				}
			}
			items_.erase(items_.begin() + static_cast<std::ptrdiff_t>(kept), items_.end());
		} else {
			for (size_t i = 0; i < elements.size(); ++i) {
				if (matches[i])
					eraseIdenticalLocked(elements[i], std::nullopt);
			}
		}
		changed();
		return true;
	}
	/** Java removeAll(collection) with Java equals. */
	template <std::ranges::input_range R>
	bool removeAll(R&& values);
	/** Java retainAll(collection) with Java equals. */
	template <std::ranges::input_range R>
	bool retainAll(R&& values);
	/** Java replaceAll(UnaryOperator) under the Monitor; the function returns T, Borrowed<T> or anything convertible to T. */
	template <class Function>
		requires std::invocable<Function&, const Element&>
	void replaceAll(Function&& function) {
		detail::readBarrier<T>();
		detail::ShimLock lock(monitor_);
		for (size_t i = 0; i < items_.size(); ++i) {
			T replacement = detail::toElement<T>(std::invoke(function, Element(items_[i])));
			if (i < items_.size())
				items_[i] = std::move(replacement);
		}
		++modCount_;
	}
	/** Java sort(Comparator) / Collections.sort (stable) under the Monitor; comparator returns int (Java) or bool (less). */
	template <class Comparator>
	void sort(Comparator&& comparator);
	/** Java sort(null): natural ordering (compareTo / operator<). */
	void sort(std::nullptr_t) { sort(JavaComparator<T>()); }
	/** Collections.shuffle(list) with the kernel's seeded Rnd. */
	void shuffle();

	/** Java forEach: iterates a snapshot (the action runs without the Monitor). */
	template <class Action>
		requires std::invocable<Action&, const Element&>
	void forEach(Action&& action) const {
		for (const Element& element : snapshot())
			std::invoke(action, element);
	}

	JavaIterator<Element> iterator() const;
	SnapshotIterator<Element> begin() const;
	std::default_sentinel_t end() const noexcept { return {}; }
	std::vector<Element> snapshot() const;

	/** `synchronized (list)` */
	Monitor& monitor() const noexcept { return monitor_; }

protected:
	/** under the Monitor: publishes the new size and counts a structural modification */
	void changed() noexcept {
		++modCount_;
		size_.store(static_cast<int32_t>(items_.size()), std::memory_order_release);
	}
	void checkIndex(int32_t index, size_t size) const {
		if (index < 0 || static_cast<size_t>(index) >= size)
			detail::throwIndexOutOfBounds(index, static_cast<int64_t>(size));
	}
	std::vector<Element> snapshotLocked() const {
		std::vector<Element> elements;
		elements.reserve(items_.size());
		for (const T& item : items_)
			elements.push_back(Element(item));
		return elements;
	}
	/** Removes the element identical to `element`, preferring `position`. @return true if removed */
	bool eraseIdenticalLocked(const Element& element, std::optional<size_t> position) {
		if (position && *position < items_.size() && detail::identical(detail::borrowOf(items_[*position]), element)) {
			items_.erase(items_.begin() + static_cast<std::ptrdiff_t>(*position));
			return true;
		}
		for (size_t i = 0; i < items_.size(); ++i) {
			if (detail::identical(detail::borrowOf(items_[i]), element)) {
				items_.erase(items_.begin() + static_cast<std::ptrdiff_t>(i));
				return true;
			}
		}
		return false;
	}
	/** Index of the first (or last) Java-equal element, under the Monitor. */
	std::optional<size_t> findLocked(const Element& value, bool last) const {
		detail::BusyScope busy(busy_);
		if (last) {
			for (size_t i = items_.size(); i-- > 0;) {
				if (i < items_.size() && detail::javaEquals(items_[i], value))
					return i;
			}
		} else {
			for (size_t i = 0; i < items_.size(); ++i) {
				if (detail::javaEquals(items_[i], value))
					return i;
			}
		}
		return std::nullopt;
	}
	template <std::ranges::input_range R>
	std::vector<Element> toElements(R&& values) const {
		detail::readBarrier<T>();
		std::vector<Element> elements;
		for (auto&& value : values)
			elements.push_back(Element(value));
		return elements;
	}

	mutable Monitor monitor_;
	std::vector<T> items_;
	std::atomic<int32_t> size_{0};
	/** structural modification counter (under the Monitor) */
	uint32_t modCount_ = 0;
	/** > 0 while the list calls element equals or a comparator on its storage (under the Monitor) */
	mutable int32_t busy_ = 0;
};

/**
 * java.util.LinkedList shim: ArrayList semantics plus the Deque operations used by the game server. Same synchronization and iteration rules.
 * getFirst, getLast, removeFirst, removeLast, element and pop on an empty list throw NoSuchElementException; peek and poll variants return an
 * empty Nullable. (Stored contiguously: first-element operations are O(n), which is irrelevant for the three Java LinkedList fields.)
 */
template <class T>
class LinkedList : public ArrayList<T> {
public:
	using Element = Borrowed<T>;

	LinkedList() : ArrayList<T>(LockClass::named("LinkedList")) {}
	explicit LinkedList(const LockClass& lockClass) : ArrayList<T>(lockClass) {}

	void addFirst(T value);
	void addLast(T value);
	bool offer(T value);
	bool offerFirst(T value);
	bool offerLast(T value);
	void push(T value);
	Element getFirst() const;
	Element getLast() const;
	Element element() const;
	Nullable<T> peek() const;
	Nullable<T> peekFirst() const;
	Nullable<T> peekLast() const;
	Element removeFirst();
	Element removeLast();
	Element pop();
	Nullable<T> poll();
	Nullable<T> pollFirst();
	Nullable<T> pollLast();

private:
	Nullable<T> peekAt(bool first) const {
		detail::readBarrier<T>();
		detail::ShimLock lock(this->monitor_);
		if (this->items_.empty())
			return Nullable<T>{};
		return Nullable<T>(first ? this->items_.front() : this->items_.back());
	}
	Nullable<T> pollAt(bool first) {
		detail::readBarrier<T>();
		detail::ShimLock lock(this->monitor_);
		detail::checkNotBusy(this->busy_);
		if (this->items_.empty())
			return Nullable<T>{};
		size_t index = first ? 0 : this->items_.size() - 1;
		Nullable<T> result(this->items_[index]);
		this->items_.erase(this->items_.begin() + static_cast<std::ptrdiff_t>(index));
		this->changed();
		return result;
	}
	Element getAt(bool first) const {
		detail::readBarrier<T>();
		detail::ShimLock lock(this->monitor_);
		if (this->items_.empty())
			detail::throwNoSuchElement("LinkedList is empty");
		return Element(first ? this->items_.front() : this->items_.back());
	}
	Element removeAtEnd(bool first) {
		detail::readBarrier<T>();
		detail::ShimLock lock(this->monitor_);
		detail::checkNotBusy(this->busy_);
		if (this->items_.empty())
			detail::throwNoSuchElement("LinkedList is empty");
		size_t index = first ? 0 : this->items_.size() - 1;
		Element result(this->items_[index]);
		this->items_.erase(this->items_.begin() + static_cast<std::ptrdiff_t>(index));
		this->changed();
		return result;
	}
};

// ------------------------------------------------------------------------------------------------------------------------------------------
// ArrayList implementation
// ------------------------------------------------------------------------------------------------------------------------------------------

template <class T>
void ArrayList<T>::add(T value) {
	detail::ShimLock lock(monitor_);
	detail::checkNotBusy(busy_);
	items_.push_back(std::move(value));
	changed();
}

template <class T>
void ArrayList<T>::add(int32_t index, T value) {
	detail::ShimLock lock(monitor_);
	detail::checkNotBusy(busy_);
	if (index < 0 || static_cast<size_t>(index) > items_.size())
		detail::throwIndexOutOfBounds(index, static_cast<int64_t>(items_.size()));
	items_.insert(items_.begin() + index, std::move(value));
	changed();
}

template <class T>
template <std::ranges::input_range R>
bool ArrayList<T>::addAll(R&& values) {
	std::vector<T> added;
	for (auto&& value : values)
		added.push_back(detail::toElement<T>(std::forward<decltype(value)>(value)));
	if (added.empty())
		return false;
	detail::ShimLock lock(monitor_);
	detail::checkNotBusy(busy_);
	items_.insert(items_.end(), std::make_move_iterator(added.begin()), std::make_move_iterator(added.end()));
	changed();
	return true;
}

template <class T>
typename ArrayList<T>::Element ArrayList<T>::get(int32_t index) const {
	detail::readBarrier<T>();
	detail::ShimLock lock(monitor_);
	checkIndex(index, items_.size());
	return Element(items_[static_cast<size_t>(index)]);
}

template <class T>
typename ArrayList<T>::Element ArrayList<T>::set(int32_t index, T value) {
	detail::readBarrier<T>();
	detail::ShimLock lock(monitor_);
	checkIndex(index, items_.size());
	Element previous(items_[static_cast<size_t>(index)]);
	items_[static_cast<size_t>(index)] = std::move(value);
	return previous;
}

template <class T>
typename ArrayList<T>::Element ArrayList<T>::removeAt(int32_t index) {
	detail::readBarrier<T>();
	detail::ShimLock lock(monitor_);
	detail::checkNotBusy(busy_);
	checkIndex(index, items_.size());
	Element previous(items_[static_cast<size_t>(index)]);
	items_.erase(items_.begin() + index);
	changed();
	return previous;
}

template <class T>
bool ArrayList<T>::remove(const Element& value) {
	detail::ShimLock lock(monitor_);
	detail::checkNotBusy(busy_);
	std::optional<size_t> index = findLocked(value, false);
	if (!index)
		return false;
	items_.erase(items_.begin() + static_cast<std::ptrdiff_t>(*index));
	changed();
	return true;
}

template <class T>
bool ArrayList<T>::contains(const Element& value) const {
	detail::ShimLock lock(monitor_);
	return findLocked(value, false).has_value();
}

template <class T>
int32_t ArrayList<T>::indexOf(const Element& value) const {
	detail::ShimLock lock(monitor_);
	std::optional<size_t> index = findLocked(value, false);
	return index ? static_cast<int32_t>(*index) : -1;
}

template <class T>
int32_t ArrayList<T>::lastIndexOf(const Element& value) const {
	detail::ShimLock lock(monitor_);
	std::optional<size_t> index = findLocked(value, true);
	return index ? static_cast<int32_t>(*index) : -1;
}

template <class T>
int32_t ArrayList<T>::size() const {
	return size_.load(std::memory_order_acquire);
}

template <class T>
void ArrayList<T>::clear() {
	std::vector<T> removed; // destroyed after the Monitor is released
	detail::ShimLock lock(monitor_);
	detail::checkNotBusy(busy_);
	removed.swap(items_);
	changed();
}

template <class T>
template <std::ranges::input_range R>
bool ArrayList<T>::removeAll(R&& values) {
	std::vector<Element> others = toElements(std::forward<R>(values));
	return removeIf([&others](const Element& element) {
		return std::ranges::any_of(others, [&element](const Element& other) { return JavaEquality<T>::equals(other, element); });
	});
}

template <class T>
template <std::ranges::input_range R>
bool ArrayList<T>::retainAll(R&& values) {
	std::vector<Element> others = toElements(std::forward<R>(values));
	return removeIf([&others](const Element& element) {
		return std::ranges::none_of(others, [&element](const Element& other) { return JavaEquality<T>::equals(other, element); });
	});
}

template <class T>
template <class Comparator>
void ArrayList<T>::sort(Comparator&& comparator) {
	detail::readBarrier<T>();
	detail::ShimLock lock(monitor_);
	detail::checkNotBusy(busy_);
	// Sorts a permutation of indices and applies it only after every comparison succeeded (review finding: std::stable_sort in place over the
	// elements lost elements and left moved-from null Refs when a comparator threw). A throwing comparator leaves the list unchanged (Java may
	// leave it partially reordered; neither loses or duplicates elements). The comparator may read the list and set() elements (the permutation
	// stays valid); structural changes throw IllegalStateException (busy guard).
	std::vector<size_t> order(items_.size());
	std::iota(order.begin(), order.end(), size_t{0});
	{
		detail::BusyScope busy(busy_);
		if constexpr (std::is_same_v<std::remove_cvref_t<Comparator>, JavaComparator<T>>) {
			std::stable_sort(order.begin(), order.end(), [this, &comparator](size_t a, size_t b) {
				return detail::compareElements<T>(comparator, detail::borrowOf(items_[a]), detail::borrowOf(items_[b])) < 0;
			});
		} else {
			std::stable_sort(order.begin(), order.end(), [this, &comparator](size_t a, size_t b) {
				return invokeComparator(comparator, detail::borrowOf(items_[a]), detail::borrowOf(items_[b])) < 0;
			});
		}
	}
	std::vector<T> sorted;
	sorted.reserve(items_.size()); // the only allocation: before any element moves
	for (size_t index : order)
		sorted.push_back(std::move(items_[index])); // element moves do not throw (Ref, numbers, strings)
	items_.swap(sorted);
	++modCount_;
}

template <class T>
void ArrayList<T>::shuffle() {
	detail::ShimLock lock(monitor_);
	detail::checkNotBusy(busy_);
	std::shuffle(items_.begin(), items_.end(), commons::utils::Rnd::generator());
	++modCount_;
}

template <class T>
JavaIterator<typename ArrayList<T>::Element> ArrayList<T>::iterator() const {
	auto* self = const_cast<ArrayList*>(this); // the iterator writes through to the (non-const) list it came from, like Java
	typename JavaIterator<Element>::IndexedRemover remover = [self, removed = size_t{0}](const Element& element, size_t index) mutable {
		detail::ShimLock lock(self->monitor_);
		detail::checkNotBusy(self->busy_);
		size_t expected = index >= removed ? index - removed : 0;
		if (!self->eraseIdenticalLocked(element, expected))
			return false;
		++removed;
		self->changed();
		return true;
	};
	return JavaIterator<Element>(snapshot(), std::move(remover));
}

template <class T>
SnapshotIterator<typename ArrayList<T>::Element> ArrayList<T>::begin() const {
	return SnapshotIterator<Element>(std::make_shared<const std::vector<Element>>(snapshot()));
}

template <class T>
std::vector<typename ArrayList<T>::Element> ArrayList<T>::snapshot() const {
	detail::readBarrier<T>();
	detail::ShimLock lock(monitor_);
	return snapshotLocked();
}

// ------------------------------------------------------------------------------------------------------------------------------------------
// LinkedList implementation
// ------------------------------------------------------------------------------------------------------------------------------------------

template <class T>
void LinkedList<T>::addFirst(T value) {
	this->add(0, std::move(value));
}
template <class T>
void LinkedList<T>::addLast(T value) {
	this->add(std::move(value));
}
template <class T>
bool LinkedList<T>::offer(T value) {
	this->add(std::move(value));
	return true;
}
template <class T>
bool LinkedList<T>::offerFirst(T value) {
	addFirst(std::move(value));
	return true;
}
template <class T>
bool LinkedList<T>::offerLast(T value) {
	this->add(std::move(value));
	return true;
}
template <class T>
void LinkedList<T>::push(T value) {
	addFirst(std::move(value));
}
template <class T>
typename LinkedList<T>::Element LinkedList<T>::getFirst() const {
	return getAt(true);
}
template <class T>
typename LinkedList<T>::Element LinkedList<T>::getLast() const {
	return getAt(false);
}
template <class T>
typename LinkedList<T>::Element LinkedList<T>::element() const {
	return getAt(true);
}
template <class T>
Nullable<T> LinkedList<T>::peek() const {
	return peekAt(true);
}
template <class T>
Nullable<T> LinkedList<T>::peekFirst() const {
	return peekAt(true);
}
template <class T>
Nullable<T> LinkedList<T>::peekLast() const {
	return peekAt(false);
}
template <class T>
typename LinkedList<T>::Element LinkedList<T>::removeFirst() {
	return removeAtEnd(true);
}
template <class T>
typename LinkedList<T>::Element LinkedList<T>::removeLast() {
	return removeAtEnd(false);
}
template <class T>
typename LinkedList<T>::Element LinkedList<T>::pop() {
	return removeAtEnd(true);
}
template <class T>
Nullable<T> LinkedList<T>::poll() {
	return pollAt(true);
}
template <class T>
Nullable<T> LinkedList<T>::pollFirst() {
	return pollAt(true);
}
template <class T>
Nullable<T> LinkedList<T>::pollLast() {
	return pollAt(false);
}

} // namespace aion::gameserver::runtime
