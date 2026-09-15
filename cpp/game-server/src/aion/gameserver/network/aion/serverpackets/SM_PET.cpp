#include "aion/gameserver/network/aion/serverpackets/SM_PET.h"

#include <cstddef>
#include <vector>

#include "aion/commons/utils/TimeUtils.h"
#include "aion/gameserver/controllers/movement/CreatureMoveController.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/PetData.h"
#include "aion/gameserver/model/animations/ObjectDeleteAnimationInfo.h"
#include "aion/gameserver/model/gameobjects/Pet.h"
#include "aion/gameserver/model/gameobjects/PetAction.h"
#include "aion/gameserver/model/gameobjects/PetActionInfo.h"
#include "aion/gameserver/model/gameobjects/PetSpecialFunctionInfo.h"
#include "aion/gameserver/model/gameobjects/player/PetCommonData.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/templates/pet/PetDopingBag.h"
#include "aion/gameserver/model/templates/pet/PetFunctionType.h"
#include "aion/gameserver/model/templates/pet/PetFunctionTypeInfo.h"
#include "aion/gameserver/model/templates/pet/PetTemplate.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"
#include "aion/gameserver/services/toypet/PetFeedProgress.h"
#include "aion/gameserver/world/WorldPosition.h"

namespace aion::gameserver::network::aion::serverpackets {

namespace {

/** Java: DataManager.PET_DATA.getPetTemplate(templateId), dereferenced (NullPointerException for an unknown template) */
const model::templates::pet::PetTemplate& petTemplateOf(int32_t templateId) {
	const model::templates::pet::PetTemplate* petTemplate = dataholders::DataManager::PET_DATA->getPetTemplate(templateId);
	if (petTemplate == nullptr)
		throw runtime::NullPointerException("SM_PET: no pet template " + std::to_string(templateId));
	return *petTemplate;
}

} // namespace

SM_PET::SM_PET(int32_t subTypeValue, int32_t itemObjectIdValue, int32_t countValue, model::gameobjects::Pet& petValue)
	: AionServerPacket(opcodeOf<SM_PET>), action(model::gameobjects::PetAction::FOOD), itemObjectId(itemObjectIdValue), count(countValue),
	  subType(subTypeValue) {
	commonData = runtime::Ref<model::gameobjects::player::PetCommonData>(petValue.getCommonData());
}

SM_PET::SM_PET(model::gameobjects::PetAction actionValue)
	: AionServerPacket(opcodeOf<SM_PET>), action(actionValue) {
}

SM_PET::SM_PET(int32_t petObjectIdValue, std::string_view petNameValue)
	: AionServerPacket(opcodeOf<SM_PET>), action(model::gameobjects::PetAction::RENAME), petObjectId(petObjectIdValue), petName(petNameValue) {
}

SM_PET::SM_PET(model::gameobjects::Pet& petValue)
	: AionServerPacket(opcodeOf<SM_PET>), action(model::gameobjects::PetAction::SPAWN), pet(petValue) {
}

SM_PET::SM_PET(model::gameobjects::player::PetCommonData& commonDataValue, bool isAdopt)
	: AionServerPacket(opcodeOf<SM_PET>), action(isAdopt ? model::gameobjects::PetAction::ADOPT : model::gameobjects::PetAction::SURRENDER),
	  commonData(commonDataValue) {
}

SM_PET::SM_PET(int32_t petId, int32_t petObjectIdValue)
	: AionServerPacket(opcodeOf<SM_PET>), action(model::gameobjects::PetAction::SURRENDER), petObjectId(petObjectIdValue) {
}

SM_PET::SM_PET(const std::vector<runtime::Ptr<model::gameobjects::player::PetCommonData>>& petsValue)
	: AionServerPacket(opcodeOf<SM_PET>), action(model::gameobjects::PetAction::LOAD_PETS), pets(petsValue.begin(), petsValue.end()) {
}

SM_PET::SM_PET(model::gameobjects::PetSpecialFunction specialFunction, bool active)
	: SM_PET(specialFunction, active, 0) {
}

SM_PET::SM_PET(model::gameobjects::PetSpecialFunction specialFunction, bool active, int32_t npcObjId)
	: AionServerPacket(opcodeOf<SM_PET>), action(model::gameobjects::PetAction::SPECIAL_FUNCTION), isActing(active), lootNpcObjId(npcObjId) {
	subType = model::gameobjects::getId(specialFunction);
}

SM_PET::SM_PET(int32_t dopeActionValue, int32_t itemId, int32_t slot)
	: AionServerPacket(opcodeOf<SM_PET>), action(model::gameobjects::PetAction::SPECIAL_FUNCTION), itemObjectId(itemId), dopeAction(dopeActionValue),
	  dopeSlot(slot) {
	subType = model::gameobjects::getId(model::gameobjects::PetSpecialFunction::DOPING);
	// itemObjectId is a template ID, not an objectId; it is also misused as slot2 for the slot switch action (dopeAction 2)
}

SM_PET::SM_PET(model::gameobjects::Pet& petValue, int32_t subTypeValue, int32_t shuggleEmotionValue)
	: AionServerPacket(opcodeOf<SM_PET>), action(model::gameobjects::PetAction::MOOD), subType(subTypeValue), shuggleEmotion(shuggleEmotionValue) {
	commonData = runtime::Ref<model::gameobjects::player::PetCommonData>(petValue.getCommonData());
}

SM_PET::SM_PET(int32_t petObjectIdValue, model::animations::ObjectDeleteAnimation animation)
	: AionServerPacket(opcodeOf<SM_PET>), action(model::gameobjects::PetAction::DISMISS), petObjectId(petObjectIdValue) {
	animationId = model::animations::getId(animation);
}

SM_PET::~SM_PET() = default;

void SM_PET::writeImpl(AionConnection* con) {
	using model::gameobjects::PetAction;
	writeH(model::gameobjects::getActionId(action));
	switch (action) {
		case PetAction::LOAD_PETS: // load list on login
			writeC(0); // unk
			writeH(static_cast<int32_t>(pets.size()));
			for (const runtime::Ref<model::gameobjects::player::PetCommonData>& petCommonData : pets)
				writePetData(*petCommonData);
			break;
		case PetAction::ADOPT:
			writePetData(*commonData);
			break;
		case PetAction::SURRENDER:
			writeD(commonData->getTemplateId());
			writeD(commonData->getObjectId());
			writeD(0); // unk
			writeD(0); // unk
			break;
		case PetAction::SPAWN:
			writeS(pet->getName());
			writeD(pet->getObjectTemplate()->getTemplateId());
			writeD(pet->getObjectId());
			writeF(pet->getPosition()->getX());
			writeF(pet->getPosition()->getY());
			writeF(pet->getPosition()->getZ());
			writeF(pet->getMoveController().getTargetX2());
			writeF(pet->getMoveController().getTargetY2());
			writeF(pet->getMoveController().getTargetZ2());
			writeC(pet->getHeading());
			writeD(pet->getMaster()->getObjectId());
			writeAppearance(*pet->getCommonData());
			break;
		case PetAction::DISMISS:
			writeD(petObjectId);
			writeC(animationId);
			break;
		case PetAction::FOOD:
			writeH(1);
			writeC(1);
			writeC(subType);
			switch (subType) {
				case 1: // eat
					writeD(commonData->getFeedProgress()->getDataForPacket());
					writeD(0);
					writeD(itemObjectId);
					writeD(count);
					break;
				case 2: // eating successful
					writeD(commonData->getFeedProgress()->getDataForPacket());
					writeD(0);
					writeD(itemObjectId);
					writeD(count);
					writeC(0);
					break;
				case 3: // not hungry
				case 4: // cancel feed
				case 5: // clean feed task
					writeD(commonData->getFeedProgress()->getDataForPacket());
					writeD(static_cast<int32_t>(commonData->getRefeedDelay()) / 1000); // Java: (int) delay / 1000 (the cast binds first)
					break;
				case 6: // give item
					writeD(commonData->getFeedProgress()->getDataForPacket());
					writeD(0);
					writeD(itemObjectId);
					writeC(0);
					break;
				case 7: // present notification
					writeD(commonData->getFeedProgress()->getDataForPacket());
					writeD(static_cast<int32_t>(commonData->getRefeedDelay()) / 1000); // time
					writeD(itemObjectId);
					writeD(0);
					break;
				case 8: // is full
					writeD(commonData->getFeedProgress()->getDataForPacket());
					writeD(static_cast<int32_t>(commonData->getRefeedDelay()) / 1000);
					writeD(itemObjectId);
					writeD(count);
					break;
			}
			break;
		case PetAction::RENAME:
			writeD(petObjectId);
			writeS(petName);
			break;
		case PetAction::MOOD:
			switch (subType) {
				case 0: // check pet status
					writeC(subType);
					// desynced feedback data, need to send delta in percents
					if (commonData->getLastSentPoints() < commonData->getMoodPoints(true)) {
						writeD(commonData->getMoodPoints(true) - commonData->getLastSentPoints());
					} else {
						writeD(0);
						commonData->setLastSentPoints(commonData->getMoodPoints(true));
					}
					break;
				case 2: // emotion sent
					writeC(subType);
					writeD(0);
					writeD(commonData->getMoodPoints(true));
					writeD(shuggleEmotion);
					commonData->setLastSentPoints(commonData->getMoodPoints(true));
					commonData->setMoodCdStarted(commons::utils::currentTimeMillis());
					break;
				case 3: // give gift
					writeC(subType);
					writeD(petTemplateOf(commonData->getTemplateId()).getConditionReward());
					commonData->setGiftCdStarted(commons::utils::currentTimeMillis());
					break;
				case 4: // periodic update
					writeC(subType);
					writeD(commonData->getMoodPoints(true));
					writeD(commonData->getMoodRemainingTime());
					writeD(commonData->getGiftRemainingTime());
					commonData->setLastSentPoints(commonData->getMoodPoints(true));
					break;
			}
			break;
		case PetAction::SPECIAL_FUNCTION:
			writeC(subType);
			if (subType == 2) {
				writeC(dopeAction);
				switch (dopeAction) {
					case 0: // add item
						writeD(itemObjectId);
						writeD(dopeSlot);
						break;
					case 1: // remove item
						writeD(dopeSlot);
						break;
					case 2: // move item from one slot to other
						writeD(dopeSlot); // slot 1
						writeD(itemObjectId); // slot 2
						break;
					case 3: // use item
						writeD(itemObjectId);
						break;
				}
			} else if (subType == 3) {
				// looting NPC
				if (lootNpcObjId > 0) {
					writeC(isActing ? 1 : 2); // 0x02 display looted msg.
					writeD(lootNpcObjId);
				} else {
					// loot function activation
					writeC(0);
					writeC(isActing ? 1 : 0);
				}
			} else if (subType == 4) {
				writeC(0);
				writeC(isActing ? 1 : 0);
			}
			break;
		default:
			break;
	}
}

void SM_PET::writePetData(model::gameobjects::player::PetCommonData& petCommonData) {
	using model::templates::pet::PetFunctionType;
	using model::templates::pet::getId;
	const model::templates::pet::PetTemplate& petTemplate = petTemplateOf(petCommonData.getTemplateId());
	writeS(petCommonData.getName());
	writeD(petCommonData.getTemplateId());
	writeD(petCommonData.getObjectId());
	writeD(petCommonData.getMasterObjectId());
	writeD(0);
	writeD(0);
	writeD(petCommonData.getBirthday());
	writeD(petCommonData.secondsUntilExpiration()); // accompanying time
	int32_t specialtyCount = 0;
	if (petTemplate.containsFunction(PetFunctionType::WAREHOUSE)) {
		writeC(getId(PetFunctionType::WAREHOUSE));
		writeC(0); // length of following bytes
		specialtyCount++;
	}
	if (petTemplate.containsFunction(PetFunctionType::LOOT)) {
		writeC(getId(PetFunctionType::LOOT));
		writeC(1); // length of following bytes
		writeC(0);
		specialtyCount++;
	}
	if (petTemplate.containsFunction(PetFunctionType::DOPING)) {
		using model::templates::pet::PetDopingBag;
		writeC(getId(PetFunctionType::DOPING));
		writeC(PetDopingBag::MAX_ITEMS * 4); // length of following bytes (always write MAX_ITEMS, otherwise some pets show items of other pets)
		std::vector<int32_t> items = petCommonData.getDopingBag()->getItems();
		for (size_t i = 0; i < PetDopingBag::MAX_ITEMS; i++)
			writeD(i < items.size() ? items[i] : 0);
		specialtyCount++;
	}
	if (petTemplate.containsFunction(PetFunctionType::FOOD)) {
		writeC(getId(PetFunctionType::FOOD));
		writeC(8); // length of following bytes
		writeD(petCommonData.getFeedProgress()->getDataForPacket());
		writeD(static_cast<int32_t>(petCommonData.getRefeedDelay() / 1000));
		specialtyCount++;
	}
	// Pets have only 2 functions max. If absent filled with NONE
	if (specialtyCount == 0) {
		writeH(getId(PetFunctionType::NONE));
		writeH(getId(PetFunctionType::NONE));
	} else if (specialtyCount == 1) {
		writeH(getId(PetFunctionType::NONE));
	}
	writeAppearance(petCommonData);
}

void SM_PET::writeAppearance(model::gameobjects::player::PetCommonData& petCommonData) {
	writeH(model::templates::pet::getId(model::templates::pet::PetFunctionType::APPEARANCE));
	writeC(0); // not implemented color R ?
	writeC(0); // not implemented color G ?
	writeC(0); // not implemented color B ?
	writeD(petCommonData.getDecoration());
	writeD(0); // wings ID if customize_attach = 1
	writeD(0); // unk
}

} // namespace aion::gameserver::network::aion::serverpackets
