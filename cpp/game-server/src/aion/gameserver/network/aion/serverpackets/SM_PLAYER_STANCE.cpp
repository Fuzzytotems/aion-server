#include "aion/gameserver/network/aion/serverpackets/SM_PLAYER_STANCE.h"

#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_PLAYER_STANCE::SM_PLAYER_STANCE(model::gameobjects::player::Player& playerValue, int32_t stateValue)
	: AionServerPacket(opcodeOf<SM_PLAYER_STANCE>), player(playerValue), state(stateValue) {
}

SM_PLAYER_STANCE::~SM_PLAYER_STANCE() = default;

void SM_PLAYER_STANCE::writeImpl(AionConnection* con) {
	writeD(player->getObjectId());
	writeC(state);
}

} // namespace aion::gameserver::network::aion::serverpackets
