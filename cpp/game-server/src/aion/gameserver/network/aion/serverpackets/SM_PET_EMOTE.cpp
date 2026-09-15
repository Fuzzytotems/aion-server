#include "aion/gameserver/network/aion/serverpackets/SM_PET_EMOTE.h"

#include "aion/gameserver/controllers/movement/CreatureMoveController.h"
#include "aion/gameserver/model/gameobjects/Pet.h"
#include "aion/gameserver/model/gameobjects/PetEmoteInfo.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_PET_EMOTE::SM_PET_EMOTE(model::gameobjects::Pet& petValue, model::gameobjects::PetEmote emoteValue)
	: SM_PET_EMOTE(petValue, emoteValue, 0, 0) {
}

SM_PET_EMOTE::SM_PET_EMOTE(model::gameobjects::Pet& petValue, model::gameobjects::PetEmote emoteValue, int32_t emotionIdValue, int32_t param1Value)
	: AionServerPacket(opcodeOf<SM_PET_EMOTE>), pet(petValue), emote(emoteValue), emotionId(emotionIdValue), param1(param1Value) {
}

SM_PET_EMOTE::~SM_PET_EMOTE() = default;

void SM_PET_EMOTE::writeImpl(AionConnection* con) {
	writeD(pet->getObjectId());
	writeC(model::gameobjects::getEmoteId(emote));
	switch (emote) {
		case model::gameobjects::PetEmote::MOVE_STOP:
			writeF(pet->getX());
			writeF(pet->getY());
			writeF(pet->getZ());
			writeC(pet->getHeading());
			break;
		case model::gameobjects::PetEmote::MOVETO:
			writeF(pet->getX());
			writeF(pet->getY());
			writeF(pet->getZ());
			writeC(pet->getHeading());
			writeF(pet->getMoveController().getTargetX2());
			writeF(pet->getMoveController().getTargetY2());
			writeF(pet->getMoveController().getTargetZ2());
			break;
		default:
			writeC(emotionId);
			writeC(param1); // happinessAdded?
			break;
	}
}

} // namespace aion::gameserver::network::aion::serverpackets
