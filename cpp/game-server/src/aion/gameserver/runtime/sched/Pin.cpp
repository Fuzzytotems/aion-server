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
		const OwnedPart* part = target.part();
		if (owner == nullptr)
			continue;
		size_t slot = 0;
		// an owner and one of its parts share a slot; two different parts of one owner need two
		while (slot < count_ && !(owners_[slot] == owner && (part == nullptr || parts_[slot] == nullptr || parts_[slot] == part)))
			++slot;
		if (slot < count_) {
			if (part != nullptr && parts_[slot] == nullptr) { // the owner was pinned directly: hold it through the part from now on
				part->retain();
				parts_[slot] = part;
				owner->release();
			}
			continue;
		}
		if (count_ == MAX_OWNERS) {
			reset();
			throw IllegalArgumentException("A Pin retains at most 4 owners");
		}
		owners_[count_] = owner;
		parts_[count_] = part;
		retainSlot(count_++);
	}
}

Pin::Pin(const Pin& other) noexcept : owners_(other.owners_), parts_(other.parts_), count_(other.count_) {
	for (size_t i = 0; i < count_; ++i)
		retainSlot(i);
}

Pin::Pin(Pin&& other) noexcept : owners_(other.owners_), parts_(other.parts_), count_(other.count_) {
	other.count_ = 0;
	other.owners_.fill(nullptr);
	other.parts_.fill(nullptr);
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
		parts_ = other.parts_;
		count_ = other.count_;
		other.count_ = 0;
		other.owners_.fill(nullptr);
		other.parts_.fill(nullptr);
	}
	return *this;
}

Pin::~Pin() {
	reset();
}

void Pin::retainSlot(size_t index) const noexcept {
	if (parts_[index] != nullptr)
		parts_[index]->retain(); // counts the part and retains its owner
	else
		owners_[index]->retain();
}

void Pin::reset() noexcept {
	size_t count = count_;
	count_ = 0;
	for (size_t i = 0; i < count; ++i) {
		const RefCounted* owner = owners_[i];
		const OwnedPart* part = parts_[i];
		owners_[i] = nullptr;
		parts_[i] = nullptr;
		if (part != nullptr)
			part->release(); // stamps the part, uncounts it and releases the owner
		else
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
