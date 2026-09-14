#pragma once

#include <array>
#include <atomic>
#include <bit>
#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <type_traits>
#include <typeinfo>
#include <utility>
#include <vector>

#include "aion/gameserver/runtime/base/Checked.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/runtime/base/YieldPoint.h"
#include "aion/gameserver/runtime/lifetime/Reclaimer.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/TaskScope.h"
#include "aion/gameserver/runtime/sync/Monitor.h"

namespace aion::gameserver::runtime {

namespace detail {
/** Reclaimer access to an OwnedPart's reference count and stamp (Reclaimer.cpp). */
struct OwnedPartAccess;
} // namespace detail

/**
 * Base of sub-objects that an owner creates for itself (design §2.3, §3.2.1): controllers, stats, effect lists, known lists, storages, AI.
 * A part shares its owner's lifetime: Ref<Part>, Pin and task captures of a part retain the OWNER, and the part is destroyed by the owner's
 * destructor (const std::unique_ptr / PartSlot / PartMap / PartList members) or, after replacement, by the Reclaimer (retirePart).
 *
 * - Owner known at construction: `explicit OwnedPart(owner)` (pattern 1/2: `new X(this)`).
 * - Late-bound controllers (pattern 3): default-construct, then the owner calls bindOwner(*this) in its constructor, before publication.
 * - A part's reference back to its owner is OwnerRef<O> (non-retaining); a field that may hold the owner or a foreign object is SelfOrRef<O>.
 * Checked builds (C11): retain/release/partOwner on an unbound part terminate; bindOwner twice, or after the owner was published (count > 1),
 * terminates. (The owner's count is 1 while its constructor runs under makeRef and right after; "published" means a second reference.)
 *
 * Replaced parts and Refs to them (review correction of design §2.3, which only reasoned about borrows): a part replaced in a
 * PartSlot<P, RECLAIMER> or PartMap may still be held by a Ref<Part> / Field<Ref<Part>> (`Player.playerAccountData` of a stale Player after
 * `account.addPlayerAccountData(reloaded)`), and retaining only the owner does not keep the part itself alive. Therefore retain()/release()
 * also count references to the part itself, and every release() stamps the part with max(stamp, E) before its decrement (the same rule as
 * RefCounted::release). The Reclaimer destroys a retired part only when no Ref holds it (partRefs == 0) and its stamp (the later of the
 * retirement epoch and the last release) is older than every published epoch. Parts that are never retired pay one extra atomic increment
 * and decrement per Ref; their count is only checked when they are destroyed (C5: no Ref may hold a destroyed part).
 * Pin(&part) in the scheduler retains the part through retain()/release() (and with it the owner), so a pinned part replaced in a RECLAIMER
 * container stays alive until the task ends (sched/Pin.h).
 * Yield points: "OwnedPart::retain", "OwnedPart::release:stamp", "OwnedPart::release:decrement".
 *
 * Thread-safety: retain/release/partOwner are lock-free and thread-safe after binding; bindOwner must happen before the part is shared.
 */
class OwnedPart : public OwnedPartBase {
public:
	/** Retains the owner and counts a reference to the part. */
	void retain() const noexcept;
	/** Stamps the part with the current epoch, uncounts the reference and releases the owner. */
	void release() const noexcept;
	/** Racy snapshot of the number of Refs to this part (tests, Reclaimer). */
	uint32_t partRefCount() const noexcept { return partRefs_.load(std::memory_order_acquire); }
	/** The owner (checked: bound, C11). */
	const RefCounted& partOwner() const noexcept;
	bool isOwnerBound() const noexcept { return owner_ != nullptr; }

	/** The part's own Java monitor (`synchronized (controller)` locks the part object, not the owner), lock class = dynamic type. */
	Monitor& monitor() const noexcept { return monitor_; }

	OwnedPart(const OwnedPart&) = delete;
	OwnedPart& operator=(const OwnedPart&) = delete;

protected:
	explicit OwnedPart(const RefCounted& owner) noexcept : owner_(&owner) {}
	OwnedPart() noexcept = default;
	/** Checked builds: terminates if a Ref still holds the part (C5). */
	~OwnedPart() override;

	/** Binds the owner of a late-bound part, once, before publication (checked: C11). */
	void bindOwner(const RefCounted& owner) noexcept;

private:
	friend struct detail::OwnedPartAccess;

	const RefCounted* owner_ = nullptr;
	/** Refs (and other retain() holders) of the part itself */
	mutable std::atomic<uint32_t> partRefs_{0};
	/** max(E) over every release() of the part and its retirement (Reclaimer::retirePart) */
	mutable std::atomic<uint64_t> partStamp_{0};
	mutable Monitor monitor_;
};

/** A part's non-retaining reference to its owner (design §2.3, §3.2: `const OwnerRef<Npc> owner;`). */
template <class O>
using OwnerRef = O&;

/** Where a PartSlot puts replaced parts (design §2.3). */
enum class RetireTo : uint8_t {
	/** kept in the slot's retired list until the owner dies (Creature.ai; replacements are rare) */
	OWNER,
	/** handed to Reclaimer::retirePart, destroyed after an epoch (Player storages, Account.accountWarehouse, replaced on every re-entry) */
	RECLAIMER,
};

namespace detail {
[[noreturn]] void throwNullPart(const std::type_info& type);
[[noreturn]] void throwNullPartArgument(const std::type_info& type);
[[noreturn]] void throwPartIndexOutOfBounds(int32_t index, int32_t size);
/** C11: terminates unless `part` is bound to `owner` */
void checkPartOwner(const OwnedPart& part, const RefCounted& owner, const std::type_info& type) noexcept;

/** Checked builds: C11 owner check for parts deriving OwnedPart; no-op for other types and in release builds. */
template <class P>
void checkPartOwnerIfOwnedPart([[maybe_unused]] const P* part, [[maybe_unused]] const RefCounted& owner) noexcept {
	if constexpr (CHECKED && std::is_base_of_v<OwnedPart, P>) {
		if (part != nullptr)
			checkPartOwner(*part, owner, typeid(P));
	}
}
} // namespace detail

/**
 * Replaceable single part (design §2.3, §3.2.1 pattern 2: a non-final field set with `setF(new X(this))`).
 *
 * - get(), `*`, `->` are pointer loads: they call TaskScope::ensurePublished() first (read barrier) and return a borrow valid until the task
 *   ends. `*`/`->` throw NullPointerException when empty.
 * - set() publishes the new part atomically (exchange); the previous part is retired according to R. set() may race with readers and other
 *   set() calls (last writer wins, as a Java field store). Checked builds (C11): an OwnedPart must already be bound to the slot's owner.
 *   RetireTo::OWNER keeps the previous part in a lock-free list until the slot is destroyed; RetireTo::RECLAIMER hands it to
 *   Reclaimer::retirePart (P must derive OwnedPartBase; an OwnedPart is additionally kept while a Ref to it exists, see OwnedPart).
 * - The slot's destructor (run by the owner's destructor) destroys the current part and every part retired to OWNER.
 * Thread-safety: all members are thread-safe. Yield points: "PartSlot::load", "PartSlot::exchange".
 */
template <class P, RetireTo R = RetireTo::OWNER>
class PartSlot {
public:
	explicit PartSlot(const RefCounted& owner) noexcept : owner_(owner) {}
	PartSlot(const RefCounted& owner, std::unique_ptr<P> initial) noexcept : owner_(owner) {
		detail::checkPartOwnerIfOwnedPart(initial.get(), owner_);
		current_.store(initial.release(), std::memory_order_release);
	}
	~PartSlot() {
		delete current_.load(std::memory_order_acquire);
		for (RetiredToOwner* node = retiredToOwner_.load(std::memory_order_acquire); node != nullptr;) {
			RetiredToOwner* next = node->next;
			delete node->part;
			delete node;
			node = next;
		}
	}
	PartSlot(const PartSlot&) = delete;
	PartSlot& operator=(const PartSlot&) = delete;

	/** Pointer load with read barrier; nullptr if empty. */
	P* get() const noexcept {
		TaskScope::ensurePublished();
		AION_YIELD_POINT("PartSlot::load");
		return current_.load(std::memory_order_acquire);
	}
	/** @throws NullPointerException if empty */
	P& operator*() const {
		P* part = get();
		if (part == nullptr) [[unlikely]]
			detail::throwNullPart(typeid(P));
		return *part;
	}
	/** @throws NullPointerException if empty */
	P* operator->() const { return &**this; }
	explicit operator bool() const noexcept { return get() != nullptr; }

	/** Publishes `part` (may be null) and retires the previous part to R. @throws std::bad_alloc (RetireTo::OWNER, before publishing) */
	void set(std::unique_ptr<P> part) {
		detail::checkPartOwnerIfOwnedPart(part.get(), owner_);
		std::unique_ptr<RetiredToOwner> node;
		if constexpr (R == RetireTo::OWNER)
			node = std::make_unique<RetiredToOwner>();
		AION_YIELD_POINT("PartSlot::exchange");
		P* previous = current_.exchange(part.release(), std::memory_order_acq_rel);
		if (previous == nullptr)
			return;
		if constexpr (R == RetireTo::RECLAIMER) {
			Reclaimer::retirePart(owner_, std::unique_ptr<OwnedPartBase>(previous));
		} else {
			node->part = previous;
			RetiredToOwner* head = retiredToOwner_.load(std::memory_order_acquire);
			do {
				node->next = head;
			} while (!retiredToOwner_.compare_exchange_weak(head, node.get(), std::memory_order_acq_rel));
			(void)node.release();
		}
	}

private:
	struct RetiredToOwner {
		P* part = nullptr;
		RetiredToOwner* next = nullptr;
	};

	const RefCounted& owner_;
	std::atomic<P*> current_{nullptr};
	/** RetireTo::OWNER only: replaced parts (push-only lock-free stack, freed by the destructor) */
	std::atomic<RetiredToOwner*> retiredToOwner_{nullptr};
};

/**
 * Map of parts keyed by K (design §2.3, RR-11/RR-19: Account.players → PlayerAccountData).
 * - get() is a pointer load (read barrier) returning a borrow valid until the task ends, null if absent.
 * - put() replaces and remove() removes; the previous part is retired to the Reclaimer (never destroyed while borrowable or held by a Ref to
 *   the part, see OwnedPart). put(key, nullptr)
 *   throws NullPointerException (Java ConcurrentHashMap semantics; a part map never stores null).
 * - Iteration uses snapshots in key order (`for (auto& [key, part] : map.snapshot())`).
 * - The destructor (owner's destructor) destroys the remaining parts.
 * - K must be less-than comparable (std::map). P must derive OwnedPartBase; checked builds verify OwnedParts are bound to the owner (C11).
 * Thread-safety: all members are thread-safe; every operation runs under the map's Monitor (monitor(), a reentrant game-level lock, so
 * callers may hold other Monitors), parts are retired after the Monitor is released.
 */
template <class K, class P>
class PartMap {
public:
	explicit PartMap(const RefCounted& owner) noexcept : owner_(owner) {}
	~PartMap() = default;
	PartMap(const PartMap&) = delete;
	PartMap& operator=(const PartMap&) = delete;

	/** @return the part for `key`, null if absent */
	Ptr<P> get(const K& key) const {
		TaskScope::ensurePublished();
		SYNCHRONIZED(monitor_) {
			auto it = parts_.find(key);
			return it == parts_.end() ? Ptr<P>() : Ptr<P>(*it->second);
		}
	}
	/** Stores `part` for `key`; a replaced part is retired to the Reclaimer. @throws NullPointerException if `part` is null */
	void put(K key, std::unique_ptr<P> part) {
		if (part == nullptr)
			detail::throwNullPartArgument(typeid(P));
		detail::checkPartOwnerIfOwnedPart(part.get(), owner_);
		std::unique_ptr<P> previous;
		SYNCHRONIZED(monitor_) {
			std::unique_ptr<P>& slot = parts_[std::move(key)];
			previous = std::exchange(slot, std::move(part));
			size_.store(static_cast<int32_t>(parts_.size()), std::memory_order_release);
		}
		retire(std::move(previous));
	}
	/** Removes the part for `key` and retires it to the Reclaimer. @return true if a part was removed */
	bool remove(const K& key) {
		std::unique_ptr<P> previous;
		SYNCHRONIZED(monitor_) {
			auto it = parts_.find(key);
			if (it == parts_.end())
				return false;
			previous = std::move(it->second);
			parts_.erase(it);
			size_.store(static_cast<int32_t>(parts_.size()), std::memory_order_release);
		}
		retire(std::move(previous));
		return true;
	}
	bool containsKey(const K& key) const {
		SYNCHRONIZED(monitor_) {
			return parts_.contains(key);
		}
	}
	/** lock-free size snapshot */
	int32_t size() const noexcept { return size_.load(std::memory_order_acquire); }
	/** lock-free */
	bool isEmpty() const noexcept { return size() == 0; }
	/** Snapshot of all entries in key order (borrows valid until the task ends). */
	std::vector<std::pair<K, Ptr<P>>> snapshot() const {
		TaskScope::ensurePublished();
		std::vector<std::pair<K, Ptr<P>>> result;
		SYNCHRONIZED(monitor_) {
			result.reserve(parts_.size());
			for (const auto& [key, part] : parts_)
				result.emplace_back(key, Ptr<P>(*part));
		}
		return result;
	}
	/** Snapshot of all parts in key order. */
	std::vector<Ptr<P>> values() const {
		TaskScope::ensurePublished();
		std::vector<Ptr<P>> result;
		SYNCHRONIZED(monitor_) {
			result.reserve(parts_.size());
			for (const auto& [key, part] : parts_)
				result.emplace_back(*part);
		}
		return result;
	}

	Monitor& monitor() const noexcept { return monitor_; }

private:
	void retire(std::unique_ptr<P> previous) noexcept {
		if (previous != nullptr)
			Reclaimer::retirePart(owner_, std::unique_ptr<OwnedPartBase>(previous.release()));
	}

	const RefCounted& owner_;
	mutable Monitor monitor_;
	std::map<K, std::unique_ptr<P>> parts_;
	std::atomic<int32_t> size_{0};
};

/**
 * Append-only list of parts with stable addresses (design §2.3: SpawnGroup templates). Elements are never removed before the owner dies.
 *
 * Storage is a fixed directory of chunks with doubling capacities (16, 32, 64, ...), so neither parts nor slots ever move and no chunk is
 * ever retired: readers are lock-free (load size, load chunk, load slot) and need no epoch reclamation. add() publishes the slot before the
 * size, so every index below size() is readable.
 * Thread-safety: all members are thread-safe; add() is serialized by monitor(). Yield point: "PartList::publish".
 */
template <class P>
class PartList {
public:
	explicit PartList(const RefCounted& owner) noexcept : owner_(owner) {}
	~PartList() {
		int32_t count = size_.load(std::memory_order_acquire);
		for (int32_t i = 0; i < count; ++i)
			delete slot(i).load(std::memory_order_acquire);
		for (auto& chunk : chunks_)
			delete[] chunk.load(std::memory_order_acquire);
	}
	PartList(const PartList&) = delete;
	PartList& operator=(const PartList&) = delete;

	/** Appends and returns the stable part. @throws NullPointerException if `part` is null; IllegalStateException if the list is full */
	P& add(std::unique_ptr<P> part) {
		if (part == nullptr)
			detail::throwNullPartArgument(typeid(P));
		detail::checkPartOwnerIfOwnedPart(part.get(), owner_);
		SYNCHRONIZED(monitor_) {
			int32_t index = size_.load(std::memory_order_relaxed);
			if (index == INT32_MAX)
				throw IllegalStateException("PartList is full");
			auto [chunkIndex, offset] = locate(index);
			if (chunks_[chunkIndex].load(std::memory_order_relaxed) == nullptr)
				chunks_[chunkIndex].store(new std::atomic<P*>[chunkCapacity(chunkIndex)](), std::memory_order_release);
			P* raw = part.release();
			chunks_[chunkIndex].load(std::memory_order_relaxed)[offset].store(raw, std::memory_order_release);
			AION_YIELD_POINT("PartList::publish");
			size_.store(index + 1, std::memory_order_release);
			return *raw;
		}
	}
	/** Pointer load with read barrier. @throws IndexOutOfBoundsException */
	Ptr<P> get(int32_t index) const {
		TaskScope::ensurePublished();
		int32_t count = size();
		if (index < 0 || index >= count)
			detail::throwPartIndexOutOfBounds(index, count);
		return Ptr<P>(*slot(index).load(std::memory_order_acquire));
	}
	int32_t size() const noexcept { return size_.load(std::memory_order_acquire); }
	bool isEmpty() const noexcept { return size() == 0; }
	/** Snapshot of the parts published so far, in insertion order. */
	std::vector<Ptr<P>> snapshot() const {
		TaskScope::ensurePublished();
		int32_t count = size();
		std::vector<Ptr<P>> result;
		result.reserve(static_cast<size_t>(count));
		for (int32_t i = 0; i < count; ++i)
			result.emplace_back(*slot(i).load(std::memory_order_acquire));
		return result;
	}

	Monitor& monitor() const noexcept { return monitor_; }

private:
	static constexpr uint32_t FIRST_CHUNK_BITS = 4; // first chunk holds 16 parts
	static constexpr size_t CHUNKS = 29;

	static size_t chunkCapacity(size_t chunkIndex) noexcept { return size_t{1} << (FIRST_CHUNK_BITS + chunkIndex); }
	static std::pair<size_t, size_t> locate(int32_t index) noexcept {
		uint64_t value = static_cast<uint64_t>(index) + (uint64_t{1} << FIRST_CHUNK_BITS);
		size_t chunkIndex = static_cast<size_t>(std::bit_width(value)) - (FIRST_CHUNK_BITS + 1);
		return {chunkIndex, static_cast<size_t>(value - (uint64_t{1} << (FIRST_CHUNK_BITS + chunkIndex)))};
	}
	std::atomic<P*>& slot(int32_t index) const noexcept {
		auto [chunkIndex, offset] = locate(index);
		return chunks_[chunkIndex].load(std::memory_order_acquire)[offset];
	}

	const RefCounted& owner_;
	mutable Monitor monitor_;
	std::array<std::atomic<std::atomic<P*>*>, CHUNKS> chunks_{};
	std::atomic<int32_t> size_{0};
};

/**
 * Part field of the owner's type that holds either the owner itself or a foreign object (design §2.3, §3.2, RR-5/RR-13): PlayerStorage.actor
 * is the owning Player for inventory/warehouse and the entering Player for the account warehouse; VisibleObject.target may be the object
 * itself (TargetField, RT-4).
 *
 * On store: identity with the owner → stored tagged and non-retaining (no self cycle); otherwise retained like Field<Ref<O>>. The owner is
 * `self` for the object form and `part.partOwner()` (resolved at store time, so late-bound parts work) for the part form.
 * API as Field<Ref<O>> (design §3.2.3): get() → Ptr<O> (pointer load with read barrier), `->`, `*` (NullPointerException), set/assignment,
 * exchange (returns the previous value as a Ref: a new reference for the owner, the stored reference for a foreign object), compareAndSet
 * (identity; returns false without storing when the current value is not `expected`).
 * The destructor (owner's destructor) releases a stored foreign object.
 * O must derive RefCounted. Thread-safety: all members are thread-safe (atomic tagged pointer; the value is retained before it is published
 * and a replaced foreign value is released after it was unlinked, so concurrent readers keep their borrows). Yield points: "SelfOrRef::load",
 * "SelfOrRef::exchange", "SelfOrRef::cas".
 */
template <class O>
class SelfOrRef {
public:
	/** Field of the object `self` itself (TargetField on VisibleObject). */
	explicit SelfOrRef(const RefCounted& self) noexcept : ownerObject_(&self) {}
	/** Field of a part: identity is checked against part.partOwner() at store time (works for late-bound parts). */
	explicit SelfOrRef(const OwnedPart& part) noexcept : ownerPart_(&part) {}
	~SelfOrRef() {
		uintptr_t stored = value_.load(std::memory_order_acquire);
		if (stored != 0 && (stored & SELF_TAG) == 0)
			reinterpret_cast<O*>(stored)->release();
	}
	SelfOrRef(const SelfOrRef&) = delete;
	SelfOrRef& operator=(const SelfOrRef&) = delete;

	/** Pointer load with read barrier; null Ptr if empty. */
	Ptr<O> get() const noexcept {
		TaskScope::ensurePublished();
		AION_YIELD_POINT("SelfOrRef::load");
		return Ptr<O>(untag(value_.load(std::memory_order_acquire)));
	}
	/** @throws NullPointerException if empty */
	O& operator*() const { return *get(); }
	/** @throws NullPointerException if empty */
	O* operator->() const { return get().operator->(); }
	explicit operator bool() const noexcept { return static_cast<bool>(get()); }

	void set(Ptr<O> value) { (void)exchange(value); }
	SelfOrRef& operator=(Ptr<O> value) {
		set(value);
		return *this;
	}
	SelfOrRef& operator=(std::nullptr_t) {
		set(Ptr<O>());
		return *this;
	}
	/** Stores `value` and returns the previous value as a Ref. */
	Ref<O> exchange(Ptr<O> value) {
		uintptr_t next = encodeRetained(value.rawPointer());
		AION_YIELD_POINT("SelfOrRef::exchange");
		return decodeOwned(value_.exchange(next, std::memory_order_acq_rel));
	}
	/** Stores `value` if the current value is identical to `expected` (null matches an empty field). */
	bool compareAndSet(Ptr<O> expected, Ptr<O> value) {
		uintptr_t next = encodeRetained(value.rawPointer());
		uintptr_t current = value_.load(std::memory_order_acquire);
		for (;;) {
			if (untag(current) != expected.rawPointer()) {
				releaseEncoded(next);
				return false;
			}
			AION_YIELD_POINT("SelfOrRef::cas");
			if (value_.compare_exchange_strong(current, next, std::memory_order_acq_rel)) {
				releaseEncoded(current); // releases a replaced foreign value (after the unlink)
				return true;
			}
		}
	}

private:
	static constexpr uintptr_t SELF_TAG = 1;

	static O* untag(uintptr_t stored) noexcept { return reinterpret_cast<O*>(stored & ~SELF_TAG); }

	const RefCounted& owner() const noexcept { return ownerObject_ != nullptr ? *ownerObject_ : ownerPart_->partOwner(); }

	/** Encoding of `raw` for storage: tagged for the owner, retained otherwise. */
	uintptr_t encodeRetained(O* raw) const noexcept {
		if (raw == nullptr)
			return 0;
		if (static_cast<const RefCounted*>(raw) == &owner())
			return reinterpret_cast<uintptr_t>(raw) | SELF_TAG;
		raw->retain();
		return reinterpret_cast<uintptr_t>(raw);
	}
	static void releaseEncoded(uintptr_t encoded) noexcept {
		if (encoded != 0 && (encoded & SELF_TAG) == 0)
			reinterpret_cast<O*>(encoded)->release();
	}
	/** Converts an unlinked stored value into a Ref (adopting the stored reference of a foreign object). */
	static Ref<O> decodeOwned(uintptr_t stored) noexcept {
		O* object = untag(stored);
		if (object == nullptr)
			return Ref<O>();
		if ((stored & SELF_TAG) != 0)
			return Ref<O>(object); // the owner: a new reference
		return Ref<O>::adopt(object);
	}

	const RefCounted* ownerObject_ = nullptr;
	const OwnedPart* ownerPart_ = nullptr;
	/** O* with SELF_TAG set when the stored value is the owner (non-retaining) */
	std::atomic<uintptr_t> value_{0};
};

} // namespace aion::gameserver::runtime
