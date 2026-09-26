#pragma once

#include <array>
#include <atomic>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <iterator>
#include <memory>
#include <mutex>
#include <optional>
#include <ranges>
#include <type_traits>
#include <typeinfo>
#include <utility>
#include <vector>

#include "aion/gameserver/runtime/base/YieldPoint.h"
#include "aion/gameserver/runtime/collections/Iterators.h"
#include "aion/gameserver/runtime/collections/JavaEquals.h"
#include "aion/gameserver/runtime/collections/detail/ShimSupport.h"
#include "aion/gameserver/runtime/lifetime/Reclaimer.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/TaskScope.h"
#include "aion/gameserver/runtime/sync/LockClass.h"
#include "aion/gameserver/runtime/sync/Monitor.h"
#include "aion/gameserver/runtime/sync/RankedMutex.h"

/**
 * AION_CHM_LOCKED_READS (design §17 risk mitigation): when defined to 1, readers take the stripe Monitor instead of reading the node tables
 * lock-free. Default 0 (lock-free reads). The switch selects the default of ConcurrentHashMap's third template parameter, so both variants can
 * coexist in one program without ODR problems (tests instantiate both).
 */
#if !defined(AION_CHM_LOCKED_READS)
#define AION_CHM_LOCKED_READS 0
#endif

namespace aion::gameserver::runtime {

namespace detail {

/** Murmur3 finalizer: spreads Java hashCodes (often small sequential ids) over all 64 bits. */
constexpr uint64_t spreadHash(size_t hash) noexcept {
	uint64_t x = static_cast<uint64_t>(hash);
	x ^= x >> 33;
	x *= 0xff51afd7ed558ccdULL;
	x ^= x >> 33;
	x *= 0xc4ceb9fe1a85ec53ULL;
	x ^= x >> 33;
	return x;
}

/** Immutable mapping node (only `next` changes while linked). Replacing a value links a new node and retires the old one. */
template <class K, class V>
struct ChmNode final : RetiredNode {
	ChmNode(uint64_t hash, K key, V value) : hash(hash), key(std::move(key)), value(std::move(value)) {}
	size_t retiredBytes() const noexcept override { return sizeof(ChmNode); }

	const uint64_t hash;
	const K key;
	const V value;
	std::atomic<ChmNode*> next{nullptr};
};

/** Bucket array of one stripe. Owns the nodes linked in it when it is destroyed (retired after a resize or clear, or map destruction). */
template <class K, class V>
struct ChmTable final : RetiredNode {
	explicit ChmTable(uint32_t bits) : bits(bits), buckets(new std::atomic<ChmNode<K, V>*>[size_t{1} << bits]()) {}
	~ChmTable() override {
		for (size_t i = 0; i < capacity(); ++i) {
			ChmNode<K, V>* node = buckets[i].load(std::memory_order_relaxed);
			while (node != nullptr) {
				ChmNode<K, V>* next = node->next.load(std::memory_order_relaxed);
				delete node;
				node = next;
			}
		}
	}
	size_t capacity() const noexcept { return size_t{1} << bits; }
	std::atomic<ChmNode<K, V>*>& bucket(uint64_t hash) const noexcept { return buckets[hash & (capacity() - 1)]; }
	size_t retiredBytes() const noexcept override { return sizeof(ChmTable) + capacity() * sizeof(void*); }

	const uint32_t bits;
	const std::unique_ptr<std::atomic<ChmNode<K, V>*>[]> buckets;
};

} // namespace detail

/** Projection of a ConcurrentHashMap view. */
enum class MapViewKind : uint8_t { KEYS, VALUES, ENTRIES };

/**
 * Java: java.util.concurrent.ConcurrentHashMap (design §3.3, §4.1, RR-1 fatal finding fix, RR-10, RR-17).
 *
 * Structure: STRIPES (16) stripes selected by the key's spread Java hash. Each stripe owns an epoch-reclaimed node table (retired with
 * Reclaimer::retireNode on resize or clear; removed and replaced nodes are retired individually) and a reentrant StripeMonitor (rank 0 game-level
 * Monitor, reported with the lock class passed to the constructor, `Owner::field#stripe`). The stripe array itself is allocated on the first
 * write, so an empty map costs a few bytes (KnownList and AggroList maps exist per creature).
 *
 * Readers - get, getOrDefault, containsKey, containsValue, iteration of keySet()/values()/entrySet(), forEach, snapshot - are lock-free: they
 * call TaskScope::ensurePublished() (read barrier) and read the node tables in place, weakly consistent (they observe some state between the
 * start and end of the call, never torn state), without copying (RR-17). Returned Ptrs stay valid until the task ends even if the mapping is
 * removed concurrently. isEmpty/size/mappingCount read an atomic counter. With the LockedReads template parameter (AION_CHM_LOCKED_READS)
 * point reads take the stripe Monitor and iteration copies each stripe under its Monitor.
 *
 * Writers - put, putIfAbsent, remove, replace, clear, compute*, merge, removeIf - take the stripe Monitor of the key. compute,
 * computeIfAbsent, computeIfPresent and merge run their callback WHILE HOLDING that StripeMonitor, as Java runs them under the bin lock: the
 * callback may take Monitors (lockdep-tracked edges stripe → monitor), call other operations on this map for other keys (including keys of
 * the same stripe, through reentrancy) or on other maps, schedule, cancel, send packets or call DAOs (conformance sites: CreatureGameStats.java
 * :73-84, PlayerContainer.java:51-54, GaleCycloneAI.java:31-35, LegionService.java:154-159, SpawnsData.java:83-96, Preview.java:211-226).
 * A write (put, remove, replace, compute, computeIfPresent, merge, and putIfAbsent/computeIfAbsent of a still absent key) of the SAME key from
 * inside its own callback throws IllegalStateException("Recursive update"), as Java does; reads of that key inside the callback (and
 * putIfAbsent/computeIfAbsent when it is present) see the old mapping. Key equals/hashCode must not modify the same stripe
 * (IllegalStateException); so must value equals called by remove(key, value)/replace(key, old, new). Resizing a stripe's node table takes the
 * map's CONTAINER_SLOT leaf mutex inside the stripe Monitor and runs no callbacks (it copies keys and values: their copy constructors must not
 * take Monitors, which holds for Ref, numbers, strings and template pointers).
 *
 * Nested writes across stripes of the same map (review finding, design §4.4 understated it): a compute-family callback that writes ANOTHER
 * key of the same map holds the first key's stripe Monitor while it takes the second key's stripe Monitor. Two such callbacks whose (outer,
 * nested) stripes cross (A: 3 -> 9, B: 9 -> 3) deadlock, with a probability of about 1/256 per concurrent pair, far more likely than with
 * Java's per-bin locks (PlayerContainer.updateCachedPlayerName: compute(oldName) -> put(newName); SpawnsData nested compute). All stripes share
 * one lock class, so lockdep reports only SAME_CLASS_NESTING, and the watchdog dumps the deadlock after the fact (D5). Code that nests writes
 * to the same map from concurrent threads must order them explicitly (e.g. a map-level Monitor around the nesting operation).
 *
 * Iterators (checked builds, C1): an iterator of keySet()/values()/entrySet() is stamped with the scope id of its creation; advancing or
 * dereferencing it after quiescentPoint() or in another scope throws IllegalStateException (it holds raw pointers into retired tables).
 *
 * Nulls: keys and values must not be null (NullPointerException, like Java). Callback results: see ComputeResult (null/empty removes).
 * Callback shapes: as SynchronizedMap (compute: (const Key&, Nullable<V>) or (Nullable<V>); computeIfAbsent: (const Key&) or ();
 * computeIfPresent: (const Key&, Value) or (Value); merge: (Value old, Value given)).
 * All operations except isEmpty/size/mappingCount need an active TaskScope (read barrier, C2).
 * Deviation 16: stripes are coarser than Java bins (more lock sharing between different keys); iteration never throws.
 * Thread-safety: every member is thread-safe. Not copyable or movable. Destroying the map frees its tables immediately: it must be unreachable
 * (owner reclaimed by the Reclaimer, or a static/stack map no other thread reads).
 */
template <class K, class V, bool LockedReads = AION_CHM_LOCKED_READS != 0>
class ConcurrentHashMap {
	using Node = detail::ChmNode<K, V>;
	using Table = detail::ChmTable<K, V>;

	struct Stripe {
		mutable Monitor monitor;
		std::atomic<Table*> table{nullptr};
		/** mappings in this stripe (under the Monitor) */
		int32_t size = 0;
		/** > 0 while the stripe runs key equals (under the Monitor) */
		int32_t busy = 0;
	};
	struct StripeArray {
		std::array<Stripe, 16> stripes;
		/**
		 * CONTAINER_SLOT leaf mutex held while a stripe of THIS map copies its node table (design §4.1). Writers already serialize per stripe
		 * through the stripe Monitor; the leaf mutex makes "no callbacks, no Monitors inside" a checked rule. Per map (bench finding: one
		 * process-wide resize mutex convoyed when many per-creature maps grew in the same tick).
		 */
		RankedMutex<LockRank::CONTAINER_SLOT> resizeMutex;
	};
	/** a located node and the atomic link pointing at it (bucket head or predecessor's next) */
	struct Location {
		std::atomic<Node*>* link;
		Node* node;
	};

public:
	using key_type = K;
	using mapped_type = V;
	using Key = Borrowed<K>;
	using Value = Borrowed<V>;
	using Entry = MapEntry<K, V>;

	static constexpr int32_t STRIPES = 16;
	static constexpr bool LOCKED_READS = LockedReads;

	/**
	 * Weakly consistent in-place view over keys, values or entries (lock-free iteration). Mutations write through: iterator().remove(),
	 * removeIf (per matching entry under its stripe Monitor), remove(element). Convertible to a std::vector snapshot.
	 */
	template <class E, MapViewKind Kind>
	class View {
	public:
		/** forward iterator over the live tables; each element is borrowed for the current task */
		class Iterator {
		public:
			using value_type = E;
			using difference_type = std::ptrdiff_t;

			Iterator() = default;
			const E& operator*() const {
				checkScope();
				return *current;
			}
			const E* operator->() const {
				checkScope();
				return &*current;
			}
			Iterator& operator++() {
				advance();
				return *this;
			}
			Iterator operator++(int) {
				Iterator old = *this;
				advance();
				return old;
			}
			friend bool operator==(const Iterator& it, std::default_sentinel_t) noexcept { return !it.current.has_value(); }

		private:
			friend class View;
			friend class ConcurrentHashMap;

			explicit Iterator(const ConcurrentHashMap& map) : map(&map) {
				TaskScope::ensurePublished();
#if AION_CHECKED
				scopeStamp = TaskScope::currentScopeId();
#endif
				stripes = map.stripes_.load(std::memory_order_acquire);
				stripeIndex = -1;
				nextStripe();
			}

			/**
			 * C1 for the iterator itself (checked builds): the lock-free iterator keeps raw table and node pointers that only the publication of
			 * the scope it was created in protects. After quiescentPoint() (or on another thread) advancing would read retired nodes, so it
			 * throws IllegalStateException like a stale Ptr, before touching them.
			 */
			void checkScope() const {
#if AION_CHECKED
				if (uint64_t currentScope = TaskScope::currentScopeId(); currentScope != scopeStamp) [[unlikely]]
					detail::throwStaleBorrow(typeid(ConcurrentHashMap), scopeStamp, currentScope);
#endif
			}
			void advance() {
				checkScope();
				if constexpr (LockedReads) {
					if (++bufferIndex < buffer.size()) {
						current = buffer[bufferIndex];
						return;
					}
					nextStripe();
				} else {
					AION_YIELD_POINT("ConcurrentHashMap::iterate:next");
					node = node->next.load(std::memory_order_acquire);
					settle();
				}
			}
			void nextStripe() {
				current.reset();
				if constexpr (LockedReads) {
					buffer.clear();
					bufferIndex = 0;
					while (buffer.empty()) {
						if (stripes == nullptr || ++stripeIndex >= STRIPES)
							return;
						Stripe& stripe = stripes->stripes[stripeIndex];
						detail::ShimLock lock(stripe.monitor, map->stripeLockClass_);
						if (Table* locked = stripe.table.load(std::memory_order_acquire)) {
							for (size_t i = 0; i < locked->capacity(); ++i) {
								for (Node* n = locked->buckets[i].load(std::memory_order_acquire); n != nullptr; n = n->next.load(std::memory_order_acquire))
									buffer.push_back(project(*n));
							}
						}
					}
					current = buffer[0];
				} else {
					node = nullptr;
					table = nullptr;
					bucketIndex = 0;
					settle();
				}
			}
			/** lock-free mode: moves to the first node at or after the current position */
			void settle() {
				while (node == nullptr) {
					if (table != nullptr && ++bucketIndex < table->capacity()) {
						AION_YIELD_POINT("ConcurrentHashMap::iterate:bucket");
						node = table->buckets[bucketIndex].load(std::memory_order_acquire);
						continue;
					}
					if (stripes == nullptr || ++stripeIndex >= STRIPES) {
						current.reset();
						return;
					}
					AION_YIELD_POINT("ConcurrentHashMap::iterate:table");
					table = stripes->stripes[stripeIndex].table.load(std::memory_order_acquire);
					if (table != nullptr) {
						bucketIndex = 0;
						node = table->buckets[0].load(std::memory_order_acquire);
					}
				}
				current = project(*node);
			}
			static E project(const Node& n) {
				if constexpr (Kind == MapViewKind::KEYS)
					return E(Key(n.key));
				else if constexpr (Kind == MapViewKind::VALUES)
					return E(Value(n.value));
				else
					return Entry{Key(n.key), Value(n.value)};
			}

			const ConcurrentHashMap* map = nullptr;
			StripeArray* stripes = nullptr;
			int32_t stripeIndex = STRIPES;
			std::optional<E> current;
#if AION_CHECKED
			/** TaskScope::currentScopeId() at creation (C1) */
			uint64_t scopeStamp = 0;
#endif
			// lock-free mode
			Table* table = nullptr;
			size_t bucketIndex = 0;
			Node* node = nullptr;
			// locked-reads mode
			std::vector<E> buffer;
			size_t bufferIndex = 0;
		};

		Iterator begin() const { return Iterator(*map); }
		std::default_sentinel_t end() const noexcept { return {}; }
		int32_t size() const { return map->size(); }
		bool isEmpty() const { return map->isEmpty(); }
		bool contains(const E& element) const {
			switch (Kind) {
				case MapViewKind::KEYS:
					return containsKeyElement(element);
				case MapViewKind::VALUES:
					return containsValueElement(element);
				case MapViewKind::ENTRIES:
					break;
			}
			return containsEntryElement(element);
		}
		std::vector<E> toVector() const {
			std::vector<E> elements;
			for (Iterator it = begin(); it != end(); ++it)
				elements.push_back(*it);
			return elements;
		}
		operator std::vector<E>() const { return toVector(); }
		/** JavaIterator over a snapshot; remove() removes the entry if its key is still mapped (keys) / mapped to the identical value */
		JavaIterator<E> iterator() const {
			auto* self = const_cast<ConcurrentHashMap*>(map);
			auto entries = std::make_shared<std::vector<Entry>>(map->snapshot());
			std::vector<E> elements;
			elements.reserve(entries->size());
			for (const Entry& entry : *entries)
				elements.push_back(projectSnapshot(entry));
			typename JavaIterator<E>::IndexedRemover remover = [self, entries](const E&, size_t index) {
				const Entry& entry = (*entries)[index];
				if (Kind == MapViewKind::KEYS)
					return self->removeKey(entry.key);
				return self->removeIfIdentical(entry.key, entry.value);
			};
			return JavaIterator<E>(std::move(elements), std::move(remover));
		}
		/** Java keySet()/values()/entrySet().removeIf: the predicate runs lock-free, each match is removed under its stripe Monitor. */
		template <class Predicate>
			requires std::predicate<Predicate&, const E&>
		bool removeIf(Predicate&& predicate) const {
			auto* self = const_cast<ConcurrentHashMap*>(map);
			return self->removeIf([&predicate](const Key& key, const Value& value) {
				if constexpr (Kind == MapViewKind::KEYS)
					return static_cast<bool>(std::invoke(predicate, E(key)));
				else if constexpr (Kind == MapViewKind::VALUES)
					return static_cast<bool>(std::invoke(predicate, E(value)));
				else
					return static_cast<bool>(std::invoke(predicate, E{key, value}));
			});
		}
		/** keys: remove(key); values: removes one entry with a Java-equal value; entries: remove(key, value) */
		bool remove(const E& element) const {
			auto* self = const_cast<ConcurrentHashMap*>(map);
			if constexpr (Kind == MapViewKind::KEYS) {
				return self->removeKey(element);
			} else if constexpr (Kind == MapViewKind::VALUES) {
				for (const Entry& entry : map->snapshot()) {
					if (JavaEquality<V>::equals(entry.value, element) && self->remove(entry.key, entry.value))
						return true;
				}
				return false;
			} else {
				return self->remove(element.key, element.value);
			}
		}

	private:
		friend class ConcurrentHashMap;
		explicit View(const ConcurrentHashMap& map) : map(&map) {}

		static E projectSnapshot(const Entry& entry) {
			if constexpr (Kind == MapViewKind::KEYS)
				return E(entry.key);
			else if constexpr (Kind == MapViewKind::VALUES)
				return E(entry.value);
			else
				return E(entry);
		}
		bool containsKeyElement(const E& element) const {
			if constexpr (Kind == MapViewKind::KEYS)
				return map->containsKey(element);
			else
				return false;
		}
		bool containsValueElement(const E& element) const {
			if constexpr (Kind == MapViewKind::VALUES)
				return map->containsValue(element);
			else
				return false;
		}
		bool containsEntryElement(const E& element) const {
			if constexpr (Kind == MapViewKind::ENTRIES) {
				Nullable<V> current = map->get(element.key);
				if (detail::isNull(current))
					return false;
				return JavaEquality<V>::equals(Value(*current), element.value);
			} else {
				return false;
			}
		}

		const ConcurrentHashMap* map;
	};

	using KeyView = View<Key, MapViewKind::KEYS>;
	using ValueView = View<Value, MapViewKind::VALUES>;
	using EntryView = View<Entry, MapViewKind::ENTRIES>;

	ConcurrentHashMap() : ConcurrentHashMap(LockClass::named("ConcurrentHashMap#stripe")) {}
	/** `ConcurrentHashMap<int32_t, Ref<KnownObject>> knownObjects{AION_LOCK_CLASS(KnownList::knownObjects#stripe)};` */
	explicit ConcurrentHashMap(const LockClass& stripeLockClass) : stripeLockClass_(stripeLockClass) {}
	/** Java `new ConcurrentHashMap<>(initialCapacity)`: the capacity is a hint */
	explicit ConcurrentHashMap(int32_t) : ConcurrentHashMap() {}
	~ConcurrentHashMap() {
		StripeArray* stripes = stripes_.load(std::memory_order_acquire);
		if (stripes == nullptr)
			return;
		for (Stripe& stripe : stripes->stripes)
			delete stripe.table.load(std::memory_order_acquire);
		delete stripes;
	}
	ConcurrentHashMap(const ConcurrentHashMap&) = delete;
	ConcurrentHashMap& operator=(const ConcurrentHashMap&) = delete;

	// ---- lock-free readers
	Nullable<V> get(const Key& key) const {
		Nullable<V> result{};
		read(key, [&result](const Node& node) { result = Nullable<V>(node.value); });
		return result;
	}
	Value getOrDefault(const Key& key, const Value& defaultValue) const {
		std::optional<Value> result;
		read(key, [&result](const Node& node) { result.emplace(node.value); });
		return result ? *result : defaultValue;
	}
	bool containsKey(const Key& key) const {
		bool found = false;
		read(key, [&found](const Node&) { found = true; });
		return found;
	}
	bool containsValue(const Value& value) const {
		for (const Value& candidate : values()) {
			if (JavaEquality<V>::equals(candidate, value))
				return true;
		}
		return false;
	}
	/** weakly consistent */
	int32_t size() const { return static_cast<int32_t>(mappingCount()); }
	int64_t mappingCount() const {
		int64_t count = count_.load(std::memory_order_acquire);
		return count < 0 ? 0 : count;
	}
	bool isEmpty() const noexcept { return count_.load(std::memory_order_acquire) <= 0; }

	// ---- writers (stripe Monitor)
	/** @throws NullPointerException for a null key or value */
	Nullable<V> put(K key, V value) {
		checkNotNull(key, value);
		TaskScope::ensurePublished();
		decltype(auto) borrowedKey = detail::borrowOf(key);
		uint64_t hash = hashOf(borrowedKey);
		Stripe& stripe = stripeForWrite(hash);
		detail::ShimLock lock(stripe.monitor, stripeLockClass_);
		detail::checkNotBusy(stripe.busy);
		detail::checkNotRecursiveUpdate(this, &borrowedKey);
		Location location = locate(stripe, hash, borrowedKey);
		if (location.node != nullptr) {
			Nullable<V> previous(location.node->value);
			replaceNode(location, std::move(value));
			return previous;
		}
		insertNode(stripe, hash, std::move(key), std::move(value));
		return Nullable<V>{};
	}
	Nullable<V> putIfAbsent(K key, V value) {
		checkNotNull(key, value);
		TaskScope::ensurePublished();
		decltype(auto) borrowedKey = detail::borrowOf(key);
		uint64_t hash = hashOf(borrowedKey);
		Stripe& stripe = stripeForWrite(hash);
		detail::ShimLock lock(stripe.monitor, stripeLockClass_);
		detail::checkNotBusy(stripe.busy);
		Location location = locate(stripe, hash, borrowedKey);
		if (location.node != nullptr)
			return Nullable<V>(location.node->value); // present: no update
		detail::checkNotRecursiveUpdate(this, &borrowedKey);
		insertNode(stripe, hash, std::move(key), std::move(value));
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
		Nullable<V> previous{};
		removeWhere(key, [&previous](const Node& node) {
			previous = Nullable<V>(node.value);
			return true;
		});
		return previous;
	}
	/** removes only if currently mapped to a Java-equal value (CreatureController.cancelTaskIfPresent) */
	bool remove(const Key& key, const Value& value) {
		return removeWhere(key, [&value](const Node& node) { return JavaEquality<V>::equals(Value(node.value), value); });
	}
	Nullable<V> replace(const Key& key, V value) {
		checkNotNullValue(value);
		TaskScope::ensurePublished();
		uint64_t hash = hashOf(key);
		StripeArray* stripes = stripes_.load(std::memory_order_acquire);
		if (stripes == nullptr)
			return Nullable<V>{};
		Stripe& stripe = stripes->stripes[stripeIndexOf(hash)];
		detail::ShimLock lock(stripe.monitor, stripeLockClass_);
		detail::checkNotBusy(stripe.busy);
		detail::checkNotRecursiveUpdate(this, &key);
		Location location = locate(stripe, hash, key);
		if (location.node == nullptr)
			return Nullable<V>{};
		Nullable<V> previous(location.node->value);
		replaceNode(location, std::move(value));
		return previous;
	}
	bool replace(const Key& key, const Value& oldValue, V newValue) {
		checkNotNullValue(newValue);
		TaskScope::ensurePublished();
		uint64_t hash = hashOf(key);
		StripeArray* stripes = stripes_.load(std::memory_order_acquire);
		if (stripes == nullptr)
			return false;
		Stripe& stripe = stripes->stripes[stripeIndexOf(hash)];
		detail::ShimLock lock(stripe.monitor, stripeLockClass_);
		detail::checkNotBusy(stripe.busy);
		detail::checkNotRecursiveUpdate(this, &key);
		Location location = locate(stripe, hash, key);
		if (location.node == nullptr)
			return false;
		{
			detail::BusyScope busy(stripe.busy); // value equals must not modify this stripe (it would invalidate `location`: double retire)
			if (!JavaEquality<V>::equals(Value(location.node->value), oldValue))
				return false;
		}
		replaceNode(location, std::move(newValue));
		return true;
	}
	/** stripe by stripe (not atomic across stripes, like Java) */
	void clear() {
		TaskScope::ensurePublished();
		StripeArray* stripes = stripes_.load(std::memory_order_acquire);
		if (stripes == nullptr)
			return;
		for (Stripe& stripe : stripes->stripes) {
			Table* old = nullptr;
			{
				detail::ShimLock lock(stripe.monitor, stripeLockClass_);
				detail::checkNotBusy(stripe.busy);
				AION_YIELD_POINT("ConcurrentHashMap::clear:table");
				old = stripe.table.exchange(nullptr, std::memory_order_acq_rel);
				count_.fetch_sub(stripe.size, std::memory_order_acq_rel);
				stripe.size = 0;
			}
			if (old != nullptr)
				Reclaimer::retireNode(std::unique_ptr<RetiredNode>(old));
		}
	}

	/** Callback runs under the key's StripeMonitor. @throws IllegalStateException("Recursive update") on same-key recursion */
	template <class F>
	Nullable<V> compute(K key, F&& remapping) {
		checkNotNullKey(key);
		TaskScope::ensurePublished();
		decltype(auto) borrowedKey = detail::borrowOf(key);
		uint64_t hash = hashOf(borrowedKey);
		Stripe& stripe = stripeForWrite(hash);
		detail::ShimLock lock(stripe.monitor, stripeLockClass_);
		detail::checkNotBusy(stripe.busy);
		detail::checkNotRecursiveUpdate(this, &borrowedKey);
		Nullable<V> old{};
		if (Location location = locate(stripe, hash, borrowedKey); location.node != nullptr)
			old = Nullable<V>(location.node->value);
		std::optional<V> result;
		{
			detail::ComputeFrameScope frame(this, &borrowedKey, &sameKey);
			result = detail::toStored<V>(detail::invokeRemapping<K, V>(remapping, borrowedKey, old));
		}
		return storeResult(stripe, hash, std::move(key), borrowedKey, std::move(result));
	}
	template <class F>
	Nullable<V> computeIfAbsent(K key, F&& mapping) {
		checkNotNullKey(key);
		TaskScope::ensurePublished();
		decltype(auto) borrowedKey = detail::borrowOf(key);
		uint64_t hash = hashOf(borrowedKey);
		Stripe& stripe = stripeForWrite(hash);
		detail::ShimLock lock(stripe.monitor, stripeLockClass_);
		detail::checkNotBusy(stripe.busy);
		if (Location location = locate(stripe, hash, borrowedKey); location.node != nullptr)
			return Nullable<V>(location.node->value); // present: no update, no recursion error (Java returns the value)
		detail::checkNotRecursiveUpdate(this, &borrowedKey);
		std::optional<V> result;
		{
			detail::ComputeFrameScope frame(this, &borrowedKey, &sameKey);
			result = detail::toStored<V>(detail::invokeMapping<K>(mapping, borrowedKey));
		}
		if (!result)
			return Nullable<V>{};
		return storeResult(stripe, hash, std::move(key), borrowedKey, std::move(result));
	}
	template <class F>
	Nullable<V> computeIfPresent(K key, F&& remapping) {
		checkNotNullKey(key);
		TaskScope::ensurePublished();
		decltype(auto) borrowedKey = detail::borrowOf(key);
		uint64_t hash = hashOf(borrowedKey);
		StripeArray* stripes = stripes_.load(std::memory_order_acquire);
		if (stripes == nullptr)
			return Nullable<V>{};
		Stripe& stripe = stripes->stripes[stripeIndexOf(hash)];
		detail::ShimLock lock(stripe.monitor, stripeLockClass_);
		detail::checkNotBusy(stripe.busy);
		detail::checkNotRecursiveUpdate(this, &borrowedKey);
		Location location = locate(stripe, hash, borrowedKey);
		if (location.node == nullptr)
			return Nullable<V>{};
		Value old(location.node->value);
		std::optional<V> result;
		{
			detail::ComputeFrameScope frame(this, &borrowedKey, &sameKey);
			result = detail::toStored<V>(detail::invokePresentRemapping<K, V>(remapping, borrowedKey, old));
		}
		return storeResult(stripe, hash, std::move(key), borrowedKey, std::move(result));
	}
	template <class F>
	Nullable<V> merge(K key, V value, F&& remapping) {
		checkNotNull(key, value);
		TaskScope::ensurePublished();
		decltype(auto) borrowedKey = detail::borrowOf(key);
		uint64_t hash = hashOf(borrowedKey);
		Stripe& stripe = stripeForWrite(hash);
		detail::ShimLock lock(stripe.monitor, stripeLockClass_);
		detail::checkNotBusy(stripe.busy);
		detail::checkNotRecursiveUpdate(this, &borrowedKey);
		Location location = locate(stripe, hash, borrowedKey);
		if (location.node == nullptr) {
			Node* inserted = insertNode(stripe, hash, std::move(key), std::move(value));
			return Nullable<V>(inserted->value);
		}
		Value old(location.node->value);
		std::optional<V> result;
		{
			detail::ComputeFrameScope frame(this, &borrowedKey, &sameKey);
			decltype(auto) given = detail::borrowOf(value);
			result = detail::toStored<V>(std::invoke(remapping, std::as_const(old), std::as_const(given)));
		}
		return storeResult(stripe, hash, std::move(key), borrowedKey, std::move(result));
	}

	/** Java entrySet().removeIf / values().removeIf: the predicate runs lock-free, each match is removed under its stripe Monitor. */
	template <class Predicate>
		requires std::predicate<Predicate&, const Key&, const Value&>
	bool removeIf(Predicate&& predicate) {
		bool removed = false;
		for (const Entry& entry : entrySet()) {
			if (std::invoke(predicate, entry.key, entry.value))
				removed |= remove(entry.key, entry.value);
		}
		return removed;
	}
	/** weakly consistent lock-free iteration; the action runs without any stripe Monitor */
	template <class Action>
		requires std::invocable<Action&, const Key&, const Value&>
	void forEach(Action&& action) const {
		for (const Entry& entry : entrySet())
			std::invoke(action, entry.key, entry.value);
	}

	KeyView keySet() const { return KeyView(*this); }
	ValueView values() const { return ValueView(*this); }
	EntryView entrySet() const { return EntryView(*this); }
	/** weakly consistent snapshot (lock-free, like iteration) */
	std::vector<Entry> snapshot() const { return entrySet().toVector(); }

	/** `synchronized (map)`: the map object's own monitor (not a stripe) */
	Monitor& monitor() const noexcept { return monitor_; }
	/** The StripeMonitor that guards `key` (tests, lockdep diagnostics). */
	Monitor& stripeMonitor(const Key& key) const { return const_cast<ConcurrentHashMap*>(this)->stripeForWrite(hashOf(key)).monitor; }

private:
	static constexpr uint32_t INITIAL_BITS = 1;

	static uint64_t hashOf(const Key& key) { return detail::spreadHash(JavaEquality<K>::hash(key)); }
	static size_t stripeIndexOf(uint64_t hash) noexcept { return static_cast<size_t>(hash >> 60); }

	static void checkNotNullKey(const K& key) {
		if (detail::isNull(key))
			detail::throwNullElement("ConcurrentHashMap does not permit null keys");
	}
	static void checkNotNullValue(const V& value) {
		if (detail::isNull(value))
			detail::throwNullElement("ConcurrentHashMap does not permit null values");
	}
	static void checkNotNull(const K& key, const V& value) {
		checkNotNullKey(key);
		checkNotNullValue(value);
	}

	static bool sameKey(const void*, const void* frameKey, const void* otherKey) {
		return JavaEquality<K>::equals(*static_cast<const Key*>(frameKey), *static_cast<const Key*>(otherKey));
	}

	Stripe& stripeForWrite(uint64_t hash) {
		StripeArray* stripes = stripes_.load(std::memory_order_acquire);
		if (stripes == nullptr) [[unlikely]] {
			auto created = std::make_unique<StripeArray>();
			AION_YIELD_POINT("ConcurrentHashMap::allocateStripes");
			if (stripes_.compare_exchange_strong(stripes, created.get(), std::memory_order_acq_rel))
				stripes = created.release();
		}
		return stripes->stripes[stripeIndexOf(hash)];
	}

	/** Lock-free (or locked-reads) lookup; calls `found` with the node if present. */
	template <class Found>
	void read(const Key& key, Found&& found) const {
		TaskScope::ensurePublished();
		uint64_t hash = hashOf(key);
		StripeArray* stripes = stripes_.load(std::memory_order_acquire);
		if (stripes == nullptr)
			return;
		Stripe& stripe = stripes->stripes[stripeIndexOf(hash)];
		if constexpr (LockedReads) {
			detail::ShimLock lock(stripe.monitor, stripeLockClass_);
			detail::BusyScope busy(stripe.busy);
			if (Node* node = findNode(stripe, hash, key))
				found(*node);
		} else {
			if (Node* node = findNode(stripe, hash, key))
				found(*node);
		}
	}
	static Node* findNode(const Stripe& stripe, uint64_t hash, const Key& key) {
		AION_YIELD_POINT("ConcurrentHashMap::read:table");
		Table* table = stripe.table.load(std::memory_order_acquire);
		if (table == nullptr)
			return nullptr;
		AION_YIELD_POINT("ConcurrentHashMap::read:bucket");
		for (Node* node = table->bucket(hash).load(std::memory_order_acquire); node != nullptr;) {
			if (node->hash == hash && JavaEquality<K>::equals(Key(node->key), key))
				return node;
			AION_YIELD_POINT("ConcurrentHashMap::read:next");
			node = node->next.load(std::memory_order_acquire);
		}
		return nullptr;
	}

	/** under the stripe Monitor */
	Location locate(Stripe& stripe, uint64_t hash, const Key& key) {
		detail::BusyScope busy(stripe.busy);
		Table* table = stripe.table.load(std::memory_order_acquire);
		if (table == nullptr)
			return {nullptr, nullptr};
		std::atomic<Node*>* link = &table->bucket(hash);
		for (Node* node = link->load(std::memory_order_acquire); node != nullptr; node = link->load(std::memory_order_acquire)) {
			if (node->hash == hash && JavaEquality<K>::equals(Key(node->key), key))
				return {link, node};
			link = &node->next;
		}
		return {link, nullptr};
	}

	/** under the stripe Monitor: links a new node at the head of its bucket (resizing first if needed) */
	Node* insertNode(Stripe& stripe, uint64_t hash, K key, V value) {
		auto node = std::make_unique<Node>(hash, std::move(key), std::move(value));
		ensureCapacity(stripe, stripe.size + 1);
		Table* table = stripe.table.load(std::memory_order_acquire);
		std::atomic<Node*>& bucket = table->bucket(hash);
		node->next.store(bucket.load(std::memory_order_acquire), std::memory_order_release);
		AION_YIELD_POINT("ConcurrentHashMap::insert:link");
		Node* inserted = node.release();
		bucket.store(inserted, std::memory_order_release);
		++stripe.size;
		count_.fetch_add(1, std::memory_order_acq_rel);
		return inserted;
	}
	/** under the stripe Monitor: replaces the located node by a copy with `value` and retires it */
	Node* replaceNode(const Location& location, V value) {
		Node* old = location.node;
		auto node = std::make_unique<Node>(old->hash, old->key, std::move(value));
		node->next.store(old->next.load(std::memory_order_acquire), std::memory_order_release);
		AION_YIELD_POINT("ConcurrentHashMap::replace:link");
		Node* replacement = node.release();
		location.link->store(replacement, std::memory_order_release);
		Reclaimer::retireNode(std::unique_ptr<RetiredNode>(old));
		return replacement;
	}
	/** under the stripe Monitor: unlinks the located node and retires it */
	void unlinkNode(Stripe& stripe, const Location& location) {
		AION_YIELD_POINT("ConcurrentHashMap::remove:unlink");
		location.link->store(location.node->next.load(std::memory_order_acquire), std::memory_order_release);
		Reclaimer::retireNode(std::unique_ptr<RetiredNode>(location.node));
		--stripe.size;
		count_.fetch_sub(1, std::memory_order_acq_rel);
	}
	/** under the stripe Monitor: creates or doubles the table so that `mappings` fit at load factor 3/4 */
	void ensureCapacity(Stripe& stripe, int32_t mappings) { // the stripe array exists: `stripe` is one of its elements
		Table* table = stripe.table.load(std::memory_order_acquire);
		if (table == nullptr) {
			AION_YIELD_POINT("ConcurrentHashMap::createTable");
			stripe.table.store(new Table(INITIAL_BITS), std::memory_order_release);
			return;
		}
		if (static_cast<size_t>(mappings) * 4 <= table->capacity() * 3)
			return;
		std::unique_ptr<Table> resized;
		{
			std::scoped_lock resizeLock(stripes_.load(std::memory_order_acquire)->resizeMutex); // CONTAINER_SLOT leaf: no callbacks, no Monitors
			resized = std::make_unique<Table>(table->bits + 1);
			for (size_t i = 0; i < table->capacity(); ++i) {
				for (Node* node = table->buckets[i].load(std::memory_order_acquire); node != nullptr; node = node->next.load(std::memory_order_acquire)) {
					auto copy = std::make_unique<Node>(node->hash, node->key, node->value);
					std::atomic<Node*>& bucket = resized->bucket(node->hash);
					copy->next.store(bucket.load(std::memory_order_relaxed), std::memory_order_relaxed);
					bucket.store(copy.release(), std::memory_order_relaxed);
				}
			}
		}
		AION_YIELD_POINT("ConcurrentHashMap::resize:publish");
		stripe.table.store(resized.release(), std::memory_order_release);
		Reclaimer::retireNode(std::unique_ptr<RetiredNode>(table));
	}

	/** compute-family tail under the stripe Monitor: re-locates the key (the callback may have changed the stripe) and stores the result */
	Nullable<V> storeResult(Stripe& stripe, uint64_t hash, K&& key, const Key& borrowedKey, std::optional<V> result) { // `borrowedKey` may refer to `key`
		Location location = locate(stripe, hash, borrowedKey);
		if (!result) {
			if (location.node != nullptr)
				unlinkNode(stripe, location);
			return Nullable<V>{};
		}
		if (location.node != nullptr)
			return Nullable<V>(replaceNode(location, std::move(*result))->value);
		return Nullable<V>(insertNode(stripe, hash, std::move(key), std::move(*result))->value);
	}

	template <class Predicate>
	bool removeWhere(const Key& key, Predicate&& predicate) {
		TaskScope::ensurePublished();
		uint64_t hash = hashOf(key);
		StripeArray* stripes = stripes_.load(std::memory_order_acquire);
		if (stripes == nullptr)
			return false;
		Stripe& stripe = stripes->stripes[stripeIndexOf(hash)];
		detail::ShimLock lock(stripe.monitor, stripeLockClass_);
		detail::checkNotBusy(stripe.busy);
		detail::checkNotRecursiveUpdate(this, &key);
		Location location = locate(stripe, hash, key);
		if (location.node == nullptr)
			return false;
		bool matches;
		{
			detail::BusyScope busy(stripe.busy);
			matches = predicate(*location.node);
		}
		if (!matches)
			return false;
		unlinkNode(stripe, location);
		return true;
	}
	bool removeKey(const Key& key) {
		return removeWhere(key, [](const Node&) { return true; });
	}
	bool removeIfIdentical(const Key& key, const Value& value) {
		return removeWhere(key, [&value](const Node& node) { return detail::identical(detail::borrowOf(node.value), value); });
	}

	const LockClass& stripeLockClass_;
	mutable Monitor monitor_;
	std::atomic<int64_t> count_{0};
	std::atomic<StripeArray*> stripes_{nullptr};
};

/**
 * Java: ConcurrentHashMap.newKeySet() (`Set<Integer> registeredObjects = ConcurrentHashMap.newKeySet();`). A ConcurrentHashMap<K, bool>
 * with Set operations: lock-free contains/iteration, add/remove under the stripe Monitor. Java equals for membership.
 * Thread-safety: every member is thread-safe.
 */
template <class K>
class ConcurrentKeySet {
public:
	using value_type = K;
	using Element = Borrowed<K>;

	ConcurrentKeySet() = default;
	explicit ConcurrentKeySet(const LockClass& stripeLockClass) : map_(stripeLockClass) {}
	ConcurrentKeySet(const ConcurrentKeySet&) = delete;
	ConcurrentKeySet& operator=(const ConcurrentKeySet&) = delete;

	bool add(K value) { return detail::isNull(map_.putIfAbsent(std::move(value), true)); }
	template <std::ranges::input_range R>
	bool addAll(R&& values) {
		bool changed = false;
		for (auto&& value : values)
			changed |= add(detail::toElement<K>(std::forward<decltype(value)>(value)));
		return changed;
	}
	bool remove(const Element& value) { return !detail::isNull(map_.remove(value)); }
	bool contains(const Element& value) const { return map_.containsKey(value); }
	int32_t size() const { return map_.size(); }
	bool isEmpty() const noexcept { return map_.isEmpty(); }
	void clear() { map_.clear(); }
	template <class Predicate>
		requires std::predicate<Predicate&, const Element&>
	bool removeIf(Predicate&& predicate) {
		return map_.removeIf([&predicate](const Element& key, const bool&) { return static_cast<bool>(std::invoke(predicate, key)); });
	}
	typename ConcurrentHashMap<K, bool>::KeyView::Iterator begin() const { return map_.keySet().begin(); }
	std::default_sentinel_t end() const noexcept { return {}; }
	JavaIterator<Element> iterator() const { return map_.keySet().iterator(); }
	std::vector<Element> snapshot() const { return map_.keySet().toVector(); }

	Monitor& monitor() const noexcept { return map_.monitor(); }

private:
	ConcurrentHashMap<K, bool> map_;
};

} // namespace aion::gameserver::runtime
