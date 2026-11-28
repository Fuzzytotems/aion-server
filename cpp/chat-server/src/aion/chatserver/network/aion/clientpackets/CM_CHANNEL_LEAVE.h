#pragma once

#include <cstdint>
#include <memory>

#include "aion/chatserver/network/aion/AbstractClientPacket.h"

namespace aion::chatserver::network::aion::clientpackets {

/**
 * Request to leave a channel (sent on map change, logout or manually via /leavechannel)
 * <p>
 * Java: com.aionemu.chatserver.network.aion.clientpackets.CM_CHANNEL_LEAVE
 *
 * @author Neon
 */
class CM_CHANNEL_LEAVE : public AbstractClientPacket {
public:
	CM_CHANNEL_LEAVE(commons::utils::ByteBuffer buf, std::shared_ptr<AionConnection> connection, int32_t opCode)
		: AbstractClientPacket(std::move(buf), std::move(connection), opCode) {}

protected:
	void readImpl() override;
	void runImpl() override;

private:
	int32_t channelId = 0;
};

} // namespace aion::chatserver::network::aion::clientpackets
