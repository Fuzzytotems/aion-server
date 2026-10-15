#pragma once

#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

namespace aion::gameserver::model::templates::detail {

/**
 * Java: new HashMap<Integer, V>() filled with put(key, value) in the given order, then values(): the buckets in index order, each bucket in the
 * order of its linked list. Models putVal (a new key is appended to its bucket, a present key keeps its position and gets the new value), resize
 * (the table doubles when the size exceeds 0.75 of the capacity; splitting a bucket keeps the relative order) and treeifyBin's resize of tables
 * below 64 buckets, like SkillData's model. Not modelled: a bucket that would become a red-black tree (8 keys in one bucket of a table of 64 or
 * more buckets); template maps keyed by ids never come close.
 */
template <class V>
std::vector<V> javaIntegerHashMapValues(const std::vector<std::pair<int32_t, V>>& puts) {
	using Bucket = std::vector<std::pair<int32_t, V>>;
	auto spread = [](int32_t key) noexcept {
		const uint32_t h = static_cast<uint32_t>(key); // Integer.hashCode, then HashMap.hash
		return h ^ (h >> 16);
	};
	std::vector<Bucket> table;
	size_t size = 0;
	auto resize = [&table, &spread] {
		const size_t newCapacity = table.empty() ? 16 : table.size() * 2;
		std::vector<Bucket> newTable(newCapacity);
		for (const Bucket& bucket : table) {
			for (const auto& entry : bucket)
				newTable[spread(entry.first) & (newCapacity - 1)].push_back(entry);
		}
		table = std::move(newTable);
	};
	for (const auto& [key, value] : puts) {
		if (table.empty())
			resize();
		Bucket& bucket = table[spread(key) & (table.size() - 1)];
		bool present = false;
		for (auto& entry : bucket) {
			if (entry.first == key) {
				entry.second = value;
				present = true;
				break;
			}
		}
		if (present)
			continue;
		const size_t previousLength = bucket.size();
		bucket.emplace_back(key, value);
		if (previousLength >= 8 && table.size() < 64) // TREEIFY_THRESHOLD, MIN_TREEIFY_CAPACITY: treeifyBin resizes instead
			resize();
		if (++size > table.size() / 4 * 3)
			resize();
	}
	std::vector<V> values;
	values.reserve(size);
	for (const Bucket& bucket : table) {
		for (const auto& entry : bucket)
			values.push_back(entry.second);
	}
	return values;
}

} // namespace aion::gameserver::model::templates::detail
