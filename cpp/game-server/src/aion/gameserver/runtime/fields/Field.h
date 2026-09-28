#pragma once

#include <atomic>
#include <concepts>
#include <cstddef>
#include <memory>
#include <string>
#include <type_traits>
#include <utility>

#include "aion/gameserver/runtime/base/YieldPoint.h"
#include "aion/gameserver/runtime/lifetime/Reclaimer.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/TaskScope.h"

namespace aion::gameserver::runtime {

/**
 * Non-final / volatile member of a shared (K4) class (design §3.2 field table, §3.2.3). Chosen mechanically by fieldmap.py, never by hand.
 *
 * Variants
 * - Field<T> for trivially copyable values (numbers, enums, bool, std::optional<scalar>, value structs, template pointers `const X*`, sibling
 *   part pointers `Sib*`): get/set/conversion/assignment; relaxed compound assignment and ++/-- for arithmetic types (a load and a store:
 *   lost updates are possible as in Java, torn values are not). No read barrier (no pointer to a reclaimable object is loaded).
 * - Field<Ref<X>>: see the specialization below (pointer load with read barrier, identity compareAndSet).
 * - Field<std::string>: immutable string boxes reclaimed by epoch; get() returns a borrow valid until the task ends.
 * - Field<std::shared_ptr<X>>: connections (design §3.2); atomic shared_ptr semantics.
 *
 * Every Java re-read is a separate load (`if (boss && !boss->isDead())` loads twice, like Java). Fields are neither copyable nor movable
 * (they are object state). Thread-safety: all members are thread-safe; every load/store has an AION_YIELD_POINT for PCT tests.
 */
template <class T>
class Field {
public:
	static_assert(std::is_trivially_copyable_v<T>, "Field<T>: T must be trivially copyable, Ref<X>, std::string or std::shared_ptr<X>");

	constexpr Field() noexcept = default;
	constexpr Field(T initial) noexcept : value_(initial) {}
	Field(const Field&) = delete;
	Field& operator=(const Field&) = delete;

	T get() const noexcept {
		AION_YIELD_POINT("Field::get");
		return value_.load(std::memory_order_acquire);
	}
	operator T() const noexcept { return get(); }

	void set(T value) noexcept {
		AION_YIELD_POINT("Field::set");
		value_.store(value, std::memory_order_release);
	}
	Field& operator=(T value) noexcept {
		set(value);
		return *this;
	}

	/** Stores `value` and returns the previous value atomically (Java AtomicReference.getAndSet). */
	T exchange(T value) noexcept {
		AION_YIELD_POINT("Field::exchange");
		return value_.exchange(value, std::memory_order_acq_rel);
	}

	/**
	 * Stores `value` if the current value equals `expected`, atomically (Java AtomicReference.compareAndSet for value types). Compares object
	 * representations like std::atomic (padding bits excluded): for floating point 0.0 and -0.0 differ and equal NaN bit patterns match.
	 */
	bool compareAndSet(T expected, T value) noexcept {
		AION_YIELD_POINT("Field::compareAndSet");
		return value_.compare_exchange_strong(expected, value, std::memory_order_acq_rel);
	}

	/** Relaxed compound assignment (Java `x += v` on a field: load, compute, store; lost updates as in Java). @return the stored value */
	T operator+=(T delta) noexcept
		requires std::is_arithmetic_v<T>
	{
		T next = static_cast<T>(get() + delta);
		set(next);
		return next;
	}
	T operator-=(T delta) noexcept
		requires std::is_arithmetic_v<T>
	{
		T next = static_cast<T>(get() - delta);
		set(next);
		return next;
	}
	T operator*=(T factor) noexcept
		requires std::is_arithmetic_v<T>
	{
		T next = static_cast<T>(get() * factor);
		set(next);
		return next;
	}
	T operator/=(T divisor) noexcept
		requires std::is_arithmetic_v<T>
	{
		T next = static_cast<T>(get() / divisor);
		set(next);
		return next;
	}
	T operator|=(T bits) noexcept
		requires std::is_integral_v<T>
	{
		T next = static_cast<T>(get() | bits);
		set(next);
		return next;
	}
	T operator&=(T bits) noexcept
		requires std::is_integral_v<T>
	{
		T next = static_cast<T>(get() & bits);
		set(next);
		return next;
	}
	T operator^=(T bits) noexcept
		requires std::is_integral_v<T>
	{
		T next = static_cast<T>(get() ^ bits);
		set(next);
		return next;
	}
	/** prefix ++ (Java `++x`): @return the new value */
	T operator++() noexcept
		requires std::is_arithmetic_v<T>
	{
		return *this += T{1};
	}
	/** postfix ++ (Java `x++`): @return the old value */
	T operator++(int) noexcept
		requires std::is_arithmetic_v<T>
	{
		T old = get();
		set(static_cast<T>(old + T{1}));
		return old;
	}
	T operator--() noexcept
		requires std::is_arithmetic_v<T>
	{
		return *this -= T{1};
	}
	T operator--(int) noexcept
		requires std::is_arithmetic_v<T>
	{
		T old = get();
		set(static_cast<T>(old - T{1}));
		return old;
	}

	/** pointer-typed fields (templates, siblings): member access through the loaded pointer */
	T operator->() const noexcept
		requires std::is_pointer_v<T>
	{
		return get();
	}

private:
	std::atomic<T> value_{};
};

/**
 * Non-final reference field `Field<Ref<X>>` (design §3.2.3): the Java `X field;` of a shared class.
 *
 * - get(), `->`, `*`, `operator bool`, conversion to Ptr<X> are pointer loads: they call TaskScope::ensurePublished() (read barrier, design
 *   §2.4) and return a borrow valid until the task ends. Checked builds (C2): loading outside a TaskScope terminates.
 * - `->`/`*` throw NullPointerException on null.
 * - set/assignment store a new reference (retain) and release the previous one (the last release stamps the epoch, so borrows of the old
 *   object stay valid). exchange returns the previous value as a Ref. compareAndSet compares identity (Java `==`), like AtomicReference.
 * - Destruction (owner destructor) releases the stored reference without dereferencing it.
 * Thread-safety: all members are thread-safe (lock-free).
 */
template <class X>
class Field<Ref<X>> {
public:
	constexpr Field() noexcept = default;
	Field(Ref<X> initial) noexcept : value_(initial.leak()) {}
	~Field() {
		if (X* stored = value_.load(std::memory_order_acquire); stored != nullptr)
			stored->release();
	}
	Field(const Field&) = delete;
	Field& operator=(const Field&) = delete;

	/** Pointer load with read barrier. */
	Ptr<X> get() const {
		TaskScope::ensurePublished();
		AION_YIELD_POINT("Field<Ref>::get");
		X* stored = value_.load(std::memory_order_acquire);
		return stored != nullptr ? Ptr<X>(*stored) : Ptr<X>();
	}
	operator Ptr<X>() const { return get(); }
	/** @throws NullPointerException */
	X* operator->() const { return get().operator->(); }
	/** @throws NullPointerException */
	X& operator*() const { return *get(); }
	explicit operator bool() const { return static_cast<bool>(get()); }

	template <class U>
		requires std::convertible_to<U*, X*>
	void set(const Ptr<U>& value) {
		(void)exchange(Ref<X>(value));
	}
	template <class U>
		requires std::convertible_to<U*, X*>
	void set(Ref<U> value) {
		(void)exchange(Ref<X>(std::move(value)));
	}
	void set(std::nullptr_t) { (void)exchange(Ref<X>()); }

	template <class U>
		requires std::convertible_to<U*, X*>
	Field& operator=(const Ptr<U>& value) {
		set(value);
		return *this;
	}
	template <class U>
		requires std::convertible_to<U*, X*>
	Field& operator=(Ref<U> value) {
		set(std::move(value));
		return *this;
	}
	Field& operator=(std::nullptr_t) {
		set(nullptr);
		return *this;
	}

	/** Stores `value` and returns the previous reference. */
	Ref<X> exchange(Ref<X> value) noexcept {
		AION_YIELD_POINT("Field<Ref>::exchange");
		return Ref<X>::adopt(value_.exchange(value.leak(), std::memory_order_acq_rel));
	}

	/** Stores `value` if the current value is identical to `expected` (Java AtomicReference.compareAndSet). */
	template <class U>
		requires std::convertible_to<U*, X*>
	bool compareAndSet(const Ptr<U>& expected, Ref<X> value) noexcept {
		AION_YIELD_POINT("Field<Ref>::compareAndSet");
		X* expectedRaw = expected.rawPointer();
		X* desired = value.get();
		if (value_.compare_exchange_strong(expectedRaw, desired, std::memory_order_acq_rel)) {
			(void)value.leak();
			if (expectedRaw != nullptr)
				expectedRaw->release();
			return true;
		}
		return false;
	}
	bool compareAndSet(std::nullptr_t, Ref<X> value) noexcept { return compareAndSet(Ptr<X>(), std::move(value)); }

	template <class U>
	friend bool operator==(const Field& field, const Ptr<U>& other) {
		return field.get() == other;
	}
	template <class U>
	friend bool operator==(const Field& field, const Ref<U>& other) {
		return field.get() == other;
	}
	friend bool operator==(const Field& field, std::nullptr_t) { return !field; }

private:
	std::atomic<X*> value_{nullptr};
};

namespace detail {
/** Immutable string box reclaimed by epoch (Field<std::string>). */
struct StringBox final : RetiredNode {
	explicit StringBox(std::string text) : value(std::move(text)) {}
	size_t retiredBytes() const noexcept override { return sizeof(StringBox) + value.capacity(); }
	const std::string value;
};
inline const std::string& emptyString() noexcept {
	static const std::string empty;
	return empty;
}
} // namespace detail

/**
 * Non-final String field (design §3.2): immutable boxes swapped atomically; the previous box is retired to the Reclaimer.
 * get() is a pointer load (read barrier) and returns a reference valid until the task ends (copy it to keep it longer).
 * The destructor deletes the current box directly: it runs only when the owning object is destroyed by the Reclaimer (no task can still
 * borrow from it) or for objects that are never shared.
 * Java null and "" are both represented as the empty string (CONVENTIONS: null strings). The rare field where null and "" differ keeps a
 * separate Field<bool> with a `// fieldmap: <reason>` waiver.
 * Thread-safety: all members are thread-safe (lock-free).
 */
template <>
class Field<std::string> {
public:
	Field() noexcept = default;
	Field(std::string initial) : box_(initial.empty() ? nullptr : new detail::StringBox(std::move(initial))) {}
	~Field() { delete box_.load(std::memory_order_acquire); }
	Field(const Field&) = delete;
	Field& operator=(const Field&) = delete;

	/** Pointer load with read barrier; the reference is valid until the task ends. */
	const std::string& get() const {
		TaskScope::ensurePublished();
		AION_YIELD_POINT("Field<string>::get");
		detail::StringBox* box = box_.load(std::memory_order_acquire);
		return box != nullptr ? box->value : detail::emptyString();
	}
	operator const std::string&() const { return get(); }

	void set(std::string value) {
		auto* next = value.empty() ? nullptr : new detail::StringBox(std::move(value));
		AION_YIELD_POINT("Field<string>::set");
		if (detail::StringBox* previous = box_.exchange(next, std::memory_order_acq_rel); previous != nullptr)
			Reclaimer::retireNode(std::unique_ptr<RetiredNode>(previous));
	}
	Field& operator=(std::string value) {
		set(std::move(value));
		return *this;
	}
	Field& operator=(const char* value) {
		set(value != nullptr ? std::string(value) : std::string());
		return *this;
	}

	friend bool operator==(const Field& field, std::string_view other) { return field.get() == other; }

private:
	std::atomic<detail::StringBox*> box_{nullptr};
};

/**
 * Connection field `Field<std::shared_ptr<AionConnection>>` (design §3.2): atomic shared_ptr semantics (C++20 std::atomic<std::shared_ptr>).
 * get() returns a new shared_ptr (keeps the connection alive for the caller). No read barrier: shared_ptr ownership is not epoch-based.
 * Thread-safety: all members are thread-safe.
 */
template <class X>
class Field<std::shared_ptr<X>> {
public:
	Field() noexcept = default;
	Field(std::shared_ptr<X> initial) noexcept : value_(std::move(initial)) {}
	Field(const Field&) = delete;
	Field& operator=(const Field&) = delete;

	std::shared_ptr<X> get() const noexcept {
		AION_YIELD_POINT("Field<shared_ptr>::get");
		return value_.load(std::memory_order_acquire);
	}
	operator std::shared_ptr<X>() const noexcept { return get(); }
	explicit operator bool() const noexcept { return get() != nullptr; }

	void set(std::shared_ptr<X> value) noexcept {
		AION_YIELD_POINT("Field<shared_ptr>::set");
		value_.store(std::move(value), std::memory_order_release);
	}
	Field& operator=(std::shared_ptr<X> value) noexcept {
		set(std::move(value));
		return *this;
	}
	Field& operator=(std::nullptr_t) noexcept {
		set(nullptr);
		return *this;
	}
	std::shared_ptr<X> exchange(std::shared_ptr<X> value) noexcept { return value_.exchange(std::move(value), std::memory_order_acq_rel); }
	bool compareAndSet(std::shared_ptr<X> expected, std::shared_ptr<X> value) noexcept {
		return value_.compare_exchange_strong(expected, std::move(value), std::memory_order_acq_rel);
	}

private:
	std::atomic<std::shared_ptr<X>> value_;
};

} // namespace aion::gameserver::runtime
