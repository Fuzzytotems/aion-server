#include "aion/gameserver/network/aion/serverpackets/SM_PLAYER_SPAWN.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_PLAYER_SPAWN::SM_PLAYER_SPAWN(model::gameobjects::player::Player& playerValue)
	: AionServerPacket(opcodeOf<SM_PLAYER_SPAWN>), player(playerValue) {
}

SM_PLAYER_SPAWN::~SM_PLAYER_SPAWN() = default;

void SM_PLAYER_SPAWN::writeImpl(AionConnection* con) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::network::aion::serverpackets
