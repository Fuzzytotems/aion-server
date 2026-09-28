#pragma once

#include <algorithm>
#include <atomic>
#include <concepts>
#include <cstdint>
#include <functional>
#include <initializer_list>
#include <iterator>
#include <memory>
#include <ranges>
#include <vector>

#include "aion/gameserver/runtime/collections/HashMap.h"
#include "aion/gameserver/runtime/collections/Iterators.h"
#include "aion/gameserver/runtime/collections/JavaEquals.h"
#include "aion/gameserver/runtime/collections/detail/MapStores.h"
#include "aion/gameserver/runtime/collections/detail/ShimSupport.h"
#include "aion/gameserver/runtime/sync/LockClass.h"
#include "aion/gameserver/runtime/sync/Monitor.h"

namespace aion::gameserver::runtime {

/**
 * Common implementation of the plain java.util set shims (design §3.3): HashSet, LinkedHashSet, TreeSet. Same rules as SynchronizedMap:
 * one reentrant collection Monitor guards every operation including callbacks; SYNCHRONIZED(set) locks it; isEmpty() is lock-free; membership
 * uses Java equals/hashCode (HASH, INSERTION) or the comparator/natural ordering (SORTED); range-for, snapshot() and iterator() use snapshots,
 * iterator().remove() removes by identity (the stored instance, not a Java-equal replacement added meanwhile). removeIf evaluates the predicate
 * on a snapshot under the Monitor and removes the identical instances. Element equals/hashCode/compareTo and comparators must not structurally
 * modify the same set (IllegalStateException). Thread-safety: every member is thread-safe. Not copyable or movable.
 */
template <class T, MapOrder Order>
class SynchronizedSet {
protected:
	static constexpr bool SORTED_STORE = Order == MapOrder::SORTED || Order == MapOrder::ENUM_ORDINAL;
	using Store = std::conditional_t<SORTED_STORE, detail::SortedStore<T, detail::Present>, detail::HashedStore<T, detail::Present>>;
	using Position = typename Store::Position;

public:
	using value_type = T;
	using Element = Borrowed<T>;

	explicit SynchronizedSet(const LockClass& lockClass) : monitor_(lockClass), store_(&comparator_) {}
	SynchronizedSet(const SynchronizedSet&) = delete;
	SynchronizedSet& operator=(const SynchronizedSet&) = delete;

	/** @return true if the set did not contain a Java-equal element */
	bool add(T value) {
		detail::ShimLock lock(monitor_);
		detail::checkNotBusy(busy_);
		Position position = locate(detail::borrowOf(value));
		if (Store::found(position))
			return false;
		store_.insert(position, std::move(value), detail::Present{});
		changed();
		return true;
	}
	template <std::ranges::input_range R>
	bool addAll(R&& values) {
		std::vector<T> added;
		for (auto&& value : values)
			added.push_back(detail::toElement<T>(std::forward<decltype(value)>(value)));
		detail::ShimLock lock(monitor_);
		bool changedAny = false;
		for (T& value : added)
			changedAny |= add(std::move(value));
		return changedAny;
	}
	bool remove(const Element& value) {
		detail::ShimLock lock(monitor_);
		detail::checkNotBusy(busy_);
		Position position = locate(value);
		if (!Store::found(position))
			return false;
		store_.erase(position);
		changed();
		return true;
	}
	template <std::ranges::input_range R>
	bool removeAll(R&& values) {
		std::vector<Element> others = toElements(std::forward<R>(values));
		detail::ShimLock lock(monitor_);
		bool changedAny = false;
		for (const Element& other : others)
			changedAny |= remove(other);
		return changedAny;
	}
	template <std::ranges::input_range R>
	bool retainAll(R&& values) {
		std::vector<Element> others = toElements(std::forward<R>(values));
		return removeIf([&others](const Element& element) {
			return std::ranges::none_of(others, [&element](const Element& other) { return JavaEquality<T>::equals(other, element); });
		});
	}
	bool contains(const Element& value) const {
		detail::ShimLock lock(monitor_);
		return Store::found(locate(value));
	}
	template <std::ranges::input_range R>
	bool containsAll(R&& values) const {
		std::vector<Element> others = toElements(std::forward<R>(values));
		detail::ShimLock lock(monitor_);
		return std::ranges::all_of(others, [this](const Element& other) { return Store::found(locate(other)); });
	}
	int32_t size() const { return size_.load(std::memory_order_acquire); }
	/** lock-free */
	bool isEmpty() const noexcept { return size_.load(std::memory_order_acquire) == 0; }
	void clear() {
		detail::ShimLock lock(monitor_);
		detail::checkNotBusy(busy_);
		store_.clear();
		changed();
	}
	/** evaluated under the Monitor on a snapshot of the set */
	template <class Predicate>
		requires std::predicate<Predicate&, const Element&>
	bool removeIf(Predicate&& predicate) {
		detail::readBarrier<T>();
		detail::ShimLock lock(monitor_);
		detail::checkNotBusy(busy_);
		bool any = false;
		for (const Element& element : elementsLocked()) {
			if (std::invoke(predicate, element))
				any |= removeIdenticalLocked(element);
		}
		return any;
	}
	/** over a snapshot */
	template <class Action>
		requires std::invocable<Action&, const Element&>
	void forEach(Action&& action) const {
		for (const Element& element : snapshot())
			std::invoke(action, element);
	}

	JavaIterator<Element> iterator() const {
		auto* self = const_cast<SynchronizedSet*>(this);
		typename JavaIterator<Element>::Remover remover = [self](const Element& element) {
			detail::ShimLock lock(self->monitor_);
			detail::checkNotBusy(self->busy_);
			return self->removeIdenticalLocked(element);
		};
		return JavaIterator<Element>(snapshot(), std::move(remover));
	}
	SnapshotIterator<Element> begin() const { return SnapshotIterator<Element>(std::make_shared<const std::vector<Element>>(snapshot())); }
	std::default_sentinel_t end() const noexcept { return {}; }
	std::vector<Element> snapshot() const {
		detail::readBarrier<T>();
		detail::ShimLock lock(monitor_);
		return elementsLocked();
	}

	/** `synchronized (set)` */
	Monitor& monitor() const noexcept { return monitor_; }

protected:
	Position locate(const Element& value) const {
		detail::BusyScope busy(busy_);
		return store_.locate(value);
	}
	void changed() noexcept { size_.store(static_cast<int32_t>(store_.size()), std::memory_order_release); }
	std::vector<Element> elementsLocked() const {
		std::vector<Element> elements;
		elements.reserve(store_.size());
		store_.forEach([&elements](const T& value, detail::Present&) { elements.push_back(Element(value)); });
		return elements;
	}
	bool removeIdenticalLocked(const Element& element) {
		Position position = locate(element);
		if (!Store::found(position) || !detail::identical(detail::borrowOf(store_.key(position)), element))
			return false;
		store_.erase(position);
		changed();
		return true;
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
	std::atomic<int32_t> size_{0};
	mutable int32_t busy_ = 0;
	/** TreeSet comparator (empty: natural ordering) */
	JavaComparator<T> comparator_;
	mutable Store store_;
};

/** Java: java.util.HashSet */
template <class T>
class HashSet : public SynchronizedSet<T, MapOrder::HASH> {
public:
	HashSet() : SynchronizedSet<T, MapOrder::HASH>(LockClass::named("HashSet")) {}
	explicit HashSet(const LockClass& lockClass) : SynchronizedSet<T, MapOrder::HASH>(lockClass) {}
	explicit HashSet(int32_t) : HashSet() {}
};

/** Java: java.util.LinkedHashSet (insertion order) */
template <class T>
class LinkedHashSet : public SynchronizedSet<T, MapOrder::INSERTION> {
public:
	LinkedHashSet() : SynchronizedSet<T, MapOrder::INSERTION>(LockClass::named("LinkedHashSet")) {}
	explicit LinkedHashSet(const LockClass& lockClass) : SynchronizedSet<T, MapOrder::INSERTION>(lockClass) {}
};

/** Java: java.util.TreeSet (comparator given at construction, called under the Monitor, or natural ordering). */
template <class T>
class TreeSet : public SynchronizedSet<T, MapOrder::SORTED> {
	using Base = SynchronizedSet<T, MapOrder::SORTED>;

public:
	using Element = Borrowed<T>;

	TreeSet() : Base(LockClass::named("TreeSet")) {}
	explicit TreeSet(const LockClass& lockClass) : Base(lockClass) {}
	explicit TreeSet(JavaComparator<T> comparator) : TreeSet() { this->comparator_ = std::move(comparator); }
	TreeSet(const LockClass& lockClass, JavaComparator<T> comparator) : TreeSet(lockClass) { this->comparator_ = std::move(comparator); }

	/** @throws NoSuchElementException if empty */
	Element first() const { return endElement(true); }
	/** @throws NoSuchElementException if empty */
	Element last() const { return endElement(false); }
	Nullable<T> pollFirst() { return pollEnd(true); }
	Nullable<T> pollLast() { return pollEnd(false); }
	Nullable<T> floor(const Element& value) const { return navigate(value, true, true); }
	Nullable<T> ceiling(const Element& value) const { return navigate(value, false, true); }
	Nullable<T> lower(const Element& value) const { return navigate(value, true, false); }
	Nullable<T> higher(const Element& value) const { return navigate(value, false, false); }
	std::vector<Element> descendingSet() const {
		std::vector<Element> elements = this->snapshot();
		std::reverse(elements.begin(), elements.end());
		return elements;
	}

private:
	Element endElement(bool first) const {
		detail::readBarrier<T>();
		detail::ShimLock lock(this->monitor_);
		auto& map = this->store_.map();
		if (map.empty())
			detail::throwNoSuchElement("TreeSet is empty");
		return Element(first ? map.begin()->first : std::prev(map.end())->first);
	}
	Nullable<T> pollEnd(bool first) {
		detail::readBarrier<T>();
		detail::ShimLock lock(this->monitor_);
		detail::checkNotBusy(this->busy_);
		auto& map = this->store_.map();
		if (map.empty())
			return Nullable<T>{};
		auto it = first ? map.begin() : std::prev(map.end());
		Nullable<T> result(it->first);
		map.erase(it);
		this->changed();
		return result;
	}
	/** below: floor/lower (greatest element <= / < value); otherwise ceiling/higher (least element >= / > value) */
	Nullable<T> navigate(const Element& value, bool below, bool inclusive) const {
		detail::readBarrier<T>();
		detail::ShimLock lock(this->monitor_);
		detail::BusyScope busy(this->busy_);
		auto& map = this->store_.map();
		if (below) {
			auto it = inclusive ? map.upper_bound(value) : map.lower_bound(value);
			if (it == map.begin())
				return Nullable<T>{};
			return Nullable<T>(std::prev(it)->first);
		}
		auto it = inclusive ? map.lower_bound(value) : map.upper_bound(value);
		if (it == map.end())
			return Nullable<T>{};
		return Nullable<T>(it->first);
	}
};

} // namespace aion::gameserver::runtime
