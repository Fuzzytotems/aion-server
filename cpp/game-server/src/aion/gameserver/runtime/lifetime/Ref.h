#pragma once

#include <compare>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <new>
#include <optional>
#include <type_traits>
#include <typeinfo>
#include <utility>

#include "aion/gameserver/runtime/base/Checked.h"
#include "aion/gameserver/runtime/lifetime/LiveInstanceCounters.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"

namespace aion::gameserver::runtime {

template <class T>
class Ptr;
template <class T>
class Ref;

/** Types that Ref<T> can hold: RefCounted subclasses and OwnedPart subclasses (whose retain/release forward to the owner). */
template <class T>
concept Retainable = requires(const T& object) {
	object.retain();
	object.release();
};

namespace detail {

/** Checked builds: the calling thread's current scope id (0 outside scopes); release builds: 0. */
uint64_t currentScopeStamp() noexcept;
/**
 * Publication of a new borrow (defined in TaskScope.cpp): inside a TaskScope and outside the Reclaimer's destructor context, publishes the
 * calling thread's epoch if it is not published yet (same loop as TaskScope::ensurePublished, but without its C2/C8 terminations). Returns
 * currentScopeStamp(). Cheap when already published (one TLS load and two predicted branches).
 */
uint64_t borrowStamp() noexcept;
[[noreturn]] void throwNullPointer(const std::type_info& type);
/** C1: a Ptr is used in a different scope than the one it was created in */
[[noreturn]] void throwStaleBorrow(const std::type_info& type, uint64_t stamp, uint64_t currentStamp);
/** C8: terminates if the calling thread is running destructors on behalf of the Reclaimer */
void checkNotInDestructorContext(const std::type_info& type) noexcept;
[[noreturn]] void throwClassCast(const std::type_info& from, const std::type_info& to);

inline void checkDereference(const void* pointer, const std::type_info& type, [[maybe_unused]] uint64_t stamp) {
	if (pointer == nullptr) [[unlikely]]
		throwNullPointer(type);
#if AION_CHECKED
	if (uint64_t current = currentScopeStamp(); stamp != current) [[unlikely]]
		throwStaleBorrow(type, stamp, current);
	checkNotInDestructorContext(type);
#endif
}

} // namespace detail

/**
 * Borrowed reference (design §2.1): parameters, locals and return values. Never stored in fields, containers, captures or thread_locals
 * (lints L3, L5, L15).
 *
 * A Ptr is valid until the task scope it was created in ends: whatever it points to was reachable when the thread published its epoch, so
 * the Reclaimer will not free it before the scope exits (design §2.4). A Ptr can only be obtained after a pointer-loading operation (which
 * publishes) or from a Ref/T&/T* the caller already holds.
 *
 * Publication (correction of design §2.4 "a Ptr can only exist after publication"): the design lets only pointer loads publish, but a Ptr can
 * also be made from a Ref the thread owns (a captured or local Ref, the temporary returned by getAndSet/exchange/create) and outlive that Ref
 * (`Ptr<Future> t = task.getAndSet(nullptr);`, or `Ptr<Npc> p = npc; field.set(std::move(npc));`). Every constructor that creates a new
 * non-null borrow from a Ref, T& or T* therefore publishes the epoch first (detail::borrowStamp), while the source still holds its reference,
 * so the object's last release is stamped with an epoch >= the published one and the object stays alive until the scope ends. Ptr copies and
 * cast/as of a Ptr need no publication. A plain `T&` taken with `*ref` is protected only while `ref` holds the object; take a Ptr before giving
 * the Ref away. Outside TaskScopes nothing is published and nothing protects a Ptr (startup code before the first scope, tests).
 *
 * - Dereferencing a null Ptr (`*`, `->`) throws NullPointerException naming T.
 * - Checked builds (C1): the Ptr is stamped with TaskScope::currentScopeId() at creation; dereferencing it under a different scope id (after
 *   the scope ended, after quiescentPoint(), on another thread) throws IllegalStateException. JOIN ForkJoin helpers share the submitter's id.
 *   A Ptr created outside any scope carries stamp 0: it is usable outside scopes (tests, startup code before the first scope) but throws
 *   inside a later scope, exactly like a borrow that escaped its task (nothing protects it there).
 * - Checked builds (C8): dereferencing inside the Reclaimer's destructor context terminates.
 * - Comparison is identity (Java `==`); use `a->equals(*b)` for Java equals.
 * Copying is free (a pointer plus, in checked builds, the stamp). Not thread-safe to share between threads (and never needed: pass Refs).
 */
template <class T>
class Ptr {
public:
	using element_type = T;

	constexpr Ptr() noexcept = default;
	constexpr Ptr(std::nullptr_t) noexcept {}
	/** Borrow of an object the caller can already reach (a T& parameter, `*this`). Publishes (see above). */
	Ptr(T& object) noexcept : Ptr(std::addressof(object), detail::borrowStamp()) {}
	/** Borrow through a raw pointer the caller can already reach (e.g. `this`). Publishes if non-null (see above). */
	explicit Ptr(T* object) noexcept : Ptr(object, object != nullptr ? detail::borrowStamp() : detail::currentScopeStamp()) {}
	template <class U>
		requires std::convertible_to<U*, T*>
	Ptr(const Ptr<U>& other) noexcept : Ptr(other.p, other.stampValue()) {}
	/** Borrow of an object held by `ref` (also a temporary Ref): publishes if non-null (see above). */
	template <class U>
		requires std::convertible_to<U*, T*>
	Ptr(const Ref<U>& ref) noexcept : Ptr(ref.get(), ref.get() != nullptr ? detail::borrowStamp() : detail::currentScopeStamp()) {}

	/**
	 * The raw pointer (nullptr if null). Never throws NullPointerException; checked builds verify the stamp of non-null pointers (C1:
	 * IllegalStateException) and the destructor context (C8). Use rawPointer() for unchecked identity.
	 */
	T* get() const {
#if AION_CHECKED
		if (p != nullptr)
			detail::checkDereference(p, typeid(T), scope);
#endif
		return p;
	}
	/** @throws NullPointerException if null; checked: IllegalStateException on a stale stamp (C1) */
	T& operator*() const {
		detail::checkDereference(p, typeid(T), stampValue());
		return *p;
	}
	/** @throws NullPointerException if null; checked: IllegalStateException on a stale stamp (C1) */
	T* operator->() const {
		detail::checkDereference(p, typeid(T), stampValue());
		return p;
	}
	explicit operator bool() const noexcept { return p != nullptr; }

	/** Identity comparison without checks, usable in any context (containers of borrowed pointers, asserts). */
	T* rawPointer() const noexcept { return p; }

	template <class U>
	friend bool operator==(const Ptr& a, const Ptr<U>& b) noexcept {
		return a.p == b.rawPointer();
	}
	friend bool operator==(const Ptr& a, std::nullptr_t) noexcept { return a.p == nullptr; }
	template <class U>
	friend bool operator==(const Ptr& a, const Ref<U>& b) noexcept {
		return a.p == b.get();
	}
	template <class U>
	friend bool operator==(const Ptr& a, const U* b) noexcept {
		return a.p == b;
	}

private:
	template <class U>
	friend class Ptr;
	template <class To, class From>
	friend Ptr<To> cast(Ptr<From> from);
	template <class To, class From>
	friend Ptr<To> as(Ptr<From> from) noexcept;

	Ptr(T* object, [[maybe_unused]] uint64_t stamp) noexcept
		: p(object)
#if AION_CHECKED
			, scope(stamp)
#endif
	{
	}

	uint64_t stampValue() const noexcept {
#if AION_CHECKED
		return scope;
#else
		return 0;
#endif
	}

	T* p = nullptr;
#if AION_CHECKED
	uint64_t scope = 0;
#endif
};

/**
 * Owning reference (design §2.1, §2.2): fields (`const Ref<X>`, `Field<Ref<X>>`), container elements, task captures, packet members.
 *
 * - Copy = retain, destruction/reset = release (lock-free, noexcept, legal on any thread).
 * - Converting from a Ptr needs no read barrier (a Ptr exists only after publication). Converting from T& retains the object the caller can
 *   reach (explicit).
 * - `*`, `->` throw NullPointerException on null; checked builds terminate inside the Reclaimer's destructor context (C8). get() never throws.
 * - T may be a RefCounted subclass or an OwnedPart subclass (retains the part's owner). T may be incomplete where Ref<T> is only declared.
 * - Comparison and std::hash are identity (Java `==`, IdentityHashMap); the collection shims apply Java equals where the class defines it.
 * Thread-safety: like std::shared_ptr, a single Ref object must not be written by one thread while another reads it; shared mutable Refs are
 * Field<Ref<X>> (design §3.2).
 */
template <class T>
class Ref {
public:
	using element_type = T;

	constexpr Ref() noexcept = default;
	constexpr Ref(std::nullptr_t) noexcept {}
	explicit Ref(T& object) noexcept : p(std::addressof(object)) { p->retain(); }
	explicit Ref(T* object) noexcept : p(object) {
		if (p != nullptr)
			p->retain();
	}
	template <class U>
		requires std::convertible_to<U*, T*>
	Ref(const Ptr<U>& borrowed) noexcept : p(borrowed.rawPointer()) {
		if (p != nullptr)
			p->retain();
	}
	Ref(const Ref& other) noexcept : p(other.p) {
		if (p != nullptr)
			p->retain();
	}
	Ref(Ref&& other) noexcept : p(std::exchange(other.p, nullptr)) {}
	template <class U>
		requires std::convertible_to<U*, T*>
	Ref(const Ref<U>& other) noexcept : p(other.get()) {
		if (p != nullptr)
			p->retain();
	}
	template <class U>
		requires std::convertible_to<U*, T*>
	Ref(Ref<U>&& other) noexcept : p(other.leak()) {}

	~Ref() {
		if (p != nullptr)
			p->release();
	}

	Ref& operator=(const Ref& other) noexcept {
		Ref(other).swap(*this);
		return *this;
	}
	Ref& operator=(Ref&& other) noexcept {
		Ref(std::move(other)).swap(*this);
		return *this;
	}
	Ref& operator=(std::nullptr_t) noexcept {
		reset();
		return *this;
	}

	/** Adopts a pointer whose reference was already counted for this Ref (makeRef, Field::exchange). */
	[[nodiscard]] static Ref adopt(T* alreadyRetained) noexcept {
		Ref ref;
		ref.p = alreadyRetained;
		return ref;
	}
	/** Gives up ownership without releasing; the caller becomes responsible for one release(). */
	[[nodiscard]] T* leak() noexcept { return std::exchange(p, nullptr); }

	/** The raw pointer, nullptr if null. Never throws (identity checks, logging). */
	T* get() const noexcept { return p; }
	/** @throws NullPointerException if null; checked: terminates in destructor context (C8) */
	T& operator*() const {
		check();
		return *p;
	}
	/** @throws NullPointerException if null; checked: terminates in destructor context (C8) */
	T* operator->() const {
		check();
		return p;
	}
	explicit operator bool() const noexcept { return p != nullptr; }

	/** Borrow for the current task scope. */
	Ptr<T> borrow() const noexcept { return Ptr<T>(*this); }

	void reset() noexcept { Ref().swap(*this); }
	void swap(Ref& other) noexcept { std::swap(p, other.p); }

	template <class U>
	friend bool operator==(const Ref& a, const Ref<U>& b) noexcept {
		return a.p == b.get();
	}
	friend bool operator==(const Ref& a, std::nullptr_t) noexcept { return a.p == nullptr; }
	template <class U>
	friend bool operator==(const Ref& a, const U* b) noexcept {
		return a.p == b;
	}

private:
	void check() const {
		if (p == nullptr) [[unlikely]]
			detail::throwNullPointer(typeid(T));
#if AION_CHECKED
		detail::checkNotInDestructorContext(typeid(T));
#endif
	}

	T* p = nullptr;
};

/** Ref<X> */
template <class T>
struct IsRefType : std::false_type {};
template <class T>
struct IsRefType<Ref<T>> : std::true_type {};
template <class T>
concept IsRef = IsRefType<std::remove_cvref_t<T>>::value;

/** Ptr<X> */
template <class T>
struct IsPtrType : std::false_type {};
template <class T>
struct IsPtrType<Ptr<T>> : std::true_type {};
template <class T>
concept IsPtr = IsPtrType<std::remove_cvref_t<T>>::value;

/**
 * Borrowed<T> / Nullable<T>: how a container element or map value of type T is handed out (design §3.3).
 * - Ref<X>: Borrowed = Nullable = Ptr<X> (valid until the task ends, even after removal, thanks to epoch reclamation)
 * - raw pointers (templates, immortals, siblings), std::shared_ptr, std::weak_ptr: the pointer itself
 * - other values (numbers, enums, strings, value structs): Borrowed = T (a copy), Nullable = std::optional<T> (Java null for absent map values)
 */
template <class T>
struct BorrowTraits {
	using Borrowed = T;
	using Nullable = std::optional<T>;
};
template <class X>
struct BorrowTraits<Ref<X>> {
	using Borrowed = Ptr<X>;
	using Nullable = Ptr<X>;
};
template <class X>
struct BorrowTraits<X*> {
	using Borrowed = X*;
	using Nullable = X*;
};
template <class X>
struct BorrowTraits<std::shared_ptr<X>> {
	using Borrowed = std::shared_ptr<X>;
	using Nullable = std::shared_ptr<X>;
};
template <class X>
struct BorrowTraits<std::weak_ptr<X>> {
	using Borrowed = std::weak_ptr<X>;
	using Nullable = std::weak_ptr<X>;
};
template <class T>
using Borrowed = typename BorrowTraits<T>::Borrowed;
template <class T>
using Nullable = typename BorrowTraits<T>::Nullable;

/**
 * The only way to construct a RefCounted (design §2.2). Allocates with the global operator new (alignment up to
 * __STDCPP_DEFAULT_NEW_ALIGNMENT__; T must not define class-specific operator new/delete), constructs T inside a
 * detail::ManagedConstruction and returns the first Ref.
 *
 * The object's count is already 1 while its constructor runs (the RefCounted constructor sets it inside the ManagedConstruction; checked
 * builds also set the cookie ALIVE), and the returned Ref adopts that reference. So a temporary retain/release of `this` inside a constructor
 * (a Pin(this) handed to a task that finishes at once, a PartSlot<RECLAIMER>::set, a temporary Ref(*this)) never takes the count to 0, and the
 * Reclaimer can never destroy an object under construction (review correction: the design's makeRef retained only after construction).
 * Exceptions from T's constructor propagate; the memory is freed and nothing may have kept a reference (checked: C5).
 */
template <class T, class... A>
Ref<T> makeRef(A&&... args) {
	static_assert(std::is_base_of_v<RefCounted, T>, "makeRef<T>: T must derive from RefCounted");
	static_assert(alignof(T) <= __STDCPP_DEFAULT_NEW_ALIGNMENT__, "makeRef<T>: over-aligned RefCounted types are not supported");
	void* memory = detail::allocateObject(sizeof(T));
	T* object;
	try {
		detail::ManagedConstruction managed(memory, sizeof(T));
		object = ::new (memory) T(std::forward<A>(args)...);
	} catch (...) {
		detail::freeObjectMemory(memory);
		throw;
	}
	detail::checkConstructedByMakeRef(*object);
#if AION_CHECKED
	detail::countLiveInstanceCreated<T>(); // debug live-instance counters (LiveInstanceCounters.h)
#endif
	return Ref<T>::adopt(object);
}

/**
 * Java cast `(To) from` (design §2.2, §14.1 rule 7). A null source yields a null Ptr (Java casts null freely).
 * @throws ClassCastException if the object is not a To
 */
namespace detail {

/**
 * Java instanceof for a cast target: a final class that derives From non-virtually is recognized by comparing the dynamic type (typeid, a
 * vptr load and usually a pointer compare) instead of dynamic_cast, which costs ~200 ns per failed check on MSVC (SM_MOVE broadcast
 * `instanceof Player` hot path). Every other target uses dynamic_cast.
 */
template <class To, class From>
To* instanceOf(From* raw) noexcept {
	if constexpr (std::is_final_v<To> && std::is_polymorphic_v<From> && std::is_base_of_v<From, To> &&
		requires(From* pointer) { static_cast<To*>(pointer); }) {
		return typeid(*raw) == typeid(To) ? static_cast<To*>(raw) : nullptr;
	} else {
		return dynamic_cast<To*>(raw);
	}
}

} // namespace detail

template <class To, class From>
Ptr<To> cast(Ptr<From> from) {
	From* raw = from.rawPointer();
	if (raw == nullptr)
		return Ptr<To>();
	To* target = detail::instanceOf<To>(raw);
	if (target == nullptr) [[unlikely]]
		detail::throwClassCast(typeid(*raw), typeid(To));
	return Ptr<To>(target, from.stampValue());
}

template <class To, class From>
Ptr<To> cast(const Ref<From>& from) {
	return cast<To>(Ptr<From>(from));
}

template <class To, class From>
	requires std::is_polymorphic_v<From>
Ptr<To> cast(From& from) {
	return cast<To>(Ptr<From>(from));
}

/** Java `from instanceof To t ? t : null` (design §2.2). Never throws. */
template <class To, class From>
Ptr<To> as(Ptr<From> from) noexcept {
	From* raw = from.rawPointer();
	return Ptr<To>(raw != nullptr ? detail::instanceOf<To>(raw) : nullptr, from.stampValue());
}

template <class To, class From>
Ptr<To> as(const Ref<From>& from) noexcept {
	return as<To>(Ptr<From>(from));
}

template <class To, class From>
	requires std::is_polymorphic_v<From>
Ptr<To> as(From& from) noexcept {
	return as<To>(Ptr<From>(from));
}

} // namespace aion::gameserver::runtime

/** Identity hash (Java IdentityHashMap / Object.hashCode default). */
template <class T>
struct std::hash<aion::gameserver::runtime::Ref<T>> {
	size_t operator()(const aion::gameserver::runtime::Ref<T>& ref) const noexcept { return std::hash<T*>()(ref.get()); }
};

template <class T>
struct std::hash<aion::gameserver::runtime::Ptr<T>> {
	size_t operator()(const aion::gameserver::runtime::Ptr<T>& ptr) const noexcept { return std::hash<T*>()(ptr.rawPointer()); }
};
