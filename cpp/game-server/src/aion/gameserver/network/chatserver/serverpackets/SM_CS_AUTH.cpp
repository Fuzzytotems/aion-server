#include "aion/gameserver/network/chatserver/serverpackets/SM_CS_AUTH.h"

#include "aion/gameserver/configs/network/NetworkConfig.h"

namespace aion::gameserver::network::chatserver::serverpackets {

SM_CS_AUTH::SM_CS_AUTH() : CsServerPacket(0x00) {
}

void SM_CS_AUTH::writeImpl(ChatServerConnection* con, commons::utils::ByteBuffer& buf) {
	writeC(buf, configs::network::NetworkConfig::GAMESERVER_ID.load());
	writeS(buf, *configs::network::NetworkConfig::CHAT_PASSWORD.get());
}

} // namespace aion::gameserver::network::chatserver::serverpackets
