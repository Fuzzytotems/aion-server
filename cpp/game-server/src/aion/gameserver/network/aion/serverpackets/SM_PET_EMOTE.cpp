#include "aion/gameserver/network/aion/serverpackets/SM_PET_EMOTE.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/model/gameobjects/Pet.h"
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
	AION_UNPORTED();
}

} // namespace aion::gameserver::network::aion::serverpackets
