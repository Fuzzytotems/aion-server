#pragma once

// Synthetic game objects of the kernel stress harness (design §12.4, §19 P2). They use every kernel member kind a generated K4 class uses
// (Field, Field<Ref>, Field<std::string>, SelfOrRef, PartSlot in both retire modes, a late-bound OwnedPart, plain and concurrent shims, Atomic*)
// and carry canaries so the harness detects use-after-free and torn memory at every access, also in builds without ASan.
//
// Cycle discipline (so that the leak check at the end can demand exactly zero live objects without cycle breakers):
// - every StressObject has a constant rank in [0, RANKS); retaining references FROM an object or one of its parts go only to objects of a
//   strictly lower rank (link, target when foreign, known, part refs), so object graphs are DAGs with a depth of at most RANKS levels;
// - the only back edge is object -> tasks (FutureRef) -> task captures -> object, as in CreatureController; it is broken when the task runs,
//   is cancelled or self-cancels (bounded runs), like the Java code relies on;
// - the hub (all global structures) is referenced only by the harness and by retired hub parts (Reclaimer::retirePart holds the owner).

#include <atomic>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>

#include "aion/gameserver/runtime/collections/Collections.h"
#include "aion/gameserver/runtime/fields/Array.h"
#include "aion/gameserver/runtime/fields/Atomic.h"
#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/lifetime/Parts.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/gameserver/runtime/services/CleanerQueue.h"
#include "aion/gameserver/runtime/sched/Future.h"
#include "aion/gameserver/runtime/sync/Monitor.h"
#include "aion/gameserver/runtime/sync/Semaphore.h"

namespace aion::gameserver::runtime::stress {

inline constexpr uint64_t OBJECT_MAGIC = 0x5354524553534F42ULL; // "STRESSOB"
inline constexpr uint64_t PART_MAGIC = 0x5354524553535041ULL;   // "STRESSPA"
inline constexpr uint64_t HUB_MAGIC = 0x5354524553534855ULL;    // "STRESSHU"
inline constexpr uint64_t DEAD_CANARY = 0xDEADC0DEDEADC0DEULL;
inline constexpr int32_t RANKS = 6;

/** Global live-instance accounting (destructors are release-only: they only touch these atomics and the CleanerQueue). */
struct StressCounters {
	std::atomic<int64_t> objectsCreated{0};
	std::atomic<int64_t> objectsDestroyed{0};
	std::atomic<int64_t> partsCreated{0};
	std::atomic<int64_t> partsDestroyed{0};
	std::atomic<int64_t> hubsCreated{0};
	std::atomic<int64_t> hubsDestroyed{0};
	std::atomic<int64_t> cleanerPushed{0};
	std::atomic<int64_t> cleanerCleaned{0};

	int64_t liveObjects() const noexcept { return objectsCreated.load() - objectsDestroyed.load(); }
	int64_t liveParts() const noexcept { return partsCreated.load() - partsDestroyed.load(); }
	int64_t liveHubs() const noexcept { return hubsCreated.load() - hubsDestroyed.load(); }
};

inline StressCounters& stressCounters() noexcept {
	static StressCounters counters;
	return counters;
}

/**
 * Field<std::string> payloads carry a checksum of their text, so a torn or freed string box is detected on read:
 * "<id>:<serial>:<padding>#<8 hex digits of FNV-1a over everything before '#'>".
 */
inline uint32_t fnv1a(std::string_view text) noexcept {
	uint32_t hash = 2166136261u;
	for (char c : text) {
		hash ^= static_cast<uint8_t>(c);
		hash *= 16777619u;
	}
	return hash;
}

inline std::string makeCheckedString(int32_t id, uint64_t serial, size_t padding) {
	std::string text = std::to_string(id) + ':' + std::to_string(serial) + ':' + std::string(padding, static_cast<char>('a' + serial % 26));
	static constexpr char HEX[] = "0123456789abcdef";
	uint32_t hash = fnv1a(text);
	text.push_back('#');
	for (int shift = 28; shift >= 0; shift -= 4)
		text.push_back(HEX[(hash >> shift) & 0xF]);
	return text;
}

/** true for "" (Java null) and for intact checked strings */
inline bool isIntactCheckedString(const std::string& text) noexcept {
	if (text.empty())
		return true;
	if (text.size() < 9 || text[text.size() - 9] != '#')
		return false;
	uint32_t expected = 0;
	for (size_t i = text.size() - 8; i < text.size(); ++i) {
		char c = text[i];
		uint32_t digit = c >= '0' && c <= '9' ? static_cast<uint32_t>(c - '0') : (c >= 'a' && c <= 'f' ? static_cast<uint32_t>(c - 'a' + 10) : 16u);
		if (digit > 15)
			return false;
		expected = (expected << 4) | digit;
	}
	return fnv1a(std::string_view(text).substr(0, text.size() - 9)) == expected;
}

class StressObject;

/** Part of a StressObject (or of the hub): replaced through PartSlot/PartMap, appended to PartList. */
class StressPart final : public OwnedPart {
public:
	StressPart(const RefCounted& owner, int32_t ownerRank, int64_t serial) noexcept : OwnedPart(owner), ownerRank(ownerRank), serial(serial) {
		stressCounters().partsCreated.fetch_add(1, std::memory_order_relaxed);
	}
	/** defined after StressObject (the Field<Ref<StressObject>> member releases a complete type) */
	~StressPart() override;

	bool intact() const noexcept { return canary == (PART_MAGIC ^ static_cast<uint64_t>(serial)); }

	/** rank of the owner (RANKS for the hub): refs must point to lower ranks */
	const int32_t ownerRank;
	const int64_t serial;
	Field<int64_t> value{0};
	Field<Ref<StressObject>> ref;
	SelfOrRef<StressObject> actor{static_cast<const OwnedPart&>(*this)};

private:
	uint64_t canary = PART_MAGIC ^ static_cast<uint64_t>(serial);
};

/** Late-bound controller part (design §3.2.1 pattern 3): default-constructed, bound in the owner's constructor. */
class StressController final : public OwnedPart {
public:
	StressController() noexcept = default;
	void bind(const RefCounted& owner) noexcept { bindOwner(owner); }
	Field<int64_t> ticks{0};
};

/** The synthetic K4 game object. */
class StressObject final : public RefCounted {
	AION_MAKE_REF_FRIEND

public:
	static Ref<StressObject> create(int32_t id, int32_t rank) { return makeRef<StressObject>(id, rank); }

	bool intact() const noexcept { return canary == (OBJECT_MAGIC ^ static_cast<uint64_t>(static_cast<uint32_t>(id))); }

	const int32_t id;
	const int32_t rank;
	Field<int64_t> counter{0};
	Field<std::string> name;
	Field<Ref<StressObject>> link;
	SelfOrRef<StressObject> target{static_cast<const RefCounted&>(*this)};
	PartSlot<StressPart, RetireTo::RECLAIMER> part{*this};
	PartSlot<StressPart, RetireTo::OWNER> ownerPart{*this};
	Field<int32_t> ownerPartReplacements{0};
	const std::unique_ptr<StressController> controller;
	ArrayList<Ref<StressObject>> known{AION_LOCK_CLASS(StressObject::known)};
	ArrayList<int32_t> ids{AION_LOCK_CLASS(StressObject::ids)};
	ConcurrentHashMap<int32_t, FutureRef> tasks{AION_LOCK_CLASS(StressObject::tasks#stripe)};
	AtomicInteger hits{AION_LOCK_CLASS(StressObject::hits)};

protected:
	StressObject(int32_t id, int32_t rank) : id(id), rank(rank), controller(std::make_unique<StressController>()) {
		controller->bind(*this);
		part.set(std::make_unique<StressPart>(*this, rank, static_cast<int64_t>(id)));
		stressCounters().objectsCreated.fetch_add(1, std::memory_order_relaxed);
	}
	~StressObject() override {
		canary = DEAD_CANARY;
		stressCounters().objectsDestroyed.fetch_add(1, std::memory_order_relaxed);
		stressCounters().cleanerPushed.fetch_add(1, std::memory_order_relaxed);
		CleanerQueue::push(id, "StressObject"); // design §6: destructors push the id and do nothing else
	}

private:
	uint64_t canary = OBJECT_MAGIC ^ static_cast<uint64_t>(static_cast<uint32_t>(id));
};

inline StressPart::~StressPart() {
	canary = DEAD_CANARY;
	stressCounters().partsDestroyed.fetch_add(1, std::memory_order_relaxed);
}

using StressList = RcArrayList<Ref<StressObject>>;
using StressInnerMap = RcConcurrentHashMap<int32_t, Ref<StressObject>>;

/** All shared global structures of one harness run (a RefCounted owner, so its shims are destroyed only once unreachable). */
class StressHub final : public RefCounted {
	AION_MAKE_REF_FRIEND

public:
	static Ref<StressHub> create(int32_t slotCount, int32_t arrayLength) { return makeRef<StressHub>(slotCount, arrayLength); }

	bool intact() const noexcept { return canary == HUB_MAGIC; }

	const int32_t slotCount;
	const std::unique_ptr<Field<Ref<StressObject>>[]> slots;
	ConcurrentHashMap<int32_t, Ref<StressObject>> world{AION_LOCK_CLASS(StressHub::world#stripe)};
	ConcurrentHashMap<int32_t, Ref<StressList>> buckets{AION_LOCK_CLASS(StressHub::buckets#stripe)};
	ConcurrentHashMap<int32_t, Ref<StressInnerMap>> nested{AION_LOCK_CLASS(StressHub::nested#stripe)};
	ArrayList<Ref<StressObject>> list{AION_LOCK_CLASS(StressHub::list)};
	HashMap<int32_t, Ref<StressObject>> groups{AION_LOCK_CLASS(StressHub::groups)};
	HashSet<Ref<StressObject>> set{AION_LOCK_CLASS(StressHub::set)};
	CopyOnWriteArrayList<Ref<StressObject>> observers{AION_LOCK_CLASS(StressHub::observers)};
	ConcurrentLinkedQueue<Ref<StressObject>> queue{AION_LOCK_CLASS(StressHub::queue)};
	PartMap<int32_t, StressPart> partMap{*this};
	PartList<StressPart> partList{*this};
	const Ref<Array<Ref<StressObject>>> array;
	AtomicReference<Ref<StressObject>> atomicRef{AION_LOCK_CLASS(StressHub::atomicRef)};
	Semaphore semaphore{AION_LOCK_CLASS(StressHub::semaphore), 4};
	mutable Monitor contended{AION_LOCK_CLASS(StressHub::contended)};

protected:
	StressHub(int32_t slotCount, int32_t arrayLength)
		: slotCount(slotCount), slots(std::make_unique<Field<Ref<StressObject>>[]>(static_cast<size_t>(slotCount))),
		  array(Array<Ref<StressObject>>::make(arrayLength)) {
		stressCounters().hubsCreated.fetch_add(1, std::memory_order_relaxed);
	}
	~StressHub() override {
		canary = DEAD_CANARY;
		stressCounters().hubsDestroyed.fetch_add(1, std::memory_order_relaxed);
	}

private:
	uint64_t canary = HUB_MAGIC;
};

} // namespace aion::gameserver::runtime::stress
