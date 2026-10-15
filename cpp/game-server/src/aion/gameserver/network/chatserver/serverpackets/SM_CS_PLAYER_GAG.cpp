#include "aion/gameserver/network/chatserver/serverpackets/SM_CS_PLAYER_GAG.h"

namespace aion::gameserver::network::chatserver::serverpackets {

SM_CS_PLAYER_GAG::SM_CS_PLAYER_GAG(int32_t playerIdValue, int64_t gagTimeValue) : CsServerPacket(0x03), playerId(playerIdValue), gagTime(gagTimeValue) {
}

void SM_CS_PLAYER_GAG::writeImpl(ChatServerConnection* con, commons::utils::ByteBuffer& buf) {
	writeD(buf, playerId);
	writeQ(buf, gagTime);
}

} // namespace aion::gameserver::network::chatserver::serverpackets
