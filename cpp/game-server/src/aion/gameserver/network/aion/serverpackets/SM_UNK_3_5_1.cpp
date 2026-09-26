#include "aion/gameserver/network/aion/serverpackets/SM_UNK_3_5_1.h"

#include "aion/gameserver/configs/network/NetworkConfig.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/network/aion/AionConnection.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"
#include "aion/gameserver/network/aion/serverpackets/detail/PacketSupport.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_UNK_3_5_1::SM_UNK_3_5_1()
	: AionServerPacket(opcodeOf<SM_UNK_3_5_1>) {
}

void SM_UNK_3_5_1::writeImpl(AionConnection* con) {
	writeD(1);
	writeD(0);
	writeD(detail::requireConnection(con, "SM_UNK_3_5_1").getActivePlayer()->getObjectId());
	writeD(configs::network::NetworkConfig::GAMESERVER_ID.load());
	writeD(0);
	writeD(0);
}

} // namespace aion::gameserver::network::aion::serverpackets
