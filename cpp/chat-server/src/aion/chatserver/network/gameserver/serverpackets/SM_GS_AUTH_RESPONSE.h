#pragma once

#include "aion/chatserver/network/gameserver/GsAuthResponse.h"
#include "aion/chatserver/network/gameserver/GsServerPacket.h"

namespace aion::chatserver::network::gameserver::serverpackets {

/**
 * The answer to CM_CS_AUTH. If the game server was registered, it carries the address the Aion clients connect to
 * (NetworkConfig::CLIENT_CONNECT_ADDRESS), which the game server sends to its clients in SM_VERSION_CHECK.
 * <p>
 * Java: com.aionemu.chatserver.network.gameserver.serverpackets.SM_GS_AUTH_RESPONSE
 *
 * @author ATracer
 */
class SM_GS_AUTH_RESPONSE : public GsServerPacket {
public:
	explicit SM_GS_AUTH_RESPONSE(GsAuthResponse rsp) noexcept : response(rsp) {}

protected:
	/** @throws commons::utils::IOException if the connect address cannot be resolved (see Config::load, which fails first) */
	void writeImpl(GsConnection* con, commons::utils::ByteBuffer& buf) const override;

private:
	const GsAuthResponse response;
};

} // namespace aion::chatserver::network::gameserver::serverpackets
