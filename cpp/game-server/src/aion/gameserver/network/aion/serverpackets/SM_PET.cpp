#include "aion/gameserver/network/aion/serverpackets/SM_PET.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/model/gameobjects/Pet.h"
#include "aion/gameserver/model/gameobjects/PetAction.h"
#include "aion/gameserver/model/gameobjects/player/PetCommonData.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_PET::SM_PET(int32_t subTypeValue, int32_t itemObjectIdValue, int32_t countValue, model::gameobjects::Pet& petValue)
	: AionServerPacket(opcodeOf<SM_PET>), action(model::gameobjects::PetAction::FOOD), itemObjectId(itemObjectIdValue), count(countValue),
	  subType(subTypeValue) {
	AION_UNPORTED();
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
	AION_UNPORTED();
}

SM_PET::SM_PET(int32_t dopeActionValue, int32_t itemId, int32_t slot)
	: AionServerPacket(opcodeOf<SM_PET>), action(model::gameobjects::PetAction::SPECIAL_FUNCTION), itemObjectId(itemId), dopeAction(dopeActionValue),
	  dopeSlot(slot) {
	AION_UNPORTED();
}

SM_PET::SM_PET(model::gameobjects::Pet& petValue, int32_t subTypeValue, int32_t shuggleEmotionValue)
	: AionServerPacket(opcodeOf<SM_PET>), action(model::gameobjects::PetAction::MOOD), subType(subTypeValue), shuggleEmotion(shuggleEmotionValue) {
	AION_UNPORTED();
}

SM_PET::SM_PET(int32_t petObjectIdValue, model::animations::ObjectDeleteAnimation animation)
	: AionServerPacket(opcodeOf<SM_PET>), action(model::gameobjects::PetAction::DISMISS), petObjectId(petObjectIdValue) {
	AION_UNPORTED();
}

SM_PET::~SM_PET() = default;

void SM_PET::writeImpl(AionConnection* con) {
	AION_UNPORTED();
}

void SM_PET::writePetData(model::gameobjects::player::PetCommonData& petCommonData) {
	AION_UNPORTED();
}

void SM_PET::writeAppearance(model::gameobjects::player::PetCommonData& petCommonData) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::network::aion::serverpackets
