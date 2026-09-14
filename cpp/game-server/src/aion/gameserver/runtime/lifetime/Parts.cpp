// OwnedPart binding checks (C11) and the out-of-line helpers of the part containers.

#include "aion/gameserver/runtime/lifetime/Parts.h"

#include <string>

#include "aion/commons/utils/ClassName.h"
#include "aion/gameserver/runtime/lifetime/detail/Epoch.h"
#include "aion/gameserver/runtime/lifetime/detail/Mutations.h"

namespace aion::gameserver::runtime {

namespace {

std::string className(const std::type_info& type) {
	try {
		return commons::utils::getClassName(type);
	} catch (...) {
		return "<unknown class>";
	}
}

} // namespace

OwnedPart::~OwnedPart() {
	AION_CHECK("C5", partRefs_.load(std::memory_order_acquire) == 0, "part destroyed while a Ref still holds it: " + className(typeid(*this)));
}

void OwnedPart::retain() const noexcept {
	AION_CHECK("C11", owner_ != nullptr, "retain() on a part whose owner is not bound: " + className(typeid(*this)));
	owner_->retain();
	AION_YIELD_POINT("OwnedPart::retain");
	[[maybe_unused]] uint32_t previous = partRefs_.fetch_add(1, std::memory_order_acq_rel);
	AION_CHECK("C4", previous != UINT32_MAX, "part reference count overflow: " + className(typeid(*this)));
}

void OwnedPart::release() const noexcept {
	AION_CHECK("C11", owner_ != nullptr, "release() on a part whose owner is not bound: " + className(typeid(*this)));
	const RefCounted* owner = owner_; // read before the decrement: afterwards a scan may destroy a retired part
	if (!detail::isMutated(detail::Mutation::PART_RELEASE_NO_STAMP)) {
		const uint64_t epoch = detail::globalEpoch.load(std::memory_order_acquire); // read after the caller's unlink
		uint64_t stamped = partStamp_.load(std::memory_order_acquire);
		while (stamped < epoch) {
			AION_YIELD_POINT("OwnedPart::release:stamp");
			if (partStamp_.compare_exchange_strong(stamped, epoch, std::memory_order_acq_rel))
				break;
		}
	}
	AION_YIELD_POINT("OwnedPart::release:decrement");
	[[maybe_unused]] uint32_t previous = partRefs_.fetch_sub(1, std::memory_order_acq_rel);
	AION_CHECK("C4", previous != 0, "part reference count underflow: " + className(typeid(*this)));
	owner->release();
}

const RefCounted& OwnedPart::partOwner() const noexcept {
	AION_CHECK("C11", owner_ != nullptr, "partOwner() on a part whose owner is not bound: " + className(typeid(*this)));
	return *owner_;
}

void OwnedPart::bindOwner(const RefCounted& owner) noexcept {
	AION_CHECK("C11", owner_ == nullptr, "bindOwner() called twice on " + className(typeid(*this)));
	AION_CHECK("C11", owner.refCount() <= 1, "bindOwner() after the owner was published: " + className(typeid(*this)));
	owner_ = &owner;
}

namespace detail {

void throwNullPart(const std::type_info& type) {
	throw NullPointerException("Dereferencing an empty part slot of " + className(type));
}

void throwNullPartArgument(const std::type_info& type) {
	throw NullPointerException("Storing a null part of " + className(type));
}

void throwPartIndexOutOfBounds(int32_t index, int32_t size) {
	throw IndexOutOfBoundsException("Index " + std::to_string(index) + " out of bounds for length " + std::to_string(size));
}

void checkPartOwner(const OwnedPart& part, const RefCounted& owner, const std::type_info& type) noexcept {
	AION_CHECK("C11", part.isOwnerBound() && &part.partOwner() == &owner,
		"storing a part of " + className(type) + " that is not bound to the container's owner " + className(typeid(owner)));
}

} // namespace detail

} // namespace aion::gameserver::runtime
