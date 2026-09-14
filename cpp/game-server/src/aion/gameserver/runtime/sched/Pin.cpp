#include "aion/gameserver/runtime/sched/Pin.h"

#include "aion/gameserver/runtime/base/Checked.h"
#include "aion/gameserver/runtime/base/Exceptions.h"

namespace aion::gameserver::runtime {

PinTarget::Classified PinTarget::classify(const Immortal* immortal) {
	if (CHECKED && immortal != nullptr && !Immortal::isRegistered(immortal))
		throw IllegalStateException("Pin of an address that is not a registered Immortal (C10)");
	return {nullptr};
}

Pin::Pin(std::initializer_list<PinTarget> targets) {
	for (const PinTarget& target : targets) {
		const RefCounted* owner = target.retained();
		if (owner == nullptr || pins(*owner))
			continue;
		if (count_ == MAX_OWNERS) {
			reset();
			throw IllegalArgumentException("A Pin retains at most 4 owners");
		}
		owner->retain();
		owners_[count_++] = owner;
	}
}

Pin::Pin(const Pin& other) noexcept : owners_(other.owners_), count_(other.count_) {
	for (size_t i = 0; i < count_; ++i)
		owners_[i]->retain();
}

Pin::Pin(Pin&& other) noexcept : owners_(other.owners_), count_(other.count_) {
	other.count_ = 0;
	other.owners_.fill(nullptr);
}

Pin& Pin::operator=(const Pin& other) noexcept {
	if (this != &other) {
		Pin copy(other);
		*this = std::move(copy);
	}
	return *this;
}

Pin& Pin::operator=(Pin&& other) noexcept {
	if (this != &other) {
		reset();
		owners_ = other.owners_;
		count_ = other.count_;
		other.count_ = 0;
		other.owners_.fill(nullptr);
	}
	return *this;
}

Pin::~Pin() {
	reset();
}

void Pin::reset() noexcept {
	size_t count = count_;
	count_ = 0;
	for (size_t i = 0; i < count; ++i) {
		const RefCounted* owner = owners_[i];
		owners_[i] = nullptr;
		owner->release();
	}
}

bool Pin::pins(const RefCounted& owner) const noexcept {
	for (size_t i = 0; i < count_; ++i)
		if (owners_[i] == &owner)
			return true;
	return false;
}

} // namespace aion::gameserver::runtime
