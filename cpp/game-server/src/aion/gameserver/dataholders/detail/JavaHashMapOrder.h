#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>
#include <utility>
#include <vector>

namespace aion::gameserver::dataholders::detail {

/** C++ only: logs a warning (once per process) that a JavaHashMapOrder bucket would be a tree in Java and counts it. Thread-safe. */
void noteJavaTreeifiedBucket() noexcept;

/** C++ only: the number of noteJavaTreeifiedBucket calls in this process (tests assert 0 for the real data). Thread-safe. */
int32_t javaTreeifiedBucketCount() noexcept;

/** Java Integer.hashCode() */
constexpr int32_t javaHashCode(int32_t key) noexcept {
	return key;
}

/** Java String.hashCode() of the string whose UTF-8 form is `utf8`: s[0]*31^(n-1) + ... + s[n-1] over its UTF-16 code units, wrapping */
int32_t javaHashCode(std::string_view utf8);

/**
 * C++ only: the iteration order of a Java `new HashMap<K, V>()` filled by `put(key, value)` calls in the given order (keySet(), values() and
 * entrySet() of the holders' @XmlTransient index maps). Holders compute it once in afterUnmarshal, so the C++ getters that return Java's
 * `map.values()` (or search it for the first match) see Java's order.
 * <p>
 * Models HashMap.putVal (a new key is appended to its bucket; a present key keeps its position and gets the new value), HashMap.computeIfAbsent
 * (a new key is inserted at the head of its bucket; the table is resized before the lookup while the size exceeds the threshold), removeNode,
 * HashMap.hash (the key's hashCode with its high bits spread into the low bits), resize (the table starts at 16 buckets and doubles; putVal
 * resizes when the size exceeds 0.75 of the capacity; splitting a bucket keeps the relative order) and treeifyBin's resize of tables below 64
 * buckets. Not modelled: a bucket that Java turns into a red-black tree (8 or more keys in a table of 64 or more buckets), whose iteration order
 * differs; the entries of such a bucket keep the list order (docs/deviations/P4-09.md). Every such bucket is reported once per map through
 * noteJavaTreeifiedBucket (a warning and javaTreeifiedBucketCount), so a data change that reaches it is noticed.
 * <p>
 * Confinement: a local of the loading thread.
 */
template <class K, class V>
class JavaHashMapOrder {
public:
	/** Java map.put(key, value) with key.hashCode() == hash */
	void put(const K& key, V value, int32_t hash) {
		if (table.empty())
			resize();
		Bucket& bucket = table[index(hash, table.size())];
		for (Entry& entry : bucket) {
			if (entry.key == key) {
				entry.value = std::move(value);
				return;
			}
		}
		const size_t previousLength = bucket.size();
		bucket.push_back(Entry{key, std::move(value), hash});
		if (previousLength >= TREEIFY_THRESHOLD) // putVal: binCount >= TREEIFY_THRESHOLD - 1 with binCount = previous length - 1
			treeifyBin();
		if (++count > threshold())
			resize();
	}

	/**
	 * Java map.computeIfAbsent(key, k -> value) with a non-null value: true if the key was present (its value is kept). A new key is inserted at
	 * the head of its bucket, and the table is resized first while the size exceeds the threshold (even if the key is present).
	 */
	bool computeIfAbsent(const K& key, V value, int32_t hash) {
		if (table.empty() || count > threshold())
			resize();
		Bucket& bucket = table[index(hash, table.size())];
		for (const Entry& entry : bucket) {
			if (entry.key == key)
				return true;
		}
		const size_t previousLength = bucket.size(); // computeIfAbsent: binCount >= TREEIFY_THRESHOLD - 1 with binCount = previous length
		bucket.insert(bucket.begin(), Entry{key, std::move(value), hash});
		if (previousLength + 1 >= TREEIFY_THRESHOLD)
			treeifyBin();
		++count;
		return false;
	}

	/** Java map.remove(key): true if the key was present (no resize: a HashMap never shrinks) */
	bool remove(const K& key, int32_t hash) {
		if (table.empty())
			return false;
		if (std::erase_if(table[index(hash, table.size())], [&key](const Entry& entry) { return entry.key == key; }) == 0)
			return false;
		--count;
		return true;
	}

	/** Java map.putIfAbsent(key, value) != null: true if the key was present (its value is kept) */
	bool putIfAbsent(const K& key, V value, int32_t hash) {
		if (contains(key, hash))
			return true;
		put(key, std::move(value), hash);
		return false;
	}

	bool contains(const K& key, int32_t hash) const {
		if (table.empty())
			return false;
		for (const Entry& entry : table[index(hash, table.size())]) {
			if (entry.key == key)
				return true;
		}
		return false;
	}

	size_t size() const noexcept { return count; }

	/** Java map.values() in iteration order */
	std::vector<V> values() const {
		std::vector<V> result;
		result.reserve(count);
		for (const Bucket& bucket : table) {
			for (const Entry& entry : bucket)
				result.push_back(entry.value);
		}
		return result;
	}

	/** Java map.entrySet() in iteration order */
	std::vector<std::pair<K, V>> entries() const {
		std::vector<std::pair<K, V>> result;
		result.reserve(count);
		for (const Bucket& bucket : table) {
			for (const Entry& entry : bucket)
				result.emplace_back(entry.key, entry.value);
		}
		return result;
	}

	/** Java map.keySet() in iteration order */
	std::vector<K> keys() const {
		std::vector<K> result;
		result.reserve(count);
		for (const Bucket& bucket : table) {
			for (const Entry& entry : bucket)
				result.push_back(entry.key);
		}
		return result;
	}

private:
	static constexpr size_t TREEIFY_THRESHOLD = 8;
	static constexpr size_t MIN_TREEIFY_CAPACITY = 64;

	struct Entry {
		K key;
		V value;
		int32_t hash;
	};
	using Bucket = std::vector<Entry>;

	size_t threshold() const noexcept { return table.size() / 4 * 3; }

	/** treeifyBin: tables below 64 buckets are resized; a larger table would get a tree bucket (reported, not modelled) */
	void treeifyBin() {
		if (table.size() < MIN_TREEIFY_CAPACITY) {
			resize();
		} else if (!treeifiedReported) {
			treeifiedReported = true;
			noteJavaTreeifiedBucket();
		}
	}

	/** Java HashMap.hash(key) & (n - 1) */
	static size_t index(int32_t hash, size_t tableSize) noexcept {
		const uint32_t h = static_cast<uint32_t>(hash);
		return (h ^ (h >> 16)) & (tableSize - 1);
	}

	void resize() {
		const size_t newCapacity = table.empty() ? 16 : table.size() * 2;
		std::vector<Bucket> newTable(newCapacity);
		for (Bucket& bucket : table) {
			for (Entry& entry : bucket)
				newTable[index(entry.hash, newCapacity)].push_back(std::move(entry));
		}
		table = std::move(newTable);
	}

	std::vector<Bucket> table;
	size_t count = 0;
	bool treeifiedReported = false;
};

/** C++ only: a LinkedMap (or any map with put) whose iteration order is the Java HashMap iteration order of `order` */
template <class Map, class K, class V>
Map toLinkedMap(const JavaHashMapOrder<K, V>& order) {
	Map map;
	for (auto& [key, value] : order.entries())
		map.put(key, value);
	return map;
}

/** Java `new HashMap<Integer, V>()` filled by `put` in order, then `values()` */
template <class V>
std::vector<V> javaHashMapValues(const std::vector<std::pair<int32_t, V>>& puts) {
	JavaHashMapOrder<int32_t, V> order;
	for (const auto& [key, value] : puts)
		order.put(key, value, javaHashCode(key));
	return order.values();
}

} // namespace aion::gameserver::dataholders::detail
