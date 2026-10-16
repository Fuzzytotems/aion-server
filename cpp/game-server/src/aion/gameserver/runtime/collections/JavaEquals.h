#pragma once

#include <compare>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <type_traits>

#include "aion/gameserver/runtime/lifetime/Ref.h"

namespace aion::gameserver::runtime {

/**
 * Java equality, hashing and natural ordering for collection shims (design §3.3 "Java equality", RR-7/RR-13).
 *
 * - A class whose Java class (or a superclass other than Object) overrides equals/hashCode declares
 *   `bool equals(const Base& other) const` and `int32_t hashCode() const` (fieldmap.json `hasEquals`; AionObject compares objectId).
 *   Collections of Ref<X>/Ptr<X>/X* whose X satisfies HasJavaEquals compare and hash through those methods (null-safe); otherwise identity.
 * - Natural ordering (TreeMap/TreeSet/PriorityQueue without comparator): `int32_t compareTo(const X& other) const` when present, otherwise
 *   operator<=> / operator< on values. Reference elements without compareTo have no natural order (ClassCastException in Java): using them
 *   without a comparator is a compile error.
 * - Values (numbers, enums, strings, structs) use operator== and std::hash (Java equals of boxed values and String).
 * - JavaIterator::remove and the identity-based APIs (Field::compareAndSet) always use identity.
 *
 * Customization: specialize JavaEquality<T> for element types with special rules.
 */
template <class X>
concept HasJavaEquals = requires(const X& a, const X& b) {
	{ a.equals(b) } -> std::convertible_to<bool>;
	{ a.hashCode() } -> std::convertible_to<int32_t>;
};

template <class X>
concept HasJavaCompareTo = requires(const X& a, const X& b) {
	{ a.compareTo(b) } -> std::convertible_to<int32_t>;
};

namespace detail {

template <class P>
struct PointeeOf {
	using type = void;
};
template <class X>
struct PointeeOf<Ref<X>> {
	using type = X;
};
template <class X>
struct PointeeOf<Ptr<X>> {
	using type = X;
};
template <class X>
struct PointeeOf<X*> {
	using type = X;
};

inline const void* rawAddress(const void* pointer) noexcept {
	return pointer;
}
template <class X>
const void* rawAddress(const Ptr<X>& pointer) noexcept {
	return pointer.rawPointer();
}
template <class X>
const void* rawAddress(const Ref<X>& pointer) noexcept {
	return pointer.get();
}

} // namespace detail

/**
 * Equality and hash of a borrowed element (Borrowed<T>) of a collection with element type T. Primary template: values and pointer-likes.
 */
template <class T>
struct JavaEquality {
	using Element = Borrowed<T>;

	static bool equals(const Element& a, const Element& b) {
		using Pointee = typename detail::PointeeOf<std::remove_cvref_t<Element>>::type;
		if constexpr (std::is_void_v<Pointee>) {
			return a == b;
		} else if constexpr (HasJavaEquals<Pointee>) {
			const void* rawA = detail::rawAddress(a);
			const void* rawB = detail::rawAddress(b);
			if (rawA == rawB)
				return true;
			if (rawA == nullptr || rawB == nullptr)
				return false;
			return static_cast<bool>((*a).equals(*b));
		} else {
			return detail::rawAddress(a) == detail::rawAddress(b);
		}
	}

	static size_t hash(const Element& value) {
		using Pointee = typename detail::PointeeOf<std::remove_cvref_t<Element>>::type;
		if constexpr (std::is_void_v<Pointee>) {
			return std::hash<Element>()(value);
		} else if constexpr (HasJavaEquals<Pointee>) {
			return detail::rawAddress(value) == nullptr ? 0 : static_cast<size_t>(static_cast<uint32_t>((*value).hashCode()));
		} else {
			return std::hash<const void*>()(detail::rawAddress(value));
		}
	}
};

/** Natural ordering of borrowed elements: compareTo when available, else <=> / <. Returns negative, zero or positive like Java. */
template <class T>
struct JavaOrdering {
	using Element = Borrowed<T>;

	static int32_t compare(const Element& a, const Element& b) {
		using Pointee = typename detail::PointeeOf<std::remove_cvref_t<Element>>::type;
		if constexpr (std::is_void_v<Pointee>) {
			if constexpr (HasJavaCompareTo<Element>)
				return a.compareTo(b);
			else
				return a < b ? -1 : (b < a ? 1 : 0);
		} else {
			static_assert(HasJavaCompareTo<Pointee>, "natural ordering of references requires compareTo; pass a comparator");
			return (*a).compareTo(*b);
		}
	}
};

/**
 * Comparator adapter: accepts Java-style comparators returning an int (negative/zero/positive) and C++-style "less" predicates returning bool.
 * @return negative, zero or positive
 */
template <class Comparator, class E>
int32_t invokeComparator(Comparator& comparator, const E& a, const E& b) {
	using Result = std::invoke_result_t<Comparator&, const E&, const E&>;
	if constexpr (std::same_as<std::remove_cvref_t<Result>, bool>)
		return comparator(a, b) ? -1 : (comparator(b, a) ? 1 : 0);
	else
		return static_cast<int32_t>(comparator(a, b));
}

/** Type-erased Java Comparator over borrowed elements, stored by sorted shims (TreeMap, TreeSet, PriorityQueue). */
template <class T>
using JavaComparator = std::function<int32_t(const Borrowed<T>&, const Borrowed<T>&)>;

} // namespace aion::gameserver::runtime
