#include "aion/gameserver/services/toypet/PetAdoptionService.h"

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/commons/utils/TimeUtils.h"
#include "aion/gameserver/controllers/PetController.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/ItemData.h"
#include "aion/gameserver/dataholders/PetData.h"
#include "aion/gameserver/model/gameobjects/Item.h"
#include "aion/gameserver/model/gameobjects/Pet.h"
#include "aion/gameserver/model/gameobjects/player/PetCommonData.h"
#include "aion/gameserver/model/gameobjects/player/PetList.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/items/storage/Storage.h"
#include "aion/gameserver/model/templates/item/ItemTemplate.h"
#include "aion/gameserver/model/templates/item/actions/AdoptPetAction.h"
#include "aion/gameserver/model/templates/item/actions/ItemActions.h"
#include "aion/gameserver/network/aion/serverpackets/SM_PET.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/taskmanager/tasks/ExpireTimerTask.h"
#include "aion/gameserver/utils/PacketSendUtility.h"
#include "aion/gameserver/utils/Util.h"
#include "aion/gameserver/utils/idfactory/IDFactory.h"

namespace aion::gameserver::services::toypet {

static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.services.toypet.PetAdoptionService");

// Java PetAdoptionService.java:32-48
void PetAdoptionService::adoptPet(model::gameobjects::player::Player& player, int32_t eggObjId, int32_t petId, std::string_view name,
	int32_t decorationId) {
	runtime::Ptr<model::gameobjects::Item> egg = player.getInventory().getItemByObjId(eggObjId);
	if (egg == nullptr) // Java: getItemByObjId(eggObjId).getItemId() on null
		throw runtime::NullPointerException("Inventory.getItemByObjId(" + std::to_string(eggObjId) + ")");
	const int32_t eggId = egg->getItemId();
	const model::templates::item::ItemTemplate* template_ = dataholders::DataManager::ITEM_DATA->getItemTemplate(eggId);
	if (!validateAdoption(player, template_, petId))
		return;

	if (!player.getInventory().decreaseByObjectId(eggObjId, 1))
		return;

	// Java: the int `getExpireMinutes() * 60` added to a long, the sum narrowed to int
	const int32_t expireMinutes = template_->getActions()->getAdoptPetAction()->getExpireMinutes();
	const int32_t expireTime = expireMinutes != 0
		? static_cast<int32_t>(commons::utils::currentTimeMillis() / 1000 + static_cast<int32_t>(static_cast<uint32_t>(expireMinutes) * 60u))
		: 0;

	addPet(player, petId, name, decorationId, expireTime);
}

// Java PetAdoptionService.java:57-64
void PetAdoptionService::addPet(model::gameobjects::player::Player& player, int32_t petId, std::string_view name, int32_t decorationId,
	int32_t expireTime) {
	const std::string convertedName = utils::Util::convertName(name);
	runtime::Ptr<model::gameobjects::player::PetCommonData> petCommonData =
		player.getPetList().addPet(player, petId, decorationId, convertedName, expireTime);
	if (petCommonData != nullptr) {
		utils::PacketSendUtility::sendPacket(player, network::aion::serverpackets::SM_PET(*petCommonData, true));
		taskmanager::tasks::ExpireTimerTask::getInstance().registerExpirable(*petCommonData, player);
	}
}

// Java PetAdoptionService.java:66-81
bool PetAdoptionService::validateAdoption(model::gameobjects::player::Player& player, const model::templates::item::ItemTemplate* template_,
	int32_t petId) {
	if (template_ == nullptr || template_->getActions() == nullptr || template_->getActions()->getAdoptPetAction() == nullptr ||
		template_->getActions()->getAdoptPetAction()->getPetId() != petId) {
		return false;
	}
	if (player.getPetList().hasPet(petId)) {
		log.warn("Duplicate pet adoption " + player.toString() + " (pet: " + std::to_string(petId) + ")");
		return false;
	}
	if (dataholders::DataManager::PET_DATA->getPetTemplate(petId) == nullptr) {
		log.warn("Trying adopt pet without template. PetId:" + std::to_string(petId));
		return false;
	}
	return true;
}

// Java PetAdoptionService.java:88-98
void PetAdoptionService::surrenderPet(model::gameobjects::player::Player& player, int32_t petId) {
	runtime::Ptr<model::gameobjects::player::PetCommonData> petCommonData = player.getPetList().deletePet(petId);
	if (petCommonData == nullptr)
		return;

	if (player.getPet() != nullptr && player.getPet()->getObjectId() == petCommonData->getObjectId())
		player.getPet()->getController().delete_();

	utils::PacketSendUtility::sendPacket(player, network::aion::serverpackets::SM_PET(*petCommonData, false));
	utils::idfactory::IDFactory::getInstance().releaseId(petCommonData->getObjectId());
}

} // namespace aion::gameserver::services::toypet
