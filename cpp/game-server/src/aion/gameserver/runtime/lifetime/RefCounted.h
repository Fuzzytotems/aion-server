#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <type_traits>

#include "aion/gameserver/runtime/base/Checked.h"
#include "aion/gameserver/runtime/sync/Monitor.h"

namespace aion::gameserver::runtime {

template <class T>
class Ref;
template <class T, class... A>
Ref<T> makeRef(A&&... args);
class Reclaimer;

namespace detail {
struct RefCountedAccess;
} // namespace detail

/**
 * Base of every shared (K4) game object: an atomic intrusive reference count reclaimed by the epoch Reclaimer (design §2.1, §2.2, §2.4).
 *
 * Construction and destruction
 * - The only way to construct a RefCounted is makeRef<T>(...) (generated classes forward from `static Ref<T> create(...)`). Subclasses have
 *   protected constructors and destructors and declare AION_MAKE_REF_FRIEND; `T object(...)` on the stack does not compile (RR-12).
 * - The count is 1 while the constructor runs (makeRef's reference, adopted by the returned Ref), so retaining and releasing `this` inside a
 *   constructor can never queue the object for reclamation.
 * - Only the Reclaimer deletes, once count == 0 and no task that could have borrowed the object is active (design §2.4). Destructors are
 *   noexcept and release-only (design §2.7): drop Refs, push ids to the CleanerQueue, log ids. In checked builds a dereference of a Ref/Ptr/
 *   Field inside the Reclaimer's destructor context terminates with class name and stack (C8).
 *
 * Reference count protocol (design §2.2, §2.4, with the correction below)
 * - retain(): fetch_add. 0 → 1 is legal (resurrection of an object that was borrowed before its last release).
 * - release(), loop: e = E.load(); c = count.load(); retireEpoch = max(retireEpoch, e) (CAS loop that only writes when the stamp is older than
 *   e, i.e. once per object per epoch); CAS count c → c-1; retry from the start if the count CAS fails. After a successful 1 → 0, if
 *   `!queued.exchange(true)` the object is pushed to the Reclaimer (Reclaimer::retire). Every step has an AION_YIELD_POINT.
 * - DEVIATION from the design's literal protocol ("while count > 1: CAS decrement, no stamp"): the stampless fast path is unsafe under count
 *   ABA. Thread X loads count 1 and E = e, thread Y (holding an older borrow) retains (2) and stores the object into location L, thread Z
 *   publishes a newer epoch and loads it from L, Y unlinks L and releases 2 → 1 without a stamp, X's CAS 1 → 0 succeeds with the stamp e read
 *   before Y's unlink, and a scan with m = e_Z > e frees the object under Z's borrow. Stamping in every release (before its CAS) restores the
 *   proof: every unlink is followed by its own release, whose stamp (E read after the unlink) is stored before that release's CAS and hence
 *   before the final 1 → 0. The common case stays a load of E plus a compare (the stamp CAS runs at most once per object per epoch).
 *   tests/runtime/lifetime/ProtocolMutationTest.cpp contains the counterexample (Mutation::DESIGN_STAMPLESS_FAST_PATH).
 * - retain()/release() are lock-free, noexcept and legal on every thread, registered or not, inside or outside a TaskScope.
 *
 * Checked builds (C3, C4, C5, C15)
 * - cookie: UNMANAGED (constructed outside makeRef) → ALIVE (set in this constructor when the object is being created by makeRef) → DEAD (set
 *   by the Reclaimer before destruction). retain() on UNMANAGED (a stack or `new`-allocated object) terminates with the class name (C15);
 *   retain()/release() on DEAD (use after free) terminate (C4). Count overflow and underflow terminate (C4).
 * - makeRef allocates a 16-byte header in front of each object; the Reclaimer poisons freed memory (keeping the DEAD cookie readable) and
 *   delays frees through a 64 MB FIFO (C3). ASan builds free immediately and rely on ASan's quarantine instead.
 * - The destructor terminates (C5) if an object that the Reclaimer did not destroy is still referenced or queued (e.g. a constructor that
 *   threw after publishing `this`).
 *
 * Every object also carries its Java monitor: `SYNCHRONIZED(*this)` / `monitorOf(object)` (design §3.4).
 * Memory: vptr + count + queued + retireEpoch + Monitor ≈ +40 bytes (checked builds +4 for the cookie).
 */
class RefCounted {
public:
	RefCounted(const RefCounted&) = delete;
	RefCounted& operator=(const RefCounted&) = delete;

	/** Increments the count. Checked: terminates if the cookie is not ALIVE (C15/C4) or on overflow. */
	void retain() const noexcept;
	/** Decrements the count following the protocol above; the last release stamps the epoch and queues the object for reclamation. */
	void release() const noexcept;

	/** The object's Java monitor (`synchronized (this)`), lock class = dynamic type (design §3.4). */
	Monitor& monitor() const noexcept { return monitor_; }

	/** Racy snapshot of the reference count; exact only when no other thread touches the object (tests, leak census on the Reclaimer). */
	uint32_t refCount() const noexcept { return count.load(std::memory_order_acquire); }

	/** Checked builds: true if created by makeRef and not yet destroyed. Release builds: always true. */
	bool isManaged() const noexcept;

protected:
	/**
	 * If this object is the first RefCounted constructed inside the memory that makeRef is constructing on this thread, the count starts at 1
	 * (the reference makeRef's Ref adopts) and, in checked builds, the cookie is set to ALIVE. Otherwise (a RefCounted data member of a makeRef
	 * object, a stack object) the count starts at 0 and the cookie stays UNMANAGED.
	 */
	RefCounted() noexcept;
	/**
	 * Protected in every subclass (generated); only the Reclaimer deletes. Checked: if the Reclaimer did not destroy the object (a constructor
	 * threw, or an unmanaged object), terminates when anything beyond makeRef's own initial reference still holds or queued it (C5).
	 */
	virtual ~RefCounted();

private:
	friend class Reclaimer;
	friend struct detail::RefCountedAccess;
	template <class T, class... A>
	friend Ref<T> makeRef(A&&... args);

	mutable std::atomic<uint32_t> count{0};
	mutable std::atomic<bool> queued{false};
	mutable std::atomic<uint64_t> retireEpoch{0};
	mutable Monitor monitor_;
#if AION_CHECKED
public:
	enum class Cookie : uint32_t { UNMANAGED = 0x554E4D47, ALIVE = 0xA11FEA11, DEAD = 0xDEADDEAD };

private:
	mutable std::atomic<Cookie> cookie{Cookie::UNMANAGED};
#endif
};

/**
 * Base of static-storage singletons, quest handlers and commands that live for the whole process (design §2.1, §5, conventions: Immortal only
 * for those; per-run service objects are RefCounted, lint L13).
 *
 * Immortals are never reclaimed; references to them are plain pointers/references and they may be captured by tasks (IsImmortalPtr) and pinned
 * (Pin checks registration, C10). The constructor registers the address in checked builds; the destructor unregisters it (static destruction
 * after quick_exit does not run, design §11). Not copyable.
 */
class Immortal {
public:
	Immortal(const Immortal&) = delete;
	Immortal& operator=(const Immortal&) = delete;

	/** The object's Java monitor, lock class = dynamic type. */
	Monitor& monitor() const noexcept { return monitor_; }

	/** Checked builds: true if `object` is a constructed, not yet destroyed Immortal. Release builds: true for non-null. Thread-safe. */
	static bool isRegistered(const Immortal* object) noexcept;

protected:
	Immortal() noexcept;
	~Immortal();

private:
	mutable Monitor monitor_;
};

/**
 * Marker base of immutable static data templates (K1, design §9): `const T*` template pointers are immortal, may be captured by tasks
 * (IsTemplatePtr) and pinned without retaining. Generated template classes derive it; alternatively specialize IsStaticTemplate<T>.
 * A class deriving both RefCounted and StaticTemplate (PlayerCommonData through CreatureTemplate) is a mutable RefCounted object, not a
 * template: IsStaticTemplate is false for it, so its pointer is no TaskArg and Pin retains it.
 */
struct StaticTemplate {};

template <class T>
struct IsStaticTemplate : std::bool_constant<std::is_base_of_v<StaticTemplate, T> && !std::is_base_of_v<RefCounted, T>> {};

/**
 * Base of objects the Reclaimer can destroy after an epoch without an object reference count: replaced parts (Reclaimer::retirePart; an
 * OwnedPart additionally counts Refs to the part, see Parts.h) and runtime nodes (Reclaimer::retireNode: ConcurrentHashMap node tables,
 * CopyOnWriteArrayList arrays, Field<std::string> boxes).
 * Destructors follow the RefCounted destructor rules (noexcept, release-only).
 */
class OwnedPartBase {
public:
	virtual ~OwnedPartBase() = default;

protected:
	OwnedPartBase() noexcept = default;
	OwnedPartBase(const OwnedPartBase&) = default;
	OwnedPartBase& operator=(const OwnedPartBase&) = default;
};

/** Base of runtime-internal memory reclaimed by epoch (see Reclaimer::retireNode). Destructors are noexcept and release-only. */
class RetiredNode {
public:
	virtual ~RetiredNode() = default;
	/** approximate heap bytes owned by the node, for Reclaimer::Stats::backlogBytes */
	virtual size_t retiredBytes() const noexcept { return 0; }

protected:
	RetiredNode() noexcept = default;
	RetiredNode(const RetiredNode&) = default;
	RetiredNode& operator=(const RetiredNode&) = default;
};

namespace detail {

/**
 * Memory for a RefCounted object of `size` bytes (makeRef). Checked builds prepend an ObjectHeader (detail/RefCountedAccess.h). Throws
 * std::bad_alloc. The Reclaimer frees the memory after destruction.
 */
void* allocateObject(size_t size);
/** Frees memory from allocateObject whose constructor threw (the object was never published). */
void freeObjectMemory(void* memory) noexcept;

/**
 * Terminates (C15, all builds) unless `object`'s RefCounted base was constructed inside makeRef's ManagedConstruction (count 1 held for the
 * first Ref; checked builds also verify the cookie is ALIVE). Fails when a RefCounted data member or a RefCounted in an earlier base consumed
 * the managed range first.
 */
void checkConstructedByMakeRef(const RefCounted& object) noexcept;

/**
 * Marks the memory range of an object that makeRef is about to construct on this thread, so that the first RefCounted constructor running
 * inside it takes the initial count 1 for makeRef's Ref (checked builds: and sets the cookie to ALIVE). Nestable (a constructor may call
 * makeRef). All builds.
 */
class ManagedConstruction {
public:
	ManagedConstruction(void* memory, size_t size) noexcept;
	~ManagedConstruction();
	ManagedConstruction(const ManagedConstruction&) = delete;
	ManagedConstruction& operator=(const ManagedConstruction&) = delete;

private:
	void* previousMemory;
	size_t previousSize;
};

} // namespace detail

} // namespace aion::gameserver::runtime

/**
 * Grants makeRef access to protected constructors of a RefCounted subclass. Generated classes put it in their class body:
 * <pre>
 * class PlayerTeamMember : public RefCounted, public TeamMember<Player> {
 *     AION_MAKE_REF_FRIEND
 * protected:
 *     explicit PlayerTeamMember(Player& player);
 *     ~PlayerTeamMember() override = default;
 * public:
 *     static Ref<PlayerTeamMember> create(Player& player) { return makeRef<PlayerTeamMember>(player); }
 * };
 * </pre>
 */
#define AION_MAKE_REF_FRIEND                                                                                                                        \
	template <class AionMakeRefT, class... AionMakeRefA>                                                                                              \
	friend ::aion::gameserver::runtime::Ref<AionMakeRefT>(::aion::gameserver::runtime::makeRef)(AionMakeRefA&&...);
