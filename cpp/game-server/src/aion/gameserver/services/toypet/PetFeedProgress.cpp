#include "aion/gameserver/services/toypet/PetFeedProgress.h"

#include "aion/gameserver/runtime/base/Unported.h"

namespace aion::gameserver::services::toypet {

PetFeedProgress::PetFeedProgress(int16_t lovedFoodLimit)
	: lovedFoodMax(static_cast<int16_t>(lovedFoodLimit & 0x3F)) {
}

runtime::Ref<PetFeedProgress> PetFeedProgress::create(int16_t lovedFoodLimit) {
	return runtime::makeRef<PetFeedProgress>(lovedFoodLimit);
}

void PetFeedProgress::setTotalPoints(int32_t points) {
	AION_UNPORTED();
}

int32_t PetFeedProgress::getRegularCount() {
	AION_UNPORTED();
}

int32_t PetFeedProgress::getLovedFoodRemaining() {
	AION_UNPORTED();
}

void PetFeedProgress::setIsLovedFeeded() {
	lovedFeeded.set(true);
}

void PetFeedProgress::incrementCount(bool lovedFood) {
	AION_UNPORTED();
}

void PetFeedProgress::reset() {
	AION_UNPORTED();
}

int32_t PetFeedProgress::getDataForPacket() {
	AION_UNPORTED();
}

void PetFeedProgress::setData(int32_t savedData) {
	AION_UNPORTED();
}

PetFeedProgress::~PetFeedProgress() = default;

} // namespace aion::gameserver::services::toypet
