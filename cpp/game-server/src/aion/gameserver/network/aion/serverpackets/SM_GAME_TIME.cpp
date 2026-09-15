#include "aion/gameserver/network/aion/serverpackets/SM_GAME_TIME.h"

#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"
#include "aion/gameserver/services/GameTimeService.h"
#include "aion/gameserver/utils/time/gametime/GameTime.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_GAME_TIME::SM_GAME_TIME() : AionServerPacket(opcodeOf<SM_GAME_TIME>) {
}

void SM_GAME_TIME::writeImpl(AionConnection* con) {
	writeD(services::GameTimeService::getInstance().getGameTime()->getTime()); // Minutes since 1/1/00 00:00:00
}

} // namespace aion::gameserver::network::aion::serverpackets
