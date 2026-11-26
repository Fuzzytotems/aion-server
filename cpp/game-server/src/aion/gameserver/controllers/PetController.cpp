#include "aion/gameserver/controllers/PetController.h"

#include <exception>

#include "aion/commons/utils/TimeUtils.h"
#include "aion/gameserver/controllers/PlayerController.h"
#include "aion/gameserver/dao/PlayerPetsDAO.h"
#include "aion/gameserver/model/TaskId.h"
#include "aion/gameserver/model/gameobjects/Pet.h"
#include "aion/gameserver/model/gameobjects/player/PetCommonData.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/templates/pet/PetDopingBag.h"
#include "aion/gameserver/network/aion/serverpackets/SM_PET.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/services/toypet/PetFeedProgress.h"
#include "aion/gameserver/services/toypet/PetHungryLevel.h"
#include "aion/gameserver/services/toypet/PetHungryLevelInfo.h"
#include "aion/gameserver/utils/PacketSendUtility.h"

namespace aion::gameserver::controllers {

using model::gameobjects::Pet;
using model::gameobjects::player::PetCommonData;
using runtime::Ptr;
using utils::PacketSendUtility;

PetController::PetUpdateTask::PetUpdateTask(model::gameobjects::player::Player& playerValue) : player(playerValue) {
}

PetController::PetUpdateTask::~PetUpdateTask() = default;

runtime::Ref<PetController::PetUpdateTask> PetController::PetUpdateTask::create(model::gameobjects::player::Player& playerValue) {
	return runtime::makeRef<PetUpdateTask>(playerValue);
}

void PetController::PetUpdateTask::run() {
	if (startTime.get() == 0)
		startTime = commons::utils::currentTimeMillis();

	try {
		if (!player->isSpawned())
			return;

		Ptr<Pet> pet = player->getPet();
		if (!pet)
			throw runtime::IllegalStateException("Pet is null");

		int32_t currentPoints = 0;

		if (pet->getCommonData()->getMoodPoints(false) < 9000) {
			if (commons::utils::currentTimeMillis() - startTime.get() >= 60 * 1000) {
				currentPoints = pet->getCommonData()->getMoodPoints(false);
				if (currentPoints == 9000) {
					PacketSendUtility::sendPacket(*player, network::aion::serverpackets::SM_PET(*pet, 4, 0));
				}

				// Java 51d7a19cb saved the mood here (PlayerPetsDAO.savePetMoodData); since f16e03ad3 the mood is kept in memory only
				startTime = commons::utils::currentTimeMillis();
			}
		}

		if (currentPoints < 9000) {
			PacketSendUtility::sendPacket(*player, network::aion::serverpackets::SM_PET(*pet, 4, 0));
		} else {
			PacketSendUtility::sendPacket(*player, network::aion::serverpackets::SM_PET(*pet, 3, 0));
			// Java 51d7a19cb saved the mood here when it reached 100% by snuggling (savePetMoodData); the columns are gone since f16e03ad3
		}
	} catch (const std::exception&) {
		player->getController().cancelTask(model::TaskId::PET_UPDATE);
	}
}

PetController::PetController() = default;

PetController::~PetController() = default;

model::gameobjects::Pet& PetController::getOwner() const {
	return static_cast<model::gameobjects::Pet&>(VisibleObjectController::getOwner());
}

void PetController::onDelete() {
	VisibleObjectController::onDelete();
	Ptr<PetCommonData> commonData = getOwner().getCommonData();
	Ptr<services::toypet::PetFeedProgress> progress = commonData->getFeedProgress();
	commonData->cancelRefeedTask();
	if (progress) {
		commonData->setCancelFeed(true);
		dao::PlayerPetsDAO::saveFeedStatus(getOwner().getObjectId(), services::toypet::getValue(progress->getHungryLevel()), progress->getDataForPacket(),
			commonData->getRefeedTime());
	}
	if (commonData->getDopingBag() && commonData->getDopingBag()->isDirty())
		dao::PlayerPetsDAO::saveDopingBag(getOwner().getObjectId(), *commonData->getDopingBag());

	getOwner().getMaster()->getController().cancelTask(model::TaskId::PET_UPDATE);
	commonData->setDespawnTime(commons::database::Timestamp(std::chrono::milliseconds(commons::utils::currentTimeMillis())));
	// Java 51d7a19cb stored the mood and the despawn time here (PlayerPetsDAO.savePetMoodData); both stay in memory since f16e03ad3 dropped
	// their columns, until wave 5 ports the rework (PetMoodService.onDismiss)
	getOwner().getMaster()->setPet(nullptr);
}

} // namespace aion::gameserver::controllers
