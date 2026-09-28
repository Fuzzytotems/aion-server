#pragma once

#include <type_traits>
#include <utility>

#include "aion/gameserver/runtime/base/Checked.h"

namespace aion::gameserver::runtime {

/**
 * Java `final` (or effectively final) member that cannot be initialized in the C++ member-initializer list (design §3.2): assigned exactly
 * once during construction, before the object is published, then read without synchronization (publication is the happens-before edge).
 * Used for late-bound part owners (`Final<Npc*> owner`, design §3.2.1 pattern 3) and values computed in the constructor body.
 *
 * Checked builds (C11): a second set() terminates; get() before set() terminates. Pass the owning object to set(value, owner) to also check
 * that the owner was not published yet (count <= 1), which catches assignments after the object escaped.
 * Thread-safety: set() only during construction (single-threaded); get() is thread-safe after publication.
 */
template <class T>
class Final {
public:
	constexpr Final() noexcept(std::is_nothrow_default_constructible_v<T>) = default;
	constexpr Final(T value) noexcept(std::is_nothrow_move_constructible_v<T>) : value_(std::move(value)) {
#if AION_CHECKED
		assigned_ = true;
#endif
	}
	Final(const Final&) = delete;
	Final& operator=(const Final&) = delete;

	/** The single assignment (checked: C11 once). */
	void set(T value) {
#if AION_CHECKED
		AION_CHECK("C11", !assigned_, "Final<T> assigned twice");
		assigned_ = true;
#endif
		value_ = std::move(value);
	}

	/** The single assignment, checking that `owner` (a RefCounted or anything with refCount()) is not yet published (checked: C11). */
	template <class Owner>
	void set(T value, const Owner& owner) {
		AION_CHECK("C11", owner.refCount() <= 1, "Final<T> assigned after the owner was published");
		set(std::move(value));
	}

	const T& get() const noexcept {
#if AION_CHECKED
		AION_CHECK("C11", assigned_, "Final<T> read before it was assigned");
#endif
		return value_;
	}
	operator const T&() const noexcept { return get(); }

	/** pointer-typed finals: member access through the stored pointer */
	T operator->() const noexcept
		requires std::is_pointer_v<T>
	{
		return get();
	}
	std::add_lvalue_reference_t<std::remove_pointer_t<T>> operator*() const noexcept
		requires std::is_pointer_v<T>
	{
		return *get();
	}

private:
	T value_{};
#if AION_CHECKED
	bool assigned_ = false;
#endif
};

} // namespace aion::gameserver::runtime
