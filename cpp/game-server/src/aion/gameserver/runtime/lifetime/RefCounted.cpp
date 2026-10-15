// RefCounted reference count protocol, cookies (C4, C15), managed allocation (makeRef) and the Immortal registry.
// The protocol is documented in RefCounted.h; the scan side lives in Reclaimer.cpp. Mutation switches: detail/Mutations.h.

#include "aion/gameserver/runtime/lifetime/RefCounted.h"

#include <mutex>
#include <new>
#include <string>
#include <unordered_set>

#include "aion/commons/utils/ClassName.h"
#include "aion/gameserver/runtime/base/YieldPoint.h"
#include "aion/gameserver/runtime/lifetime/Reclaimer.h"
#include "aion/gameserver/runtime/lifetime/detail/Epoch.h"
#include "aion/gameserver/runtime/lifetime/detail/Mutations.h"
#include "aion/gameserver/runtime/lifetime/detail/RefCountedAccess.h"

namespace aion::gameserver::runtime {

namespace detail {
#if AION_LIFETIME_MUTATIONS
std::atomic<Mutation> activeMutation{Mutation::NONE};
#endif
} // namespace detail

namespace {

/** Memory range of the object makeRef is constructing on this thread (checked builds); consumed by the first RefCounted constructed in it. */
thread_local void* pendingManagedMemory = nullptr;
thread_local size_t pendingManagedSize = 0;

std::string className(const std::type_info& type) {
	try {
		return commons::utils::getClassName(type);
	} catch (...) {
		return "<unknown class>";
	}
}

} // namespace

// ------------------------------------------------------------------------------------------------------------------------------ RefCounted

RefCounted::RefCounted() noexcept {
	auto* self = static_cast<char*>(static_cast<void*>(this));
	auto* memory = static_cast<char*>(pendingManagedMemory);
	if (memory != nullptr && self >= memory && self < memory + pendingManagedSize) {
		// the reference makeRef's Ref adopts: the count never reaches 0 while the constructor runs, whatever it retains and releases
		count.store(1, std::memory_order_relaxed);
#if AION_CHECKED
		cookie.store(Cookie::ALIVE, std::memory_order_relaxed);
#endif
		pendingManagedMemory = nullptr; // a RefCounted data member of this object stays unmanaged
		pendingManagedSize = 0;
	}
}

RefCounted::~RefCounted() {
#if AION_CHECKED
	// DEAD: destroyed by the Reclaimer after it verified count == 0. ALIVE: destroyed by makeRef after its constructor threw; only makeRef's
	// initial reference may remain. UNMANAGED: a stack or member object that nobody may reference. Nothing may still queue the object.
	if (Cookie current = cookie.load(std::memory_order_relaxed); current != Cookie::DEAD) {
		const uint32_t allowed = current == Cookie::ALIVE ? 1 : 0;
		AION_CHECK("C5", count.load(std::memory_order_acquire) <= allowed && !queued.load(std::memory_order_acquire),
			"RefCounted destroyed outside the Reclaimer while referenced or queued (did a throwing constructor publish `this`?): " +
				className(typeid(*this)));
	}
#endif
}

bool RefCounted::isManaged() const noexcept {
#if AION_CHECKED
	return cookie.load(std::memory_order_relaxed) == Cookie::ALIVE;
#else
	return true;
#endif
}

#if AION_CHECKED
namespace {

[[noreturn]] void cookieFailure(const char* operation, RefCounted::Cookie cookie, const RefCounted& object) noexcept {
	if (cookie == RefCounted::Cookie::UNMANAGED) {
		checkFailed("C15", std::string(operation) + " on an object not created by makeRef (stack or unmanaged RefCounted): " + className(typeid(object)));
	}
	// DEAD (or garbage): the memory was destroyed (and poisoned), so the dynamic type is no longer available
	checkFailed("C4", std::string(operation) + " on a destroyed RefCounted (use after free)");
}

} // namespace
#endif

void RefCounted::retain() const noexcept {
#if AION_CHECKED
	if (Cookie current = cookie.load(std::memory_order_relaxed); current != Cookie::ALIVE) [[unlikely]]
		cookieFailure("retain()", current, *this);
#endif
	AION_YIELD_POINT("RefCounted::retain");
	[[maybe_unused]] uint32_t previous = count.fetch_add(1, std::memory_order_acq_rel);
	AION_CHECK("C4", previous != UINT32_MAX, "reference count overflow: " + className(typeid(*this)));
}

void RefCounted::release() const noexcept {
#if AION_CHECKED
	if (Cookie current = cookie.load(std::memory_order_relaxed); current != Cookie::ALIVE) [[unlikely]]
		cookieFailure("release()", current, *this);
#endif
	using detail::isMutated;
	using detail::Mutation;
	// retireEpoch = max(retireEpoch, e); only writes when the stamp is older than e
	auto stamp = [this](uint64_t epoch) noexcept {
		if (isMutated(Mutation::STAMP_OVERWRITE)) {
			AION_YIELD_POINT("RefCounted::release:stamp");
			retireEpoch.store(epoch, std::memory_order_release);
			return;
		}
		uint64_t stamped = retireEpoch.load(std::memory_order_acquire);
		while (stamped < epoch) {
			AION_YIELD_POINT("RefCounted::release:stamp");
			if (retireEpoch.compare_exchange_strong(stamped, epoch, std::memory_order_acq_rel))
				break;
		}
	};
	for (;;) {
		AION_YIELD_POINT("RefCounted::release:epoch");
		const uint64_t epoch = detail::globalEpoch.load(std::memory_order_acquire); // read after the caller's unlink
		AION_YIELD_POINT("RefCounted::release:load");
		uint32_t current = count.load(std::memory_order_acquire);
		AION_CHECK("C4", current != 0, "reference count underflow: " + className(typeid(*this)));
		const bool stampBeforeCas = !isMutated(Mutation::SKIP_RELEASE_STAMP) && !isMutated(Mutation::CAS_BEFORE_STAMP) &&
			!(current > 1 && isMutated(Mutation::DESIGN_STAMPLESS_FAST_PATH));
		if (stampBeforeCas)
			stamp(epoch);
		AION_YIELD_POINT("RefCounted::release:cas");
		if (isMutated(Mutation::NON_ATOMIC_DECREMENT)) {
			count.store(current - 1, std::memory_order_release);
		} else if (!count.compare_exchange_strong(current, current - 1, std::memory_order_acq_rel)) {
			continue;
		}
		if (current > 1)
			return;
		if (isMutated(Mutation::CAS_BEFORE_STAMP))
			stamp(epoch);
		AION_YIELD_POINT("RefCounted::release:queued");
		if (isMutated(Mutation::RELEASE_ALWAYS_PUSH) || !queued.exchange(true, std::memory_order_acq_rel))
			Reclaimer::retire(*this);
		return;
	}
}

// ------------------------------------------------------------------------------------------------------------------------------ Immortal

namespace {

struct ImmortalRegistry {
	std::mutex mutex; // plain mutex: registration runs during static initialization, no callbacks under it
	std::unordered_set<const Immortal*> objects;

	static ImmortalRegistry& get() {
		static auto* registry = new ImmortalRegistry(); // leaked: usable during static destruction
		return *registry;
	}
};

} // namespace

Immortal::Immortal() noexcept {
#if AION_CHECKED
	try {
		ImmortalRegistry& registry = ImmortalRegistry::get();
		std::scoped_lock lock(registry.mutex);
		registry.objects.insert(this);
	} catch (...) {
		checkFailed("C10", "Immortal registration: out of memory");
	}
#endif
}

Immortal::~Immortal() {
#if AION_CHECKED
	ImmortalRegistry& registry = ImmortalRegistry::get();
	std::scoped_lock lock(registry.mutex);
	registry.objects.erase(this);
#endif
}

bool Immortal::isRegistered(const Immortal* object) noexcept {
#if AION_CHECKED
	ImmortalRegistry& registry = ImmortalRegistry::get();
	std::scoped_lock lock(registry.mutex);
	return registry.objects.contains(object);
#else
	return object != nullptr;
#endif
}

// ------------------------------------------------------------------------------------------------------------------------------ detail

namespace detail {

void checkConstructedByMakeRef(const RefCounted& object) noexcept {
#if AION_CHECKED
	if (RefCounted::Cookie current = RefCountedAccess::cookie(object).load(std::memory_order_relaxed); current != RefCounted::Cookie::ALIVE)
		checkFailed("C15", "makeRef: the object's RefCounted base was not constructed as the managed object (a RefCounted member or base "
						   "consumed the managed range first): " + className(typeid(object)));
#endif
	if (object.refCount() == 0) [[unlikely]]
		checkFailed("C15", "makeRef: the object's RefCounted base was not constructed as the managed object: " + className(typeid(object)));
}

ManagedConstruction::ManagedConstruction(void* memory, size_t size) noexcept : previousMemory(pendingManagedMemory), previousSize(pendingManagedSize) {
	pendingManagedMemory = memory;
	pendingManagedSize = size;
}

ManagedConstruction::~ManagedConstruction() {
	pendingManagedMemory = previousMemory;
	pendingManagedSize = previousSize;
}

void* allocateObject(size_t size) {
#if AION_CHECKED
	void* raw = ::operator new(size + sizeof(ObjectHeader));
	auto* header = static_cast<ObjectHeader*>(raw);
	header->size = size;
	header->magic = OBJECT_HEADER_MAGIC;
	return header + 1;
#else
	return ::operator new(size);
#endif
}

void freeObjectMemory(void* memory) noexcept {
#if AION_CHECKED
	::operator delete(static_cast<ObjectHeader*>(memory) - 1);
#else
	::operator delete(memory);
#endif
}

} // namespace detail

} // namespace aion::gameserver::runtime
