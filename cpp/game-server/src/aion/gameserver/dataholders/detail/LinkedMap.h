#pragma once

#include <cstddef>
#include <functional>
#include <unordered_map>
#include <utility>
#include <vector>

namespace aion::gameserver::dataholders::detail {

/**
 * C++ only: the read side of a Java LinkedHashMap built once by a holder hook: an index for get() plus the entries in insertion order for
 * iteration (a put of a present key replaces the value and keeps the position, as LinkedHashMap.put does).
 * <p>
 * Confinement: built by the loading thread before the holder is published; afterwards read-only, so readers on any thread need no lock.
 */
template <class K, class V, class Hash = std::hash<K>>
class LinkedMap {
public:
	using Entry = std::pair<K, V>;

	/** Java put(key, value): @return true if the key was new */
	bool put(const K& key, V value) {
		auto [it, inserted] = index.try_emplace(key, entries.size());
		if (inserted)
			entries.emplace_back(key, std::move(value));
		else
			entries[it->second].second = std::move(value);
		return inserted;
	}

	/** Java putIfAbsent(key, value) != null: @return true if the key was present (its value is kept) */
	bool putIfAbsent(const K& key, V value) {
		auto [it, inserted] = index.try_emplace(key, entries.size());
		if (inserted)
			entries.emplace_back(key, std::move(value));
		return !inserted;
	}

	/** Java get(key): @return the value, nullptr (Java null) if the key is absent */
	const V* get(const K& key) const {
		auto it = index.find(key);
		return it != index.end() ? &entries[it->second].second : nullptr;
	}

	bool containsKey(const K& key) const { return index.contains(key); }

	void clear() {
		index.clear();
		entries.clear();
	}

	size_t size() const noexcept { return entries.size(); }
	bool empty() const noexcept { return entries.empty(); }

	/** the entries in insertion order */
	const std::vector<Entry>& entrySet() const noexcept { return entries; }
	typename std::vector<Entry>::const_iterator begin() const noexcept { return entries.begin(); }
	typename std::vector<Entry>::const_iterator end() const noexcept { return entries.end(); }

private:
	std::unordered_map<K, size_t, Hash> index;
	std::vector<Entry> entries;
};

} // namespace aion::gameserver::dataholders::detail
