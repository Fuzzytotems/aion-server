#include "aion/gameserver/network/chatserver/serverpackets/SM_CS_PLAYER_LOGOUT.h"

namespace aion::gameserver::network::chatserver::serverpackets {

SM_CS_PLAYER_LOGOUT::SM_CS_PLAYER_LOGOUT(int32_t playerIdValue) : CsServerPacket(0x02), playerId(playerIdValue) {
}

void SM_CS_PLAYER_LOGOUT::writeImpl(ChatServerConnection* con, commons::utils::ByteBuffer& buf) {
	writeD(buf, playerId);
}

} // namespace aion::gameserver::network::chatserver::serverpackets
