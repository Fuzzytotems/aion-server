#pragma once

#include <cstdint>
#include <memory>

#include "aion/loginserver/network/aion/AionClientPacket.h"

namespace aion::loginserver::network::aion::clientpackets {

/**
 * This packet is send when client was connected to game server and now is reconnection to login server.
 * <p>
 * Java: com.aionemu.loginserver.network.aion.clientpackets.CM_UPDATE_SESSION
 *
 * @author -Nemesiss-
 */
class CM_UPDATE_SESSION : public AionClientPacket {
public:
	/** Constructs new instance of <tt>CM_UPDATE_SESSION </tt> packet. */
	CM_UPDATE_SESSION(commons::utils::ByteBuffer buf, std::shared_ptr<LoginConnection> client, int32_t opCode)
		: AionClientPacket(std::move(buf), std::move(client), opCode) {}

protected:
	void readImpl() override;
	void runImpl() override;

private:
	/** accountId is part of session key - its used for security purposes */
	int32_t accountId = 0;
	/** loginOk is part of session key - its used for security purposes */
	int32_t loginOk = 0;
	/** reconectKey is key that server sends to client for fast reconnection to login server - we will check if this key is valid. */
	int32_t reconnectKey = 0;
};

} // namespace aion::loginserver::network::aion::clientpackets
