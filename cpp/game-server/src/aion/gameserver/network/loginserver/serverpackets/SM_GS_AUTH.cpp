#include "aion/gameserver/network/loginserver/serverpackets/SM_GS_AUTH.h"

#include <cstdint>
#include <vector>

#include "aion/gameserver/configs/network/NetworkConfig.h"

namespace aion::gameserver::network::loginserver::serverpackets {

using configs::network::NetworkConfig;

SM_GS_AUTH::SM_GS_AUTH() : LsServerPacket(0x00) {
}

void SM_GS_AUTH::writeImpl(LoginServerConnection* con, commons::utils::ByteBuffer& buf) {
	auto clientConnectAddress = NetworkConfig::CLIENT_CONNECT_ADDRESS.get();
	std::vector<uint8_t> gsIp = clientConnectAddress->resolveAddressBytes();
	writeC(buf, NetworkConfig::GAMESERVER_ID.load());
	writeS(buf, *NetworkConfig::LOGIN_PASSWORD.get());
	writeC(buf, static_cast<int32_t>(gsIp.size()));
	writeB(buf, gsIp);
	writeH(buf, clientConnectAddress->port);
	writeC(buf, NetworkConfig::MIN_ACCESS_LEVEL.load());
	writeD(buf, NetworkConfig::MAX_ONLINE_PLAYERS.load());
}

} // namespace aion::gameserver::network::loginserver::serverpackets
