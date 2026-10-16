#pragma once

#include <cstdint>
#include <memory>

#include "aion/loginserver/network/aion/AionClientPacket.h"

namespace aion::loginserver::network::aion::clientpackets {

/**
 * The client wants to play on a game server.
 * <p>
 * Java: com.aionemu.loginserver.network.aion.clientpackets.CM_PLAY
 *
 * @author -Nemesiss-
 */
class CM_PLAY : public AionClientPacket {
public:
	CM_PLAY(commons::utils::ByteBuffer buf, std::shared_ptr<LoginConnection> client, int32_t opCode)
		: AionClientPacket(std::move(buf), std::move(client), opCode) {}

protected:
	void readImpl() override;
	void runImpl() override;

private:
	/** accountId is part of session key - its used for security purposes */
	int32_t accountId = 0;
	/** loginOk is part of session key - its used for security purposes */
	int32_t loginOk = 0;
	/** Id of game server that this client is trying to play on. */
	int8_t servId = 0;
};

} // namespace aion::loginserver::network::aion::clientpackets
