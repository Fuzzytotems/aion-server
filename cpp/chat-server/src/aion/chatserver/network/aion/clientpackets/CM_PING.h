#pragma once

#include <memory>

#include "aion/chatserver/network/aion/AbstractClientPacket.h"

namespace aion::chatserver::network::aion::clientpackets {

/**
 * Client sends this packet every 10 seconds after connecting to chat server
 * <p>
 * Java: com.aionemu.chatserver.network.aion.clientpackets.CM_PING
 *
 * @author Neon
 */
class CM_PING : public AbstractClientPacket {
public:
	CM_PING(commons::utils::ByteBuffer buf, std::shared_ptr<AionConnection> connection, int32_t opCode)
		: AbstractClientPacket(std::move(buf), std::move(connection), opCode) {}

protected:
	void readImpl() override;
	void runImpl() override {}
};

} // namespace aion::chatserver::network::aion::clientpackets
