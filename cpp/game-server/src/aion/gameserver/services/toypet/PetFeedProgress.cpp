#include "aion/gameserver/services/toypet/PetFeedProgress.h"


namespace aion::gameserver::services::toypet {

PetFeedProgress::PetFeedProgress(int16_t lovedFoodLimit)
	: lovedFoodMax(static_cast<int16_t>(lovedFoodLimit & 0x3F)) {
}

runtime::Ref<PetFeedProgress> PetFeedProgress::create(int16_t lovedFoodLimit) {
	return runtime::makeRef<PetFeedProgress>(lovedFoodLimit);
}

// Java PetFeedProgress.java:26-28
void PetFeedProgress::setTotalPoints(int32_t points) {
	totalPoints.set(points & 0x3FFF);
}

// Java PetFeedProgress.java:44-46
int32_t PetFeedProgress::getRegularCount() {
	return regularConsumed.get() & 0xFF;
}

// Java PetFeedProgress.java:52-54: short - short, promoted to int
int32_t PetFeedProgress::getLovedFoodRemaining() {
	return lovedFoodMax - lovedConsumed.get();
}

void PetFeedProgress::setIsLovedFeeded() {
	lovedFeeded.set(true);
}

// Java PetFeedProgress.java:64-70: short++ (wraps like a Java short)
void PetFeedProgress::incrementCount(bool lovedFood) {
	if (lovedFood) {
		lovedConsumed.set(static_cast<int16_t>(static_cast<uint16_t>(lovedConsumed.get()) + 1u));
	} else {
		regularConsumed.set(static_cast<int16_t>(static_cast<uint16_t>(regularConsumed.get()) + 1u));
	}
}

// Java PetFeedProgress.java:72-79
void PetFeedProgress::reset() {
	if (lovedFeeded.get())
		lovedFeeded.set(false);
	else {
		totalPoints.set(0);
		regularConsumed.set(0);
	}
}

// Java PetFeedProgress.java:81-89: int shifts, in unsigned arithmetic (Java's int wraps; a signed C++ overflow would not)
int32_t PetFeedProgress::getDataForPacket() {
	uint32_t value = static_cast<uint32_t>(getRegularCount() & 0xFF);
	value <<= 14;
	value |= static_cast<uint32_t>(totalPoints.get() >> 2);
	value <<= 6;
	value |= static_cast<uint32_t>(lovedConsumed.get() & 0x3F);
	value <<= 4; // unk
	return static_cast<int32_t>(value);
}

// Java PetFeedProgress.java:91-99: >>= is Java's arithmetic shift of an int (C++20 defines >> on a negative int the same way)
void PetFeedProgress::setData(int32_t savedData) {
	savedData >>= 4; // drop unk
	lovedConsumed.set(static_cast<int16_t>(savedData & 0x3F));
	savedData >>= 6;
	totalPoints.set((savedData & 0x3FFF) << 2);
	savedData >>= 14;
	regularConsumed.set(static_cast<int16_t>(savedData & 0xFF));
}

PetFeedProgress::~PetFeedProgress() = default;

} // namespace aion::gameserver::services::toypet
