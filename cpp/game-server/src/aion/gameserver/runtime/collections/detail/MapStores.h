#pragma once

#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <unordered_map>
#include <utility>
#include <vector>

#include "aion/gameserver/runtime/collections/JavaEquals.h"
#include "aion/gameserver/runtime/collections/detail/ShimSupport.h"

/**
 * Storage of the plain (Monitor-guarded) map and set shims. Not thread-safe by itself: the owning shim holds its Monitor around every call.
 *
 * - HashedStore: Java equals/hashCode keys, insertion order (used for HashMap, whose order is unspecified, and LinkedHashMap). Entries live in a
 *   slot vector in insertion order (removed slots become holes, compacted when more than half are holes); a hash multimap indexes slot numbers.
 * - SortedStore: std::map ordered by a JavaComparator or natural ordering (TreeMap, TreeSet, EnumMap by ordinal).
 *
 * Both expose the same interface: locate(key) -> Position, found(Position), key/value(Position), insert(Position, K, V) -> V&, erase(Position),
 * forEach(fn(const K&, V&)), size(), clear(). A Position is invalidated by any insert/erase/clear.
 */
namespace aion::gameserver::runtime::detail {

/** Value type of set stores. */
struct Present {};

template <class K, class V>
class HashedStore {
public:
	using Key = Borrowed<K>;

	struct Position {
		size_t slot;
		size_t hash;
	};

	explicit HashedStore(const JavaComparator<K>*) noexcept {}

	Position locate(const Key& key) const {
		size_t hash = JavaEquality<K>::hash(key);
		auto [first, last] = index_.equal_range(hash);
		for (auto it = first; it != last; ++it) {
			const Slot& slot = slots_[it->second];
			if (JavaEquality<K>::equals(borrowOf(slot.entry->first), key))
				return {it->second, hash};
		}
		return {NOT_FOUND, hash};
	}
	static bool found(const Position& position) noexcept { return position.slot != NOT_FOUND; }
	K& key(const Position& position) noexcept { return slots_[position.slot].entry->first; }
	V& value(const Position& position) noexcept { return slots_[position.slot].entry->second; }

	V& insert(const Position& position, K key, V value) {
		slots_.push_back(Slot{position.hash, std::move(key), std::move(value)});
		try {
			index_.emplace(position.hash, slots_.size() - 1);
		} catch (...) {
			slots_.pop_back();
			throw;
		}
		++live_;
		return slots_.back().entry->second;
	}

	void erase(const Position& position) {
		auto [first, last] = index_.equal_range(position.hash);
		for (auto it = first; it != last; ++it) {
			if (it->second == position.slot) {
				index_.erase(it);
				break;
			}
		}
		slots_[position.slot].entry.reset();
		--live_;
		if (live_ == 0) {
			slots_.clear();
			index_.clear();
		} else if (slots_.size() > 16 && live_ < slots_.size() / 2) {
			compact();
		}
	}

	template <class F>
	void forEach(F&& function) {
		for (Slot& slot : slots_) {
			if (slot.entry)
				function(std::as_const(slot.entry->first), slot.entry->second);
		}
	}

	size_t size() const noexcept { return live_; }
	void clear() noexcept {
		slots_.clear();
		index_.clear();
		live_ = 0;
	}

private:
	static constexpr size_t NOT_FOUND = static_cast<size_t>(-1);

	struct Slot {
		std::optional<std::pair<K, V>> entry;
		size_t hash;

		Slot(size_t hash, K key, V value) : entry(std::in_place, std::move(key), std::move(value)), hash(hash) {}
	};

	/** Removes holes. Uses the stored hashes and moves entries (no user code runs, nothing can throw after the allocations). */
	void compact() {
		std::vector<Slot> slots;
		slots.reserve(live_);
		std::unordered_multimap<size_t, size_t> index;
		index.reserve(live_);
		size_t next = 0;
		for (const Slot& slot : slots_) {
			if (slot.entry)
				index.emplace(slot.hash, next++); // may throw bad_alloc: nothing was moved yet
		}
		for (Slot& slot : slots_) {
			if (slot.entry)
				slots.push_back(Slot{slot.hash, std::move(slot.entry->first), std::move(slot.entry->second)}); // reserved: no reallocation
		}
		slots_.swap(slots);
		index_.swap(index);
	}

	std::vector<Slot> slots_;
	std::unordered_multimap<size_t, size_t> index_;
	size_t live_ = 0;
};

template <class K, class V>
class SortedStore {
public:
	using Key = Borrowed<K>;

	struct Less {
		using is_transparent = void;
		const JavaComparator<K>* comparator;

		template <class A, class B>
		bool operator()(const A& a, const B& b) const { return compare(a, b) < 0; }

	private:
		int32_t compare(const Key& a, const Key& b) const { return compareElements<K>(*comparator, a, b); }
	};
	using Map = std::map<K, V, Less>;

	struct Position {
		typename Map::iterator it;
		bool present;
	};

	explicit SortedStore(const JavaComparator<K>* comparator) : map_(Less{comparator}) {}

	Position locate(const Key& key) {
		if (map_.empty())
			(void)compareElements<K>(*map_.key_comp().comparator, key, key); // Java TreeMap.put: compare(key, key) type check on an empty map
		auto it = map_.find(key);
		return {it, it != map_.end()};
	}
	static bool found(const Position& position) noexcept { return position.present; }
	const K& key(const Position& position) noexcept { return position.it->first; }
	V& value(const Position& position) noexcept { return position.it->second; }
	V& insert(const Position&, K key, V value) { return map_.emplace(std::move(key), std::move(value)).first->second; }
	void erase(const Position& position) { map_.erase(position.it); }

	template <class F>
	void forEach(F&& function) {
		for (auto& [key, value] : map_)
			function(key, value);
	}

	size_t size() const noexcept { return map_.size(); }
	void clear() noexcept { map_.clear(); }

	Map& map() noexcept { return map_; }

private:
	Map map_;
};

} // namespace aion::gameserver::runtime::detail
