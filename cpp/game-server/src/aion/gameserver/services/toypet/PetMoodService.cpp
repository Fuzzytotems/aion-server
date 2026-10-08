#include "aion/gameserver/services/toypet/PetMoodService.h"

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/gameserver/model/gameobjects/Pet.h"
#include "aion/gameserver/model/gameobjects/player/PetCommonData.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/items/storage/Storage.h"
#include "aion/gameserver/model/templates/pet/PetTemplate.h"
#include "aion/gameserver/network/aion/serverpackets/SM_PET.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/services/item/ItemService.h"
#include "aion/gameserver/utils/PacketSendUtility.h"
#include "aion/gameserver/utils/audit/AuditLogger.h"

namespace aion::gameserver::services::toypet {

static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.services.toypet.PetMoodService");

// Java PetMoodService.java:20-32
void PetMoodService::checkMood(model::gameobjects::Pet& pet, int32_t type, int32_t shuggleEmotion) {
	switch (type) {
		case 0:
			startCheckingMood(pet);
			break;
		case 1:
			interactWithPet(pet, shuggleEmotion);
			break;
		case 3:
			requestPresent(pet);
			break;
	}
}

// Java PetMoodService.java:37-61
void PetMoodService::requestPresent(model::gameobjects::Pet& pet) {
	using network::aion::serverpackets::SM_PET;
	runtime::Ptr<model::gameobjects::player::Player> master = pet.getMaster();
	if (pet.getCommonData()->getMoodPoints(false) < 9000) {
		log.warn("Requested present before mood fill up: {}", master->getName());
		return;
	}

	if (pet.getCommonData()->getGiftRemainingTime() > 0) {
		utils::audit::AuditLogger::log(*master, "tried to get gift of pet " + std::to_string(pet.getObjectId()) + " during CD");
		return;
	}

	if (master->getInventory().isFull()) {
		utils::PacketSendUtility::sendPacket(*master, network::aion::serverpackets::SM_SYSTEM_MESSAGE::STR_WAREHOUSE_FULL_INVENTORY());
		return;
	}

	pet.getCommonData()->clearMoodStatistics();
	utils::PacketSendUtility::sendPacket(*master, SM_PET(pet, 4, 0));
	utils::PacketSendUtility::sendPacket(*master, SM_PET(pet, 3, 0));
	const int32_t itemId = pet.getObjectTemplate()->getConditionReward();
	if (itemId != 0) {
		item::ItemService::addItem(*master, pet.getObjectTemplate()->getConditionReward(), 1);
	}
}

// Java PetMoodService.java:66-73
void PetMoodService::interactWithPet(model::gameobjects::Pet& pet, int32_t shuggleEmotion) {
	using network::aion::serverpackets::SM_PET;
	if (pet.getCommonData() != nullptr) {
		if (pet.getCommonData()->increaseShuggleCounter()) {
			utils::PacketSendUtility::sendPacket(*pet.getMaster(), SM_PET(pet, 2, shuggleEmotion));
			utils::PacketSendUtility::sendPacket(*pet.getMaster(), SM_PET(pet, 4, 0)); // Update progress immediately
		}
	}
}

// Java PetMoodService.java:78-80
void PetMoodService::startCheckingMood(model::gameobjects::Pet& pet) {
	utils::PacketSendUtility::sendPacket(*pet.getMaster(), network::aion::serverpackets::SM_PET(pet, 0, 0));
}

} // namespace aion::gameserver::services::toypet
