// Non-template parts of the Atomic* shims.

#include "aion/gameserver/runtime/fields/Atomic.h"

// the header-only field types are compiled here too, so the library build checks them for warnings
#include "aion/gameserver/runtime/fields/Array.h"
#include "aion/gameserver/runtime/fields/Final.h"
#include "aion/gameserver/runtime/base/YieldPoint.h"

namespace aion::gameserver::runtime {

namespace {

int32_t checkLength(int32_t length) {
	if (length < 0)
		throw IllegalArgumentException("Negative array size: " + std::to_string(length));
	return length;
}

} // namespace

AtomicLongArray::AtomicLongArray(int32_t length) : AtomicLongArray(LockClass::named("AtomicLongArray"), length) {
}

AtomicLongArray::AtomicLongArray(const LockClass& lockClass, int32_t length)
	: length_(checkLength(length)), values_(std::make_unique<std::atomic<int64_t>[]>(static_cast<size_t>(length_))), monitor_(lockClass) {
}

std::atomic<int64_t>& AtomicLongArray::slot(int32_t index) const {
	if (index < 0 || index >= length_) [[unlikely]]
		throw ArrayIndexOutOfBoundsException("Index " + std::to_string(index) + " out of bounds for length " + std::to_string(length_));
	return values_[static_cast<size_t>(index)];
}

int64_t AtomicLongArray::get(int32_t index) const {
	std::atomic<int64_t>& value = slot(index);
	AION_YIELD_POINT("AtomicLongArray::get");
	return value.load(std::memory_order_acquire);
}

void AtomicLongArray::set(int32_t index, int64_t newValue) {
	std::atomic<int64_t>& value = slot(index);
	AION_YIELD_POINT("AtomicLongArray::set");
	value.store(newValue, std::memory_order_release);
}

int64_t AtomicLongArray::getAndSet(int32_t index, int64_t newValue) {
	std::atomic<int64_t>& value = slot(index);
	AION_YIELD_POINT("AtomicLongArray::getAndSet");
	return value.exchange(newValue, std::memory_order_acq_rel);
}

bool AtomicLongArray::compareAndSet(int32_t index, int64_t expectedValue, int64_t newValue) {
	std::atomic<int64_t>& value = slot(index);
	AION_YIELD_POINT("AtomicLongArray::compareAndSet");
	return value.compare_exchange_strong(expectedValue, newValue, std::memory_order_acq_rel);
}

int64_t AtomicLongArray::getAndAdd(int32_t index, int64_t delta) {
	std::atomic<int64_t>& value = slot(index);
	AION_YIELD_POINT("AtomicLongArray::getAndAdd");
	return value.fetch_add(delta, std::memory_order_acq_rel);
}

int64_t AtomicLongArray::addAndGet(int32_t index, int64_t delta) {
	std::atomic<int64_t>& value = slot(index);
	AION_YIELD_POINT("AtomicLongArray::addAndGet");
	return static_cast<int64_t>(static_cast<uint64_t>(value.fetch_add(delta, std::memory_order_acq_rel)) + static_cast<uint64_t>(delta));
}

} // namespace aion::gameserver::runtime
