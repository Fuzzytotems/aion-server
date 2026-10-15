#pragma once

#include <atomic>
#include <concepts>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>

#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/runtime/base/YieldPoint.h"
#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/sync/LockClass.h"
#include "aion/gameserver/runtime/sync/Monitor.h"

/**
 * java.util.concurrent.atomic shims with Java names and semantics (design §3.2 "final AtomicX → same-named shim", §3.4, RR-9).
 *
 * Every shim also carries a Monitor, because Java code synchronizes on atomics (`synchronized (isAggred)`, CaptainMuruganAI.java:23); its lock
 * class is the member's static class when constructed with AION_LOCK_CLASS(Owner::field), else the shim type name.
 * All operations are lock-free and thread-safe, with AION_YIELD_POINTs. update/accumulate functions may be called more than once under
 * contention (Java semantics) and must be side-effect free.
 * Shims constructed without a lock class report the dynamic shim type (LockClass::ofType, e.g. AtomicNumber<int>) to the lock-order validator.
 */
namespace aion::gameserver::runtime {

/** Java: AtomicBoolean */
class AtomicBoolean {
public:
	explicit AtomicBoolean(bool initialValue = false) noexcept : value_(initialValue) {}
	AtomicBoolean(const LockClass& lockClass, bool initialValue = false) noexcept : value_(initialValue), monitor_(lockClass) {}
	AtomicBoolean(const AtomicBoolean&) = delete;
	AtomicBoolean& operator=(const AtomicBoolean&) = delete;

	bool get() const noexcept {
		AION_YIELD_POINT("AtomicBoolean::get");
		return value_.load(std::memory_order_acquire);
	}
	void set(bool newValue) noexcept {
		AION_YIELD_POINT("AtomicBoolean::set");
		value_.store(newValue, std::memory_order_release);
	}
	void lazySet(bool newValue) noexcept { set(newValue); }
	bool getAndSet(bool newValue) noexcept {
		AION_YIELD_POINT("AtomicBoolean::getAndSet");
		return value_.exchange(newValue, std::memory_order_acq_rel);
	}
	bool compareAndSet(bool expectedValue, bool newValue) noexcept {
		AION_YIELD_POINT("AtomicBoolean::compareAndSet");
		return value_.compare_exchange_strong(expectedValue, newValue, std::memory_order_acq_rel);
	}
	/** Java toString() */
	std::string toString() const { return get() ? "true" : "false"; }

	Monitor& monitor() const noexcept { return monitor_; }

private:
	std::atomic<bool> value_;
	mutable Monitor monitor_;
};

/** Java: AtomicInteger (int32_t) and AtomicLong (int64_t). Arithmetic wraps like Java (two's complement, no UB). */
template <std::signed_integral V>
class AtomicNumber {
public:
	explicit AtomicNumber(V initialValue = 0) noexcept : value_(initialValue) {}
	AtomicNumber(const LockClass& lockClass, V initialValue = 0) noexcept : value_(initialValue), monitor_(lockClass) {}
	AtomicNumber(const AtomicNumber&) = delete;
	AtomicNumber& operator=(const AtomicNumber&) = delete;

	V get() const noexcept {
		AION_YIELD_POINT("AtomicNumber::get");
		return value_.load(std::memory_order_acquire);
	}
	void set(V newValue) noexcept {
		AION_YIELD_POINT("AtomicNumber::set");
		value_.store(newValue, std::memory_order_release);
	}
	void lazySet(V newValue) noexcept { set(newValue); }
	V getAndSet(V newValue) noexcept {
		AION_YIELD_POINT("AtomicNumber::getAndSet");
		return value_.exchange(newValue, std::memory_order_acq_rel);
	}
	bool compareAndSet(V expectedValue, V newValue) noexcept {
		AION_YIELD_POINT("AtomicNumber::compareAndSet");
		return value_.compare_exchange_strong(expectedValue, newValue, std::memory_order_acq_rel);
	}
	/** std::atomic arithmetic on signed integers wraps (two's complement) like Java. */
	V getAndIncrement() noexcept { return getAndAdd(1); }
	V getAndDecrement() noexcept { return getAndAdd(-1); }
	V incrementAndGet() noexcept { return addAndGet(1); }
	V decrementAndGet() noexcept { return addAndGet(-1); }
	V getAndAdd(V delta) noexcept {
		AION_YIELD_POINT("AtomicNumber::getAndAdd");
		return value_.fetch_add(delta, std::memory_order_acq_rel);
	}
	V addAndGet(V delta) noexcept {
		AION_YIELD_POINT("AtomicNumber::addAndGet");
		return static_cast<V>(static_cast<std::make_unsigned_t<V>>(value_.fetch_add(delta, std::memory_order_acq_rel)) +
			static_cast<std::make_unsigned_t<V>>(delta));
	}
	V getAndUpdate(std::invocable<V> auto&& updateFunction) {
		V previous = get();
		for (;;) {
			V next = static_cast<V>(updateFunction(previous));
			AION_YIELD_POINT("AtomicNumber::getAndUpdate");
			if (value_.compare_exchange_weak(previous, next, std::memory_order_acq_rel))
				return previous;
		}
	}
	V updateAndGet(std::invocable<V> auto&& updateFunction) {
		V previous = get();
		for (;;) {
			V next = static_cast<V>(updateFunction(previous));
			AION_YIELD_POINT("AtomicNumber::updateAndGet");
			if (value_.compare_exchange_weak(previous, next, std::memory_order_acq_rel))
				return next;
		}
	}
	V getAndAccumulate(V x, std::invocable<V, V> auto&& accumulatorFunction) {
		return getAndUpdate([&](V previous) { return accumulatorFunction(previous, x); });
	}
	V accumulateAndGet(V x, std::invocable<V, V> auto&& accumulatorFunction) {
		return updateAndGet([&](V previous) { return accumulatorFunction(previous, x); });
	}
	int32_t intValue() const noexcept { return static_cast<int32_t>(get()); }
	int64_t longValue() const noexcept { return static_cast<int64_t>(get()); }
	std::string toString() const { return std::to_string(get()); }

	Monitor& monitor() const noexcept { return monitor_; }

private:
	std::atomic<V> value_;
	mutable Monitor monitor_;
};

/** Java: AtomicInteger */
using AtomicInteger = AtomicNumber<int32_t>;
/** Java: AtomicLong */
using AtomicLong = AtomicNumber<int64_t>;

/**
 * Java: AtomicReference<V>. For object references use AtomicReference<Ref<X>> (e.g. AtomicReference<FutureRef>): get() is a pointer load with
 * read barrier returning Ptr<X>; getAndSet returns the previous Ref; compareAndSet compares identity (Java `==`).
 * For trivially copyable V (enums, template pointers) it behaves like AtomicNumber without arithmetic.
 */
template <class V>
class AtomicReference {
public:
	AtomicReference() noexcept = default;
	explicit AtomicReference(V initialValue) noexcept : value_(initialValue) {}
	AtomicReference(const LockClass& lockClass, V initialValue = V{}) noexcept : value_(initialValue), monitor_(lockClass) {}
	AtomicReference(const AtomicReference&) = delete;
	AtomicReference& operator=(const AtomicReference&) = delete;

	V get() const noexcept { return value_.get(); }
	void set(V newValue) noexcept { value_.set(newValue); }
	void lazySet(V newValue) noexcept { set(newValue); }
	V getAndSet(V newValue) noexcept { return value_.exchange(newValue); }
	/** Compares values (object representation, see Field<T>::compareAndSet); for template pointers this is identity like Java. */
	bool compareAndSet(V expectedValue, V newValue) noexcept { return value_.compareAndSet(expectedValue, newValue); }
	V getAndUpdate(std::invocable<V> auto&& updateFunction) {
		for (;;) {
			V previous = get();
			if (compareAndSet(previous, static_cast<V>(updateFunction(previous))))
				return previous;
		}
	}
	V updateAndGet(std::invocable<V> auto&& updateFunction) {
		for (;;) {
			V previous = get();
			V next = static_cast<V>(updateFunction(previous));
			if (compareAndSet(previous, next))
				return next;
		}
	}

	Monitor& monitor() const noexcept { return monitor_; }

private:

	Field<V> value_;
	mutable Monitor monitor_;
};

template <class X>
class AtomicReference<Ref<X>> {
public:
	AtomicReference() noexcept = default;
	explicit AtomicReference(Ref<X> initialValue) noexcept : value_(std::move(initialValue)) {}
	AtomicReference(const LockClass& lockClass, Ref<X> initialValue = nullptr) noexcept : value_(std::move(initialValue)), monitor_(lockClass) {}
	AtomicReference(const AtomicReference&) = delete;
	AtomicReference& operator=(const AtomicReference&) = delete;

	/** Pointer load with read barrier. */
	Ptr<X> get() const { return value_.get(); }
	void set(Ref<X> newValue) { value_.set(std::move(newValue)); }
	void lazySet(Ref<X> newValue) { set(std::move(newValue)); }
	Ref<X> getAndSet(Ref<X> newValue) noexcept { return value_.exchange(std::move(newValue)); }
	/** identity comparison */
	bool compareAndSet(Ptr<X> expectedValue, Ref<X> newValue) noexcept { return value_.compareAndSet(expectedValue, std::move(newValue)); }
	bool compareAndSet(std::nullptr_t, Ref<X> newValue) noexcept { return value_.compareAndSet(nullptr, std::move(newValue)); }
	Ptr<X> getAndUpdate(std::invocable<Ptr<X>> auto&& updateFunction) {
		for (;;) {
			Ptr<X> previous = get();
			if (compareAndSet(previous, Ref<X>(updateFunction(previous))))
				return previous;
		}
	}
	Ptr<X> updateAndGet(std::invocable<Ptr<X>> auto&& updateFunction) {
		for (;;) {
			Ptr<X> previous = get();
			Ref<X> next(updateFunction(previous));
			Ptr<X> nextBorrow(next);
			if (compareAndSet(previous, std::move(next)))
				return nextBorrow;
		}
	}

	Monitor& monitor() const noexcept { return monitor_; }

private:
	Field<Ref<X>> value_;
	mutable Monitor monitor_;
};

/** Java: AtomicLongArray (TheHexwayInstance.stageStartMillis) */
class AtomicLongArray {
public:
	explicit AtomicLongArray(int32_t length);
	AtomicLongArray(const LockClass& lockClass, int32_t length);
	AtomicLongArray(const AtomicLongArray&) = delete;
	AtomicLongArray& operator=(const AtomicLongArray&) = delete;

	int32_t length() const noexcept { return length_; }
	/** @throws ArrayIndexOutOfBoundsException for every indexed operation */
	int64_t get(int32_t index) const;
	void set(int32_t index, int64_t newValue);
	void lazySet(int32_t index, int64_t newValue) { set(index, newValue); }
	int64_t getAndSet(int32_t index, int64_t newValue);
	bool compareAndSet(int32_t index, int64_t expectedValue, int64_t newValue);
	int64_t getAndIncrement(int32_t index) { return getAndAdd(index, 1); }
	int64_t getAndDecrement(int32_t index) { return getAndAdd(index, -1); }
	int64_t incrementAndGet(int32_t index) { return addAndGet(index, 1); }
	int64_t decrementAndGet(int32_t index) { return addAndGet(index, -1); }
	int64_t getAndAdd(int32_t index, int64_t delta);
	int64_t addAndGet(int32_t index, int64_t delta);

	Monitor& monitor() const noexcept { return monitor_; }

private:
	std::atomic<int64_t>& slot(int32_t index) const;

	const int32_t length_;
	const std::unique_ptr<std::atomic<int64_t>[]> values_;
	mutable Monitor monitor_;
};

} // namespace aion::gameserver::runtime
