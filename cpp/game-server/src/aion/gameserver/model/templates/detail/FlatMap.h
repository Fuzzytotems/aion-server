#pragma once

#include <algorithm>
#include <utility>
#include <vector>

namespace aion::gameserver::model::templates::detail {

/**
 * A map kept as a vector of (key, value) pairs sorted by key, for the C++-only index members of bound templates.
 * <p>
 * Bound templates live in vectors that the binder reserves and fills in place, and `std::vector` moves its elements only when their move
 * constructor is noexcept (otherwise it copies, which does not compile for templates owning `unique_ptr` members). The MSVC `std::map` and
 * `std::unordered_map` move constructors are not noexcept, a vector's is, so index members use this map. Lookups are binary searches; the
 * templates' indexes are filled once by their hooks.
 */
template <class K, class V>
class FlatMap {
public:
	using Entry = std::pair<K, V>;

	/** Java Map.put: inserts or replaces the value of the key; @return true if the key was new */
	bool insertOrAssign(const K& key, V value) {
		auto position = lowerBound(key);
		if (position != entries.end() && position->first == key) {
			position->second = std::move(value);
			return false;
		}
		entries.emplace(position, key, std::move(value));
		return true;
	}

	/** Java Map.putIfAbsent: inserts the value unless the key is present; @return true if the key was new */
	bool tryEmplace(const K& key, V value) {
		auto position = lowerBound(key);
		if (position != entries.end() && position->first == key)
			return false;
		entries.emplace(position, key, std::move(value));
		return true;
	}

	/** @return the value of the key, nullptr if the key is absent */
	const V* find(const K& key) const {
		auto position = std::lower_bound(entries.begin(), entries.end(), key, [](const Entry& entry, const K& k) { return entry.first < k; });
		return position != entries.end() && position->first == key ? &position->second : nullptr;
	}

	/** @return the value of the key, nullptr if the key is absent */
	V* find(const K& key) {
		auto position = lowerBound(key);
		return position != entries.end() && position->first == key ? &position->second : nullptr;
	}

	/** @return the value of the key, inserted value-initialized if the key is absent */
	V& operator[](const K& key) {
		auto position = lowerBound(key);
		if (position == entries.end() || position->first != key)
			position = entries.emplace(position, key, V{});
		return position->second;
	}

	void clear() noexcept { entries.clear(); }

	size_t size() const noexcept { return entries.size(); }

	bool empty() const noexcept { return entries.empty(); }

	/** the entries in ascending key order */
	auto begin() const noexcept { return entries.begin(); }
	auto end() const noexcept { return entries.end(); }

private:
	std::vector<Entry> entries;

	typename std::vector<Entry>::iterator lowerBound(const K& key) {
		return std::lower_bound(entries.begin(), entries.end(), key, [](const Entry& entry, const K& k) { return entry.first < k; });
	}
};

} // namespace aion::gameserver::model::templates::detail
