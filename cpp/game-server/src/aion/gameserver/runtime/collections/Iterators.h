#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <type_traits>
#include <functional>
#include <iterator>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"

namespace aion::gameserver::runtime {

/**
 * Range-for iterator over an owned snapshot (design §3.3 "Iteration uses snapshots"): `for (Ptr<Npc> npc : list)`.
 * The snapshot holds borrowed elements (Ptr for Ref elements, valid until the task ends) and is shared by copies of the iterator.
 * Iteration never observes later modifications and never throws ConcurrentModificationException (deviation 16).
 */
template <class E>
class SnapshotIterator {
public:
	using value_type = E;
	using difference_type = std::ptrdiff_t;

	SnapshotIterator() = default;
	explicit SnapshotIterator(std::shared_ptr<const std::vector<E>> items) : items(std::move(items)) {}

	const E& operator*() const { return (*items)[index]; }
	const E* operator->() const { return &(*items)[index]; }
	SnapshotIterator& operator++() {
		++index;
		return *this;
	}
	SnapshotIterator operator++(int) {
		SnapshotIterator old = *this;
		++index;
		return old;
	}
	friend bool operator==(const SnapshotIterator& it, std::default_sentinel_t) noexcept { return it.items == nullptr || it.index >= it.items->size(); }

private:
	std::shared_ptr<const std::vector<E>> items;
	size_t index = 0;
};

/**
 * Java explicit iterator (design §3.3, §14.1 rule 3, RR-8): `auto it = list.iterator(); while (it.hasNext()) { Ptr<X> x = it.next(); ...
 * it.remove(); }`. Iterates an owned snapshot; remove() deletes the element last returned by next() from the backing collection by IDENTITY
 * (Java's positional removal), under the collection's Monitor, and is a no-op if that element was already removed concurrently.
 * A JavaIterator is a local: never store it (lint L11).
 */
template <class E>
class JavaIterator {
public:
	/** removes one element identical to the argument from the backing collection; returns false if absent. Empty = unsupported (COW). */
	using Remover = std::function<bool(const E&)>;
	/**
	 * Like Remover, additionally receiving the snapshot index of the element (collections use it to find the element's position or its map key
	 * without an identity search).
	 */
	using IndexedRemover = std::function<bool(const E&, size_t snapshotIndex)>;

	JavaIterator(std::vector<E> snapshot, Remover remover) : items(std::move(snapshot)) {
		if (remover)
			this->remover = [remover = std::move(remover)](const E& element, size_t) { return remover(element); };
	}
	JavaIterator(std::vector<E> snapshot, IndexedRemover remover) : items(std::move(snapshot)), remover(std::move(remover)) {}

	bool hasNext() const noexcept { return index < items.size(); }

	/** @throws NoSuchElementException past the end */
	E next() {
		if (index >= items.size())
			throw NoSuchElementException("JavaIterator::next past the end");
		canRemove = true;
		return items[index++];
	}

	/**
	 * @throws IllegalStateException if next() was not called or remove() was already called for this element
	 * @throws UnsupportedOperationException for snapshot iterators of copy-on-write collections (Java behaviour)
	 */
	void remove() {
		if (!canRemove)
			throw IllegalStateException("JavaIterator::remove without next");
		if (!remover)
			throw UnsupportedOperationException("remove is not supported by this iterator");
		canRemove = false;
		remover(items[index - 1], index - 1);
	}

private:
	std::vector<E> items;
	IndexedRemover remover;
	size_t index = 0;
	bool canRemove = false;
};

/**
 * Snapshot-backed view returned by keySet()/values()/entrySet() of the synchronized map shims (design §3.3). Iterable, convertible to a
 * std::vector, and mutating operations write through to the backing map under its Monitor: iterator().remove(), removeIf(pred), remove(e).
 * A view is a local like an iterator (valid while the backing map is borrowed).
 */
template <class E>
class SnapshotView {
public:
	using Remover = std::function<bool(const E&)>;
	using IndexedRemover = typename JavaIterator<E>::IndexedRemover;
	using RemoveIf = std::function<bool(const std::function<bool(const E&)>&)>;

	SnapshotView(std::vector<E> items, Remover remover, RemoveIf removeIfFunction)
		: items(std::make_shared<const std::vector<E>>(std::move(items))), remover(std::move(remover)), removeIfFunction(std::move(removeIfFunction)) {}
	/** `elementRemover` implements remove(e) (Java equals); `iteratorRemover` implements iterator().remove() (positional/identity). */
	SnapshotView(std::vector<E> items, Remover elementRemover, IndexedRemover iteratorRemover, RemoveIf removeIfFunction)
		: items(std::make_shared<const std::vector<E>>(std::move(items))), remover(std::move(elementRemover)),
		  iteratorRemover(std::move(iteratorRemover)), removeIfFunction(std::move(removeIfFunction)) {}

	SnapshotIterator<E> begin() const { return SnapshotIterator<E>(items); }
	std::default_sentinel_t end() const noexcept { return {}; }
	int32_t size() const noexcept { return static_cast<int32_t>(items->size()); }
	bool isEmpty() const noexcept { return items->empty(); }
	const std::vector<E>& toVector() const noexcept { return *items; }
	operator std::vector<E>() const { return *items; }

	/** JavaIterator over the view's snapshot whose remove() writes through to the backing map. */
	JavaIterator<E> iterator() const {
		if (iteratorRemover)
			return JavaIterator<E>(*items, iteratorRemover);
		return JavaIterator<E>(*items, remover);
	}
	/** Removes matching entries from the backing map (evaluated under its Monitor on the live map). @return true if any was removed */
	bool removeIf(const std::function<bool(const E&)>& predicate) const { return removeIfFunction(predicate); }
	/** Removes the first matching element from the backing map. */
	bool remove(const E& element) const { return remover(element); }

private:
	std::shared_ptr<const std::vector<E>> items;
	Remover remover;
	IndexedRemover iteratorRemover;
	RemoveIf removeIfFunction;
};

/** Map entry handed out by entrySet()/snapshots: borrowed key and value, structured bindings `auto [key, value]`, Java accessors. */
template <class K, class V>
struct MapEntry {
	Borrowed<K> key;
	Borrowed<V> value;

	const Borrowed<K>& getKey() const noexcept { return key; }
	const Borrowed<V>& getValue() const noexcept { return value; }
};

/** Result type of compute/computeIfAbsent/computeIfPresent/merge callbacks: V for references (null = remove), std::optional<V> otherwise. */
template <class V>
using ComputeResult = std::conditional_t<std::is_same_v<Nullable<V>, std::optional<V>>, std::optional<V>, V>;

} // namespace aion::gameserver::runtime
