#pragma once

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <type_traits>
#include <utility>

#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/runtime/collections/JavaEquals.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/TaskScope.h"
#include "aion/gameserver/runtime/sync/Monitor.h"

/**
 * Internal helpers shared by the collection shims (not part of the porting API).
 *
 * - Read barrier: shims whose elements are Refs hand out Ptrs, which is a pointer load (design §2.4), so they call TaskScope::ensurePublished()
 *   before reading. Shims of values (numbers, strings, template pointers) hand out copies and need no publication.
 * - Recursive update detection (design §3.3, RR-1): compute-like operations push a ComputeFrame on a thread-local stack while their callback
 *   runs. Every structural write first checks the stack: a write to a key whose callback is running on this thread throws
 *   IllegalStateException("Recursive update"). Only the thread holding the collection's (stripe) Monitor can run such a callback, so the
 *   thread-local stack sees every relevant frame.
 * - Busy guard: while the shim itself traverses its storage and calls user code that is not a documented callback (equals/hashCode/compareTo
 *   of elements, sort comparators, heap comparators), a structural modification of the same collection from that user code would invalidate
 *   the traversal. Such modifications throw IllegalStateException instead of corrupting memory (Java would throw
 *   ConcurrentModificationException or corrupt the structure). Reads stay allowed.
 */
namespace aion::gameserver::runtime::detail {

template <class T>
struct IsOptionalType : std::false_type {};
template <class T>
struct IsOptionalType<std::optional<T>> : std::true_type {};
template <class T>
struct IsSharedPtrType : std::false_type {};
template <class T>
struct IsSharedPtrType<std::shared_ptr<T>> : std::true_type {};

/** Types with a Java null state: Ref, Ptr, raw pointers, shared_ptr, optional, nullptr_t. */
template <class T>
concept NullableKind = IsRef<T> || IsPtr<T> || std::is_pointer_v<std::remove_cvref_t<T>> || std::is_null_pointer_v<std::remove_cvref_t<T>> ||
	IsSharedPtrType<std::remove_cvref_t<T>>::value || IsOptionalType<std::remove_cvref_t<T>>::value;

/** true if `value` is Java null (only for NullableKind types; values are never null). */
template <class T>
bool isNull(const T& value) noexcept {
	using D = std::remove_cvref_t<T>;
	if constexpr (std::is_null_pointer_v<D>)
		return true;
	else if constexpr (IsOptionalType<D>::value)
		return !value.has_value();
	else if constexpr (NullableKind<D>)
		return !static_cast<bool>(value);
	else
		return false;
}

/** Read barrier for a shim whose stored element type is T (design §2.4). */
template <class T>
inline void readBarrier() noexcept {
	if constexpr (IsRef<T>)
		TaskScope::ensurePublished();
}

/** Borrowed view of a stored element without copying values: `const T&` when Borrowed<T> is T, else a Borrowed<T> (Ptr) temporary. */
template <class T>
decltype(auto) borrowOf(const T& stored) noexcept(std::is_same_v<Borrowed<T>, T> || IsRef<T>) {
	if constexpr (std::is_same_v<Borrowed<T>, T>)
		return (stored);
	else
		return Borrowed<T>(stored);
}

/** Identity of two borrowed elements (JavaIterator::remove, Field-like identity checks): address for references, == for values. */
template <class E>
bool identical(const E& a, const E& b) {
	if constexpr (IsPtr<E> || IsRef<E>)
		return rawAddress(a) == rawAddress(b);
	else if constexpr (std::is_pointer_v<E>)
		return a == b;
	else if constexpr (std::equality_comparable<E>)
		return a == b;
	else
		return true; // values without operator== cannot be told apart: the position (or map key) decides
}

/** Java equals on stored elements (T) against a borrowed element. */
template <class T>
bool javaEquals(const T& stored, const Borrowed<T>& other) {
	return JavaEquality<T>::equals(borrowOf(stored), other);
}

/** Natural ordering is available for T (JavaOrdering<T> compiles). */
template <class T>
using BorrowedPointee = typename PointeeOf<std::remove_cvref_t<Borrowed<T>>>::type;

template <class T>
concept NaturallyOrderedValue = HasJavaCompareTo<Borrowed<T>> || requires(const Borrowed<T>& a, const Borrowed<T>& b) {
	{ a < b } -> std::convertible_to<bool>;
};

template <class T>
concept NaturallyOrdered = (std::is_void_v<BorrowedPointee<T>> && NaturallyOrderedValue<T>) ||
	(!std::is_void_v<BorrowedPointee<T>> && HasJavaCompareTo<BorrowedPointee<T>>);

[[noreturn]] void throwClassCastNoNaturalOrder(const char* shim);

/** Compares two borrowed elements with the comparator if set, else with natural ordering. */
template <class T>
int32_t compareElements(const JavaComparator<T>& comparator, const Borrowed<T>& a, const Borrowed<T>& b) {
	if (comparator)
		return comparator(a, b);
	if constexpr (NaturallyOrdered<T>)
		return JavaOrdering<T>::compare(a, b);
	else
		throwClassCastNoNaturalOrder("sorted collection");
}

/** Converts a callback result (Ref, Ptr, raw pointer, shared_ptr, optional, nullptr or a plain value) into an optional stored value. */
template <class V, class R>
std::optional<V> toStored(R&& result) {
	using D = std::remove_cvref_t<R>;
	if constexpr (std::is_null_pointer_v<D>) {
		return std::nullopt;
	} else if constexpr (IsOptionalType<D>::value) {
		if (!result.has_value())
			return std::nullopt;
		return std::optional<V>(std::in_place, *std::forward<R>(result));
	} else if constexpr (NullableKind<D>) {
		if (!static_cast<bool>(result))
			return std::nullopt;
		return std::optional<V>(std::in_place, std::forward<R>(result));
	} else {
		return std::optional<V>(std::in_place, std::forward<R>(result));
	}
}

// ---- exceptions (Collections.cpp)
[[noreturn]] void throwIndexOutOfBounds(int64_t index, int64_t size);
[[noreturn]] void throwNoSuchElement(const char* what);
[[noreturn]] void throwRecursiveUpdate();
[[noreturn]] void throwModifiedFromCallback();
[[noreturn]] void throwNullElement(const char* what);

// ---- recursive update detection

/** One running compute-like callback on this thread. */
struct ComputeFrame {
	const void* container;
	const void* key;
	/** (container, frame key, other key) -> same key for this container */
	bool (*sameKey)(const void* container, const void* frameKey, const void* otherKey);
	ComputeFrame* previous;
};

/** The calling thread's innermost frame (thread-local, Collections.cpp). */
ComputeFrame*& computeFrameTop() noexcept;

class ComputeFrameScope {
public:
	ComputeFrameScope(const void* container, const void* key, bool (*sameKey)(const void*, const void*, const void*)) noexcept
		: frame{container, key, sameKey, computeFrameTop()} {
		computeFrameTop() = &frame;
	}
	~ComputeFrameScope() { computeFrameTop() = frame.previous; }
	ComputeFrameScope(const ComputeFrameScope&) = delete;
	ComputeFrameScope& operator=(const ComputeFrameScope&) = delete;

private:
	ComputeFrame frame;
};

/** @throws IllegalStateException("Recursive update") if a callback for the same key of `container` is running on this thread */
inline void checkNotRecursiveUpdate(const void* container, const void* key) {
	for (ComputeFrame* frame = computeFrameTop(); frame != nullptr; frame = frame->previous) [[unlikely]] {
		if (frame->container == container && frame->sameKey(container, frame->key, key))
			throwRecursiveUpdate();
	}
}

// ---- busy guard

class BusyScope {
public:
	explicit BusyScope(int32_t& counter) noexcept : counter(counter) { ++counter; }
	~BusyScope() { --counter; }
	BusyScope(const BusyScope&) = delete;
	BusyScope& operator=(const BusyScope&) = delete;

private:
	int32_t& counter;
};

inline void checkNotBusy(int32_t busy) {
	if (busy != 0) [[unlikely]]
		throwModifiedFromCallback();
}

// ---- locking

/** Locks a shim Monitor, attributing it to its static lock class (every shim Monitor has one). */
class [[nodiscard]] ShimLock {
public:
	explicit ShimLock(Monitor& monitor) : guard(monitorOf(monitor)) {}
	ShimLock(Monitor& monitor, const LockClass& lockClass) : guard(MonitorHandle{monitor, lockClass}) {}

private:
	MonitorGuard guard;
};

// ---- callback shapes

/** compute: (const Key&, Nullable<V>) or (Nullable<V>) */
template <class K, class V, class F>
decltype(auto) invokeRemapping(F& function, const Borrowed<K>& key, const Nullable<V>& old) {
	if constexpr (std::invocable<F&, const Borrowed<K>&, const Nullable<V>&>)
		return std::invoke(function, key, old);
	else
		return std::invoke(function, old);
}

/** computeIfAbsent: (const Key&) or () */
template <class K, class F>
decltype(auto) invokeMapping(F& function, const Borrowed<K>& key) {
	if constexpr (std::invocable<F&, const Borrowed<K>&>)
		return std::invoke(function, key);
	else
		return std::invoke(function);
}

/** computeIfPresent: (const Key&, Value) or (Value) */
template <class K, class V, class F>
decltype(auto) invokePresentRemapping(F& function, const Borrowed<K>& key, const Borrowed<V>& value) {
	if constexpr (std::invocable<F&, const Borrowed<K>&, const Borrowed<V>&>)
		return std::invoke(function, key, value);
	else
		return std::invoke(function, value);
}

/** Converts elements of a foreign range (T, Borrowed<T>, anything convertible) to T. */
template <class T, class U>
T toElement(U&& value) {
	return T(std::forward<U>(value));
}

} // namespace aion::gameserver::runtime::detail
