#include "aion/gameserver/model/gameobjects/player/PetCommonData.h"

#include <string>

#include "aion/commons/utils/TimeUtils.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/PetData.h"
#include "aion/gameserver/dataholders/PetFeedData.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/gameobjects/player/detail/PlayerMath.h"
#include "aion/gameserver/model/templates/pet/PetDopingBag.h"
#include "aion/gameserver/model/templates/pet/PetFlavour.h"
#include "aion/gameserver/model/templates/pet/PetFunction.h"
#include "aion/gameserver/model/templates/pet/PetFunctionType.h"
#include "aion/gameserver/model/templates/pet/PetTemplate.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/services/toypet/PetAdoptionService.h"
#include "aion/gameserver/services/toypet/PetFeedProgress.h"
#include "aion/gameserver/services/toypet/PetHungryLevel.h"
#include "aion/gameserver/utils/PacketSendUtility.h"
#include "aion/gameserver/utils/ThreadPoolManager.h"

namespace aion::gameserver::model::gameobjects::player {

PetCommonData::PetCommonData(int32_t objectIdValue, int32_t templateIdValue, int32_t masterObjectIdValue, int32_t expireTimeValue)
	: objectId(objectIdValue), templateId(templateIdValue), masterObjectId(masterObjectIdValue), expireTime(expireTimeValue) {
	const templates::pet::PetTemplate* template_ = dataholders::DataManager::PET_DATA->getPetTemplate(templateIdValue);
	if (template_ == nullptr)
		throw runtime::NullPointerException("PetTemplate " + std::to_string(templateIdValue) + " is null");
	if (template_->containsFunction(templates::pet::PetFunctionType::FOOD)) {
		int32_t flavourId = template_->getPetFunction(templates::pet::PetFunctionType::FOOD)->getId();
		const templates::pet::PetFlavour* flavour = dataholders::DataManager::PET_FEED_DATA->getFlavourById(flavourId);
		if (flavour == nullptr)
			throw runtime::NullPointerException("PetFlavour " + std::to_string(flavourId) + " is null");
		int32_t lovedLimit = flavour->getLovedFoodLimit();
		feedProgress.set(services::toypet::PetFeedProgress::create(static_cast<int8_t>(lovedLimit & 0xFF))); // Java: (byte) (lovedLimit & 0xFF)
	}
	if (template_->containsFunction(templates::pet::PetFunctionType::DOPING)) {
		dopingBag.set(templates::pet::PetDopingBag::create());
	}
}

PetCommonData::~PetCommonData() = default;

runtime::Ref<PetCommonData> PetCommonData::create(int32_t objectIdValue, int32_t templateIdValue, int32_t masterObjectIdValue,
	int32_t expireTimeValue) {
	return runtime::makeRef<PetCommonData>(objectIdValue, templateIdValue, masterObjectIdValue, expireTimeValue);
}

int32_t PetCommonData::getBirthday() {
	std::optional<commons::database::Timestamp> value = birthday.get();
	if (!value)
		return 0;
	return static_cast<int32_t>(value->time_since_epoch().count() / 1000);
}

void PetCommonData::scheduleRefeed(int64_t reFoodTime) {
	cancelRefeedTask();
	refeedTask.set(utils::ThreadPoolManager::getInstance().schedule(
		{this},
		[this] {
			refeedTime.set(0);
			feedProgress->setHungryLevel(services::toypet::PetHungryLevel::HUNGRY);
		},
		reFoodTime));
}

void PetCommonData::cancelRefeedTask() {
	runtime::Ptr<runtime::Future> task = refeedTask.get();
	if (task)
		task->cancel(false);
}

int64_t PetCommonData::getRefeedDelay() {
	int64_t time = refeedTime.get() - commons::utils::currentTimeMillis();
	if (time < 0) {
		refeedTime.set(0);
		time = 0;
	}
	return time;
}

int32_t PetCommonData::getMoodPoints(bool forPacket) {
	if (startMoodTime.get() == 0)
		startMoodTime.set(commons::utils::currentTimeMillis());
	// Java: Math.round((System.currentTimeMillis() - startMoodTime) / 1000f) + shuggleCounter * 1000 (long / float is a float division)
	const float seconds = static_cast<float>(commons::utils::currentTimeMillis() - startMoodTime.get()) / 1000.0f;
	const int32_t points = static_cast<int32_t>(static_cast<uint32_t>(detail::javaRound(seconds)) + static_cast<uint32_t>(shuggleCounter.get()) * 1000u);
	if (forPacket && points > 9000)
		return 9000;
	return points;
}

bool PetCommonData::increaseShuggleCounter() {
	if (getMoodRemainingTime() > 0)
		return false;
	moodCdStarted.set(commons::utils::currentTimeMillis());
	shuggleCounter++;
	return true;
}

void PetCommonData::clearMoodStatistics() {
	startMoodTime.set(0);
	shuggleCounter.set(0);
}

int32_t PetCommonData::getMoodRemainingTime() {
	int64_t stop = moodCdStarted.get() + 600000;
	int64_t remains = stop - commons::utils::currentTimeMillis();
	if (remains <= 0) {
		setMoodCdStarted(0);
		return 0;
	}
	return static_cast<int32_t>(remains / 1000);
}

int32_t PetCommonData::getGiftRemainingTime() {
	int64_t stop = giftCdStarted.get() + 3600 * 1000;
	int64_t remains = stop - commons::utils::currentTimeMillis();
	if (remains <= 0) {
		setGiftCdStarted(0);
		return 0;
	}
	return static_cast<int32_t>(remains / 1000);
}

void PetCommonData::onExpire(Player& player) {
	utils::PacketSendUtility::sendPacket(player, network::aion::serverpackets::SM_SYSTEM_MESSAGE::STR_MSG_PET_ABANDON_EXPIRE_TIME_COMPLETE(name.get()));
	services::toypet::PetAdoptionService::surrenderPet(player, templateId);
}

} // namespace aion::gameserver::model::gameobjects::player
