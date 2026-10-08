#include "aion/gameserver/network/aion/clientpackets/CM_PET.h"

#include <any>
#include <array>
#include <memory>
#include <optional>
#include <span>
#include <string>

#include "aion/gameserver/controllers/VisibleObjectController.h"
#include "aion/gameserver/handlers/HandlerRegistry.h"
#include "aion/gameserver/model/EmotionType.h"
#include "aion/gameserver/model/gameobjects/Pet.h"
#include "aion/gameserver/model/gameobjects/PetActionInfo.h"
#include "aion/gameserver/model/gameobjects/player/PetCommonData.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/network/aion/AionConnection.h"
#include "aion/gameserver/network/aion/serverpackets/SM_EMOTION.h"
#include "aion/gameserver/network/aion/serverpackets/SM_PET.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/services/NameRestrictionService.h"
#include "aion/gameserver/services/toypet/PetAdoptionService.h"
#include "aion/gameserver/services/toypet/PetMoodService.h"
#include "aion/gameserver/services/toypet/PetService.h"
#include "aion/gameserver/services/toypet/PetSpawnService.h"
#include "aion/gameserver/utils/PacketSendUtility.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets {

CM_PET::CM_PET(int32_t opcode, const StateSet& validStates) : AionClientPacket(opcode, validStates) {
}

// Java CM_PET.java:50-115
void CM_PET::readImpl() {
	action = model::gameobjects::getActionById(readUH());
	switch (action) {
		case model::gameobjects::PetAction::ADOPT:
			eggObjId = readD();
			templateId = readD();
			unk2 = readUC();
			unk3 = readD();
			decorationId = readD();
			unk5 = readD();
			unk6 = readD();
			petName = readS();
			break;
		case model::gameobjects::PetAction::SURRENDER:
		case model::gameobjects::PetAction::SPAWN:
		case model::gameobjects::PetAction::DISMISS:
			templateId = readD();
			break;
		case model::gameobjects::PetAction::FOOD:
			actionType = readD();
			if (actionType == 3 || actionType == 4) { // auto loot (3), or auto sell items (4)
				activateSpecialFunction = readD();
				readD(); // always 0
				readD(); // always 0
			} else if (actionType == 2) {
				dopingAction = readD();
				if (dopingAction == 0) { // add item
					dopingItemId = readD();
					dopingSlot1 = readD();
				} else if (dopingAction == 1) { // remove item
					dopingSlot1 = readD();
					dopingItemId = readD();
				} else if (dopingAction == 2) { // switch items in two occupied slots
					dopingSlot1 = readD();
					dopingSlot2 = readD();
				} else if (dopingAction == 3) { // use doping
					dopingItemId = readD();
					dopingSlot1 = readD();
				}
			} else {
				objectId = readD();
				count = readD();
				unk2 = readD();
			}
			break;
		case model::gameobjects::PetAction::RENAME:
			objectId = readD();
			petName = readS();
			break;
		case model::gameobjects::PetAction::MOOD:
			subType = readD();
			emotionId = readD();
			break;
		case model::gameobjects::PetAction::EXTEND_EXPIRATION: // extend expiration date
			eggObjId = readD(); // itemObjId
			objectId = readD(); // petObjId
			break;
		default:
			break;
	}
}

// Java CM_PET.java:117-172
void CM_PET::runImpl() {
	runtime::Ptr<model::gameobjects::player::Player> player = getConnection()->getActivePlayer();
	if (player == nullptr)
		return;

	runtime::Ptr<model::gameobjects::Pet> pet = player->getPet();
	switch (action) {
		case model::gameobjects::PetAction::ADOPT:
			if (!services::NameRestrictionService::isValidPetName(petName) || services::NameRestrictionService::isForbidden(petName))
				utils::PacketSendUtility::sendPacket(*player, serverpackets::SM_SYSTEM_MESSAGE::STR_MSG_PET_NOT_AVALIABE_NAME());
			else
				services::toypet::PetAdoptionService::adoptPet(*player, eggObjId, templateId, petName, decorationId);
			break;
		case model::gameobjects::PetAction::EXTEND_EXPIRATION:
			// for now we will do nothing, cause expiration-time is shitty
			break;
		case model::gameobjects::PetAction::SURRENDER:
			services::toypet::PetAdoptionService::surrenderPet(*player, templateId);
			break;
		case model::gameobjects::PetAction::SPAWN:
			services::toypet::PetSpawnService::summonPet(*player, templateId);
			break;
		case model::gameobjects::PetAction::DISMISS:
			if (pet != nullptr)
				pet->getController().delete_();
			break;
		case model::gameobjects::PetAction::FOOD:
			if (pet == nullptr)
				return;
			if (actionType == 2) { // Pet doping
				services::toypet::PetService::getInstance().useDoping(*pet, dopingAction, dopingItemId, dopingSlot1, dopingSlot2);
			} else if (actionType == 3) { // Pet looting
				services::toypet::PetService::getInstance().activateLoot(*pet, activateSpecialFunction != 0);
			} else if (actionType == 4) { // Pet auto sell items
				services::toypet::PetService::getInstance().activateAutoSell(*pet, activateSpecialFunction != 0);
			} else if (objectId == 0) {
				pet->getCommonData()->setCancelFeed(true);
				utils::PacketSendUtility::sendPacket(*player, serverpackets::SM_PET(4, 0, 0, *player->getPet()));
				utils::PacketSendUtility::sendPacket(*player, serverpackets::SM_EMOTION(*player, model::EmotionType::END_FEEDING, 0, player->getObjectId()));
			} else if (pet->getCommonData()->getRefeedDelay() > 0) { // not hungry yet
				utils::PacketSendUtility::sendPacket(*player, serverpackets::SM_PET(8, objectId, count, *player->getPet()));
			} else
				services::toypet::PetService::getInstance().removeObject(objectId, count, *player);
			break;
		case model::gameobjects::PetAction::RENAME:
			if (!services::NameRestrictionService::isValidPetName(petName) || services::NameRestrictionService::isForbidden(petName))
				utils::PacketSendUtility::sendPacket(*player, serverpackets::SM_SYSTEM_MESSAGE::STR_MSG_PET_NOT_AVALIABE_NAME());
			else
				services::toypet::PetService::getInstance().renamePet(*player, petName);
			break;
		case model::gameobjects::PetAction::MOOD:
			if (pet != nullptr && ((subType == 0 && pet->getCommonData()->getMoodRemainingTime() == 0) ||
				(subType == 3 && pet->getCommonData()->getGiftRemainingTime() == 0) || emotionId != 0)) {
				services::toypet::PetMoodService::checkMood(*pet, subType, emotionId);
			}
			break;
		default:
			break;
	}
}

AION_CLIENT_PACKET(CM_PET);

} // namespace aion::gameserver::network::aion::clientpackets
