#include "aion/chatserver/network/gameserver/serverpackets/SM_GS_AUTH_RESPONSE.h"

#include <cstdint>
#include <vector>

#include "aion/chatserver/configs/network/NetworkConfig.h"

namespace aion::chatserver::network::gameserver::serverpackets {

using configs::network::NetworkConfig;

void SM_GS_AUTH_RESPONSE::writeImpl(GsConnection* con, commons::utils::ByteBuffer& buf) const {
	writeC(buf, 0);
	writeC(buf, getResponseId(response));
	if (response == GsAuthResponse::AUTHED) {
		std::vector<uint8_t> csIp = NetworkConfig::CLIENT_CONNECT_ADDRESS.resolveAddressBytes();
		writeC(buf, static_cast<int32_t>(csIp.size()));
		writeB(buf, csIp);
		writeH(buf, NetworkConfig::CLIENT_CONNECT_ADDRESS.port);
	}
}

} // namespace aion::chatserver::network::gameserver::serverpackets
