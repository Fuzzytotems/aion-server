#include "aion/gameserver/model/gameobjects/player/PetCommonData.h"

#include "aion/gameserver/model/templates/pet/PetDopingBag.h"
#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/services/toypet/PetFeedProgress.h"

namespace aion::gameserver::model::gameobjects::player {

PetCommonData::PetCommonData(int32_t objectIdValue, int32_t templateIdValue, int32_t masterObjectIdValue, int32_t expireTimeValue)
	: objectId(objectIdValue), templateId(templateIdValue), masterObjectId(masterObjectIdValue), expireTime(expireTimeValue) {
	// Java: the pet template's FOOD function creates feedProgress (PET_FEED_DATA), its DOPING function the doping bag
	AION_UNPORTED();
}

PetCommonData::~PetCommonData() = default;

runtime::Ref<PetCommonData> PetCommonData::create(int32_t objectIdValue, int32_t templateIdValue, int32_t masterObjectIdValue,
	int32_t expireTimeValue) {
	return runtime::makeRef<PetCommonData>(objectIdValue, templateIdValue, masterObjectIdValue, expireTimeValue);
}

int32_t PetCommonData::getBirthday() {
	AION_UNPORTED();
}

void PetCommonData::scheduleRefeed(int64_t reFoodTime) {
	AION_UNPORTED();
}

void PetCommonData::cancelRefeedTask() {
	AION_UNPORTED();
}

int64_t PetCommonData::getRefeedDelay() {
	AION_UNPORTED();
}

int32_t PetCommonData::getMoodPoints(bool forPacket) {
	AION_UNPORTED();
}

bool PetCommonData::increaseShuggleCounter() {
	AION_UNPORTED();
}

void PetCommonData::clearMoodStatistics() {
	AION_UNPORTED();
}

int32_t PetCommonData::getMoodRemainingTime() {
	AION_UNPORTED();
}

int32_t PetCommonData::getGiftRemainingTime() {
	AION_UNPORTED();
}

void PetCommonData::onExpire(Player& player) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::model::gameobjects::player
