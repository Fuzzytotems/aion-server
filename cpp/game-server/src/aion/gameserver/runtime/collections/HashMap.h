#pragma once

#include <algorithm>
#include <atomic>
#include <concepts>
#include <cstdint>
#include <functional>
#include <iterator>
#include <memory>
#include <optional>
#include <type_traits>
#include <utility>
#include <vector>

#include "aion/gameserver/runtime/collections/Iterators.h"
#include "aion/gameserver/runtime/collections/JavaEquals.h"
#include "aion/gameserver/runtime/collections/detail/MapStores.h"
#include "aion/gameserver/runtime/collections/detail/ShimSupport.h"
#include "aion/gameserver/runtime/sync/LockClass.h"
#include "aion/gameserver/runtime/sync/Monitor.h"

namespace aion::gameserver::runtime {

/** Iteration order of a synchronized map shim. */
enum class MapOrder : uint8_t {
	/** java.util.HashMap: unspecified (never rely on it; the implementation happens to keep insertion order) */
	HASH,
	/** java.util.LinkedHashMap: insertion order (re-putting an existing key keeps its position) */
	INSERTION,
	/** java.util.TreeMap: comparator / natural order of keys */
	SORTED,
	/** java.util.EnumMap: ordinal order of enum keys */
	ENUM_ORDINAL,
};

/**
 * Common implementation of the plain java.util map shims (design §3.3): HashMap, LinkedHashMap, TreeMap, EnumMap.
 *
 * Synchronization: one reentrant collection Monitor (lock class `Owner::field` via AION_LOCK_CLASS, default the shim name) guards every
 * operation INCLUDING user callbacks (compute/computeIfAbsent/computeIfPresent/merge remapping functions, removeIf predicates, replaceAll
 * functions, comparators). Callbacks may take other Monitors, call back into this map (reentrant), schedule, send packets or call DAOs.
 * `SYNCHRONIZED(map)` locks the same Monitor. isEmpty() and size() are lock-free.
 *
 * Callbacks modifying the same map (decided by the collections stage, see DEVIATIONS "collection shims"):
 * - compute-like callbacks may modify OTHER keys of the same map. A write (put, remove, replace, compute..., merge; putIfAbsent and
 *   computeIfAbsent only if the key is absent; clear excluded) of the key whose callback is running throws IllegalStateException("Recursive
 *   update"), like ConcurrentHashMap. Java's HashMap would throw
 *   ConcurrentModificationException after the callback for ANY structural change; the shim never throws that exception (deviation 16).
 * - removeIf/replaceAll evaluate the callback on a snapshot of the entries taken under the Monitor, in iteration order; an entry is removed or
 *   replaced only if it is still mapped (removeIf: to the identical value). Java evaluates on the live map; without re-entrant modification the
 *   result is the same.
 * - comparators and key equals/hashCode must not structurally modify the same map: that throws IllegalStateException. Reads are allowed.
 *
 * Keys and values: stored as K and V (Ref<X> for objects). Lookups take Borrowed<K>; reads return Nullable<V> (Ptr<X> for references, which
 * loads a pointer: read barrier; std::optional<V> for values) or Borrowed<V>. Keys use Java equals/hashCode (JavaEquality<K>) for HASH and
 * INSERTION order, the comparator/natural ordering (JavaOrdering<K>) for SORTED and ENUM_ORDINAL. Null keys and values are allowed like Java
 * (a null Ref value is a present mapping whose get() returns a null Ptr).
 *
 * Callbacks: compute: (const Borrowed<K>& key, Nullable<V> old) -> ComputeResult<V>, or (Nullable<V> old) -> ComputeResult<V>;
 * computeIfAbsent: (const Borrowed<K>& key) -> ComputeResult<V>, or () -> ComputeResult<V>; computeIfPresent: (const Borrowed<K>&, Borrowed<V>)
 * or (Borrowed<V>) -> ComputeResult<V>; merge: (Borrowed<V> old, Borrowed<V> given) -> ComputeResult<V>. A null/empty result removes the
 * mapping (Java). Callbacks returning Ptr<X>, Ref<X>, std::optional<V> or a plain V are all accepted.
 *
 * Iteration: keySet()/values()/entrySet() return SnapshotViews taken under the Monitor (range-for, conversion to std::vector, iterator().remove()
 * and removeIf write through; iterator().remove() removes the entry only if the key is still mapped to the identical value).
 * No ConcurrentModificationException (deviation 16).
 * Thread-safety: every member is thread-safe. Not copyable or movable.
 */
template <class K, class V, MapOrder Order>
class SynchronizedMap {
protected:
	static constexpr bool SORTED_STORE = Order == MapOrder::SORTED || Order == MapOrder::ENUM_ORDINAL;
	using Store = std::conditional_t<SORTED_STORE, detail::SortedStore<K, V>, detail::HashedStore<K, V>>;
	using Position = typename Store::Position;

public:
	using key_type = K;
	using mapped_type = V;
	using Key = Borrowed<K>;
	using Value = Borrowed<V>;
	using Entry = MapEntry<K, V>;

	explicit SynchronizedMap(const LockClass& lockClass) : monitor_(lockClass), store_(&comparator_) {}
	SynchronizedMap(const SynchronizedMap&) = delete;
	SynchronizedMap& operator=(const SynchronizedMap&) = delete;

	int32_t size() const { return size_.load(std::memory_order_acquire); }
	/** lock-free */
	bool isEmpty() const noexcept { return size_.load(std::memory_order_acquire) == 0; }
	bool containsKey(const Key& key) const {
		detail::ShimLock lock(monitor_);
		return Store::found(locate(key));
	}
	/** Java equals on values */
	bool containsValue(const Value& value) const {
		detail::ShimLock lock(monitor_);
		detail::BusyScope busy(busy_);
		bool found = false;
		store_.forEach([&](const K&, V& stored) {
			if (!found && detail::javaEquals(stored, value))
				found = true;
		});
		return found;
	}
	Nullable<V> get(const Key& key) const {
		detail::readBarrier<V>();
		detail::ShimLock lock(monitor_);
		Position position = locate(key);
		return Store::found(position) ? Nullable<V>(store_.value(position)) : Nullable<V>{};
	}
	Value getOrDefault(const Key& key, const Value& defaultValue) const {
		detail::readBarrier<V>();
		detail::ShimLock lock(monitor_);
		Position position = locate(key);
		return Store::found(position) ? Value(store_.value(position)) : defaultValue;
	}
	/** @return the previous value */
	Nullable<V> put(K key, V value) {
		detail::readBarrier<V>();
		detail::ShimLock lock(monitor_);
		detail::checkNotBusy(busy_);
		decltype(auto) borrowedKey = detail::borrowOf(key);
		detail::checkNotRecursiveUpdate(this, &borrowedKey);
		Position position = locate(borrowedKey);
		if (Store::found(position)) {
			Nullable<V> previous(store_.value(position));
			store_.value(position) = std::move(value);
			return previous;
		}
		store_.insert(position, std::move(key), std::move(value));
		changed();
		return Nullable<V>{};
	}
	/** Java: a key mapped to null counts as absent. @return the current (non-null) value, or null if `value` was stored */
	Nullable<V> putIfAbsent(K key, V value) {
		detail::readBarrier<V>();
		detail::ShimLock lock(monitor_);
		detail::checkNotBusy(busy_);
		decltype(auto) borrowedKey = detail::borrowOf(key);
		Position position = locate(borrowedKey);
		if (Store::found(position) && !detail::isNull(store_.value(position)))
			return Nullable<V>(store_.value(position)); // present: no update
		detail::checkNotRecursiveUpdate(this, &borrowedKey);
		if (Store::found(position)) {
			store_.value(position) = std::move(value);
			return Nullable<V>{};
		}
		store_.insert(position, std::move(key), std::move(value));
		changed();
		return Nullable<V>{};
	}
	/** Copies the mappings of another shim (via snapshot()) or of a std:: map. */
	template <class Map>
	void putAll(const Map& other) {
		if constexpr (requires { other.snapshot(); }) {
			for (const auto& entry : other.snapshot())
				put(K(entry.key), V(entry.value));
		} else {
			for (const auto& [key, value] : other)
				put(K(key), V(value));
		}
	}
	Nullable<V> remove(const Key& key) {
		detail::readBarrier<V>();
		detail::ShimLock lock(monitor_);
		detail::checkNotBusy(busy_);
		detail::checkNotRecursiveUpdate(this, &key);
		Position position = locate(key);
		if (!Store::found(position))
			return Nullable<V>{};
		Nullable<V> previous(store_.value(position));
		store_.erase(position);
		changed();
		return previous;
	}
	/** removes only if mapped to a Java-equal value */
	bool remove(const Key& key, const Value& value) {
		detail::ShimLock lock(monitor_);
		detail::checkNotBusy(busy_);
		detail::checkNotRecursiveUpdate(this, &key);
		Position position = locate(key);
		if (!Store::found(position) || !equalValue(store_.value(position), value))
			return false;
		store_.erase(position);
		changed();
		return true;
	}
	Nullable<V> replace(const Key& key, V value) {
		detail::readBarrier<V>();
		detail::ShimLock lock(monitor_);
		detail::checkNotBusy(busy_);
		detail::checkNotRecursiveUpdate(this, &key);
		Position position = locate(key);
		if (!Store::found(position))
			return Nullable<V>{};
		Nullable<V> previous(store_.value(position));
		store_.value(position) = std::move(value);
		return previous;
	}
	bool replace(const Key& key, const Value& oldValue, V newValue) {
		detail::ShimLock lock(monitor_);
		detail::checkNotBusy(busy_);
		detail::checkNotRecursiveUpdate(this, &key);
		Position position = locate(key);
		if (!Store::found(position) || !equalValue(store_.value(position), oldValue))
			return false;
		store_.value(position) = std::move(newValue);
		return true;
	}
	void clear() {
		detail::ShimLock lock(monitor_);
		detail::checkNotBusy(busy_);
		store_.clear();
		changed();
	}

	template <class F>
	Nullable<V> compute(K key, F&& remapping) {
		detail::readBarrier<V>();
		detail::ShimLock lock(monitor_);
		detail::checkNotBusy(busy_);
		decltype(auto) borrowedKey = detail::borrowOf(key);
		detail::checkNotRecursiveUpdate(this, &borrowedKey);
		Nullable<V> old;
		if (Position position = locate(borrowedKey); Store::found(position))
			old = Nullable<V>(store_.value(position));
		std::optional<V> result;
		{
			detail::ComputeFrameScope frame(this, &borrowedKey, &sameKey);
			result = detail::toStored<V>(detail::invokeRemapping<K, V>(remapping, borrowedKey, old));
		}
		return store(std::move(key), borrowedKey, std::move(result));
	}
	template <class F>
	Nullable<V> computeIfAbsent(K key, F&& mapping) {
		detail::readBarrier<V>();
		detail::ShimLock lock(monitor_);
		detail::checkNotBusy(busy_);
		decltype(auto) borrowedKey = detail::borrowOf(key);
		if (Position position = locate(borrowedKey); Store::found(position) && !detail::isNull(store_.value(position)))
			return Nullable<V>(store_.value(position)); // present: no update, no recursion error
		detail::checkNotRecursiveUpdate(this, &borrowedKey);
		std::optional<V> result;
		{
			detail::ComputeFrameScope frame(this, &borrowedKey, &sameKey);
			result = detail::toStored<V>(detail::invokeMapping<K>(mapping, borrowedKey));
		}
		if (!result)
			return Nullable<V>{};
		return store(std::move(key), borrowedKey, std::move(result));
	}
	template <class F>
	Nullable<V> computeIfPresent(K key, F&& remapping) {
		detail::readBarrier<V>();
		detail::ShimLock lock(monitor_);
		detail::checkNotBusy(busy_);
		decltype(auto) borrowedKey = detail::borrowOf(key);
		detail::checkNotRecursiveUpdate(this, &borrowedKey);
		Value old{};
		if (Position position = locate(borrowedKey); Store::found(position) && !detail::isNull(store_.value(position)))
			old = Value(store_.value(position));
		else
			return Nullable<V>{};
		std::optional<V> result;
		{
			detail::ComputeFrameScope frame(this, &borrowedKey, &sameKey);
			result = detail::toStored<V>(detail::invokePresentRemapping<K, V>(remapping, borrowedKey, old));
		}
		return store(std::move(key), borrowedKey, std::move(result));
	}
	template <class F>
	Nullable<V> merge(K key, V value, F&& remapping) {
		if (detail::isNull(value))
			detail::throwNullElement("merge: null value");
		detail::readBarrier<V>();
		detail::ShimLock lock(monitor_);
		detail::checkNotBusy(busy_);
		decltype(auto) borrowedKey = detail::borrowOf(key);
		detail::checkNotRecursiveUpdate(this, &borrowedKey);
		Position position = locate(borrowedKey);
		if (!Store::found(position) || detail::isNull(store_.value(position)))
			return store(std::move(key), borrowedKey, std::optional<V>(std::move(value)));
		Value old(store_.value(position));
		std::optional<V> result;
		{
			detail::ComputeFrameScope frame(this, &borrowedKey, &sameKey);
			decltype(auto) given = detail::borrowOf(value);
			result = detail::toStored<V>(std::invoke(remapping, std::as_const(old), std::as_const(given)));
		}
		return store(std::move(key), borrowedKey, std::move(result));
	}

	/** Removes entries matching (const Key&, Value), evaluated under the Monitor on a snapshot. @return true if any was removed */
	template <class Predicate>
		requires std::predicate<Predicate&, const Key&, const Value&>
	bool removeIf(Predicate&& predicate) {
		detail::readBarrier<K>();
		detail::readBarrier<V>();
		detail::ShimLock lock(monitor_);
		detail::checkNotBusy(busy_);
		bool any = false;
		for (const Entry& entry : entriesLocked()) {
			if (std::invoke(predicate, entry.key, entry.value))
				any |= removeIfUnchangedLocked(entry.key, entry.value);
		}
		return any;
	}
	/** Java replaceAll((k, v) -> newValue) under the Monitor; the function returns V, Borrowed<V> or anything convertible to V. */
	template <class F>
	void replaceAll(F&& function) {
		detail::readBarrier<K>();
		detail::readBarrier<V>();
		detail::ShimLock lock(monitor_);
		detail::checkNotBusy(busy_);
		for (const Entry& entry : entriesLocked()) {
			if (!Store::found(locate(entry.key)))
				continue;
			V replacement = detail::toElement<V>(std::invoke(function, entry.key, entry.value));
			if (Position position = locate(entry.key); Store::found(position))
				store_.value(position) = std::move(replacement);
		}
	}
	/** Java forEach((k, v) -> ...) over a snapshot (the action runs without the Monitor). */
	template <class Action>
		requires std::invocable<Action&, const Key&, const Value&>
	void forEach(Action&& action) const {
		for (const Entry& entry : snapshot())
			std::invoke(action, entry.key, entry.value);
	}

	SnapshotView<Key> keySet() const {
		auto* self = const_cast<SynchronizedMap*>(this); // views write through to the (non-const) map they came from, like Java
		std::vector<Key> keys;
		for (const Entry& entry : snapshot())
			keys.push_back(entry.key);
		return SnapshotView<Key>(
			std::move(keys), [self](const Key& key) { return self->removeKey(key); },
			[self](const std::function<bool(const Key&)>& predicate) {
				return self->removeIf([&predicate](const Key& key, const Value&) { return predicate(key); });
			});
	}
	SnapshotView<Value> values() const {
		auto* self = const_cast<SynchronizedMap*>(this);
		std::vector<Entry> entries = snapshot();
		std::vector<Value> values;
		values.reserve(entries.size());
		for (const Entry& entry : entries)
			values.push_back(entry.value);
		auto keys = std::make_shared<std::vector<Entry>>(std::move(entries));
		return SnapshotView<Value>(
			std::move(values), [self](const Value& value) { return self->removeFirstValue(value); },
			[self, keys](const Value&, size_t index) {
				const Entry& entry = (*keys)[index];
				return self->removeIfUnchanged(entry.key, entry.value);
			},
			[self](const std::function<bool(const Value&)>& predicate) {
				return self->removeIf([&predicate](const Key&, const Value& value) { return predicate(value); });
			});
	}
	SnapshotView<Entry> entrySet() const {
		auto* self = const_cast<SynchronizedMap*>(this);
		return SnapshotView<Entry>(
			snapshot(), [self](const Entry& entry) { return self->remove(entry.key, entry.value); },
			[self](const Entry& entry, size_t) { return self->removeIfUnchanged(entry.key, entry.value); },
			[self](const std::function<bool(const Entry&)>& predicate) {
				return self->removeIf([&predicate](const Key& key, const Value& value) { return predicate(Entry{key, value}); });
			});
	}
	std::vector<Entry> snapshot() const {
		detail::readBarrier<K>();
		detail::readBarrier<V>();
		detail::ShimLock lock(monitor_);
		return entriesLocked();
	}

	/** `synchronized (map)` */
	Monitor& monitor() const noexcept { return monitor_; }

protected:
	Position locate(const Key& key) const {
		detail::BusyScope busy(busy_);
		return store_.locate(key);
	}
	void changed() noexcept { size_.store(static_cast<int32_t>(store_.size()), std::memory_order_release); }
	static bool equalValue(const V& stored, const Value& value) { return detail::javaEquals(stored, value); }

	/** compute-family tail: stores or removes the result for `key` (re-located: the callback may have changed the map). */
	Nullable<V> store(K&& key, const Key& borrowedKey, std::optional<V> result) { // `borrowedKey` may refer to `key`: moved only on insert
		Position position = locate(borrowedKey);
		if (!result) {
			if (Store::found(position)) {
				store_.erase(position);
				changed();
			}
			return Nullable<V>{};
		}
		if (Store::found(position)) {
			V& stored = store_.value(position);
			stored = std::move(*result);
			return Nullable<V>(stored);
		}
		V& stored = store_.insert(position, std::move(key), std::move(*result));
		changed();
		return Nullable<V>(stored);
	}

	static bool sameKey(const void* container, const void* frameKey, const void* otherKey) {
		const auto* self = static_cast<const SynchronizedMap*>(container);
		const Key& a = *static_cast<const Key*>(frameKey);
		const Key& b = *static_cast<const Key*>(otherKey);
		if constexpr (SORTED_STORE)
			return detail::compareElements<K>(self->comparator_, a, b) == 0;
		else
			return JavaEquality<K>::equals(a, b);
	}

	std::vector<Entry> entriesLocked() const {
		std::vector<Entry> entries;
		entries.reserve(store_.size());
		store_.forEach([&entries](const K& key, V& value) { entries.push_back(Entry{Key(key), Value(value)}); });
		return entries;
	}

	bool removeKey(const Key& key) {
		detail::ShimLock lock(monitor_);
		detail::checkNotBusy(busy_);
		detail::checkNotRecursiveUpdate(this, &key);
		Position position = locate(key);
		if (!Store::found(position))
			return false;
		store_.erase(position);
		changed();
		return true;
	}
	bool removeFirstValue(const Value& value) {
		detail::readBarrier<K>();
		detail::ShimLock lock(monitor_);
		detail::checkNotBusy(busy_);
		for (const Entry& entry : entriesLocked()) {
			if (JavaEquality<V>::equals(entry.value, value))
				return removeIfUnchangedLocked(entry.key, entry.value);
		}
		return false;
	}
	bool removeIfUnchanged(const Key& key, const Value& value) {
		detail::ShimLock lock(monitor_);
		detail::checkNotBusy(busy_);
		return removeIfUnchangedLocked(key, value);
	}
	bool removeIfUnchangedLocked(const Key& key, const Value& value) {
		detail::checkNotRecursiveUpdate(this, &key);
		Position position = locate(key);
		if (!Store::found(position) || !detail::identical(detail::borrowOf(store_.value(position)), value))
			return false;
		store_.erase(position);
		changed();
		return true;
	}

	mutable Monitor monitor_;
	std::atomic<int32_t> size_{0};
	/** > 0 while the map runs key equals/hashCode or a comparator on its storage (under the Monitor) */
	mutable int32_t busy_ = 0;
	/** TreeMap comparator (empty: natural ordering); set by TreeMap's constructor before first use */
	JavaComparator<K> comparator_;
	mutable Store store_;
};

/** Java: java.util.HashMap */
template <class K, class V>
class HashMap : public SynchronizedMap<K, V, MapOrder::HASH> {
public:
	HashMap() : SynchronizedMap<K, V, MapOrder::HASH>(LockClass::named("HashMap")) {}
	explicit HashMap(const LockClass& lockClass) : SynchronizedMap<K, V, MapOrder::HASH>(lockClass) {}
	/** Java `new HashMap<>(initialCapacity)`: the capacity is a hint */
	explicit HashMap(int32_t) : HashMap() {}
};

/** Java: java.util.LinkedHashMap (insertion order; access order is not used by the game server) */
template <class K, class V>
class LinkedHashMap : public SynchronizedMap<K, V, MapOrder::INSERTION> {
public:
	LinkedHashMap() : SynchronizedMap<K, V, MapOrder::INSERTION>(LockClass::named("LinkedHashMap")) {}
	explicit LinkedHashMap(const LockClass& lockClass) : SynchronizedMap<K, V, MapOrder::INSERTION>(lockClass) {}
	explicit LinkedHashMap(int32_t) : LinkedHashMap() {}
};

/**
 * Java: java.util.TreeMap (also Collections.synchronizedSortedMap(new TreeMap<>()), Equipment.java:48). Keys ordered by the comparator given at
 * construction (Java Comparator returning int, called under the Monitor) or by natural ordering (JavaOrdering<K>; a key type without natural
 * ordering and no comparator throws ClassCastException on first comparison, like Java).
 * Navigation methods return Nullable keys / std::optional entries where Java returns null; firstKey/lastKey throw NoSuchElementException.
 */
template <class K, class V>
class TreeMap : public SynchronizedMap<K, V, MapOrder::SORTED> {
	using Base = SynchronizedMap<K, V, MapOrder::SORTED>;

public:
	using Key = Borrowed<K>;
	using Value = Borrowed<V>;
	using Entry = MapEntry<K, V>;

	TreeMap() : Base(LockClass::named("TreeMap")) {}
	explicit TreeMap(const LockClass& lockClass) : Base(lockClass) {}
	explicit TreeMap(JavaComparator<K> comparator) : TreeMap() { this->comparator_ = std::move(comparator); }
	TreeMap(const LockClass& lockClass, JavaComparator<K> comparator) : TreeMap(lockClass) { this->comparator_ = std::move(comparator); }

	/** @throws NoSuchElementException if empty */
	Key firstKey() const { return endKey(true); }
	/** @throws NoSuchElementException if empty */
	Key lastKey() const { return endKey(false); }
	std::optional<Entry> firstEntry() const { return endEntry(true, false); }
	std::optional<Entry> lastEntry() const { return endEntry(false, false); }
	std::optional<Entry> pollFirstEntry() { return endEntry(true, true); }
	std::optional<Entry> pollLastEntry() { return endEntry(false, true); }
	Nullable<K> floorKey(const Key& key) const { return navigateKey(key, Navigation::FLOOR); }
	Nullable<K> ceilingKey(const Key& key) const { return navigateKey(key, Navigation::CEILING); }
	Nullable<K> lowerKey(const Key& key) const { return navigateKey(key, Navigation::LOWER); }
	Nullable<K> higherKey(const Key& key) const { return navigateKey(key, Navigation::HIGHER); }
	std::optional<Entry> floorEntry(const Key& key) const { return navigateEntry(key, Navigation::FLOOR); }
	std::optional<Entry> ceilingEntry(const Key& key) const { return navigateEntry(key, Navigation::CEILING); }
	std::optional<Entry> lowerEntry(const Key& key) const { return navigateEntry(key, Navigation::LOWER); }
	std::optional<Entry> higherEntry(const Key& key) const { return navigateEntry(key, Navigation::HIGHER); }
	/** snapshot of entries with key < toKey (or <= if inclusive), in order */
	std::vector<Entry> headMap(const Key& toKey, bool inclusive = false) const {
		detail::readBarrier<K>();
		detail::readBarrier<V>();
		detail::ShimLock lock(this->monitor_);
		auto& map = this->store_.map();
		auto last = bound(toKey, inclusive);
		std::vector<Entry> entries;
		for (auto it = map.begin(); it != last; ++it)
			entries.push_back(Entry{Key(it->first), Value(it->second)});
		return entries;
	}
	/** snapshot of entries with key >= fromKey (or > if !inclusive), in order */
	std::vector<Entry> tailMap(const Key& fromKey, bool inclusive = true) const {
		detail::readBarrier<K>();
		detail::readBarrier<V>();
		detail::ShimLock lock(this->monitor_);
		auto& map = this->store_.map();
		std::vector<Entry> entries;
		for (auto it = bound(fromKey, !inclusive); it != map.end(); ++it)
			entries.push_back(Entry{Key(it->first), Value(it->second)});
		return entries;
	}
	/** snapshot in descending order */
	std::vector<Entry> descendingMap() const {
		std::vector<Entry> entries = this->snapshot();
		std::reverse(entries.begin(), entries.end());
		return entries;
	}

private:
	enum class Navigation : uint8_t { FLOOR, CEILING, LOWER, HIGHER };

	/** first position with key > `key` (upper) or >= `key` (lower) */
	auto bound(const Key& key, bool upper) const {
		detail::BusyScope busy(this->busy_);
		auto& map = this->store_.map();
		return upper ? map.upper_bound(key) : map.lower_bound(key);
	}
	/** iterator of the navigation result or end() */
	auto navigate(const Key& key, Navigation navigation) const {
		auto& map = this->store_.map();
		switch (navigation) {
			case Navigation::FLOOR: {
				auto it = bound(key, true);
				return it == map.begin() ? map.end() : std::prev(it);
			}
			case Navigation::CEILING:
				return bound(key, false);
			case Navigation::LOWER: {
				auto it = bound(key, false);
				return it == map.begin() ? map.end() : std::prev(it);
			}
			case Navigation::HIGHER:
				break;
		}
		return bound(key, true);
	}
	Nullable<K> navigateKey(const Key& key, Navigation navigation) const {
		detail::readBarrier<K>();
		detail::ShimLock lock(this->monitor_);
		auto it = navigate(key, navigation);
		return it == this->store_.map().end() ? Nullable<K>{} : Nullable<K>(it->first);
	}
	std::optional<Entry> navigateEntry(const Key& key, Navigation navigation) const {
		detail::readBarrier<K>();
		detail::readBarrier<V>();
		detail::ShimLock lock(this->monitor_);
		auto it = navigate(key, navigation);
		if (it == this->store_.map().end())
			return std::nullopt;
		return Entry{Key(it->first), Value(it->second)};
	}
	Key endKey(bool first) const {
		detail::readBarrier<K>();
		detail::ShimLock lock(this->monitor_);
		auto& map = this->store_.map();
		if (map.empty())
			detail::throwNoSuchElement("TreeMap is empty");
		return Key(first ? map.begin()->first : std::prev(map.end())->first);
	}
	std::optional<Entry> endEntry(bool first, bool poll) const {
		detail::readBarrier<K>();
		detail::readBarrier<V>();
		detail::ShimLock lock(this->monitor_);
		auto& map = this->store_.map();
		if (map.empty())
			return std::nullopt;
		auto it = first ? map.begin() : std::prev(map.end());
		Entry entry{Key(it->first), Value(it->second)};
		if (poll) {
			detail::checkNotBusy(this->busy_);
			detail::checkNotRecursiveUpdate(static_cast<const Base*>(this), &entry.key);
			map.erase(it);
			const_cast<TreeMap*>(this)->changed();
		}
		return entry;
	}
};

/** Java: java.util.EnumMap (K must be an enum; iteration in ordinal order). */
template <class K, class V>
	requires std::is_enum_v<K>
class EnumMap : public SynchronizedMap<K, V, MapOrder::ENUM_ORDINAL> {
public:
	EnumMap() : SynchronizedMap<K, V, MapOrder::ENUM_ORDINAL>(LockClass::named("EnumMap")) {}
	explicit EnumMap(const LockClass& lockClass) : SynchronizedMap<K, V, MapOrder::ENUM_ORDINAL>(lockClass) {}
};

} // namespace aion::gameserver::runtime
