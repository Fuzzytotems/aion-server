#include "aion/gameserver/network/aion/serverpackets/SM_PLAYER_INFO.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/network/aion/AionServerPacket.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_PLAYER_INFO::SM_PLAYER_INFO(model::gameobjects::player::Player& playerValue)
	: SM_PLAYER_INFO(playerValue, false) {
}

SM_PLAYER_INFO::SM_PLAYER_INFO(model::gameobjects::player::Player& playerValue, bool enemyValue)
	: AbstractPlayerInfoPacket(opcodeOf<SM_PLAYER_INFO>), player(playerValue), enemy(enemyValue) {
}

SM_PLAYER_INFO::~SM_PLAYER_INFO() = default;

void SM_PLAYER_INFO::writeImpl(AionConnection* con) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::network::aion::serverpackets
