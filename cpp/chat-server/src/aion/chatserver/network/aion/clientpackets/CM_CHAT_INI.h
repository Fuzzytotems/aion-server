#pragma once

#include <memory>

#include "aion/chatserver/network/aion/AbstractClientPacket.h"

namespace aion::chatserver::network::aion::clientpackets {

/**
 * The first packet of a client, answered with SM_CHAT_INI.
 * <p>
 * Java: com.aionemu.chatserver.network.aion.clientpackets.CM_CHAT_INI
 *
 * @author ginho1
 */
class CM_CHAT_INI : public AbstractClientPacket {
public:
	CM_CHAT_INI(commons::utils::ByteBuffer buf, std::shared_ptr<AionConnection> connection, int32_t opCode)
		: AbstractClientPacket(std::move(buf), std::move(connection), opCode) {}

protected:
	void readImpl() override;
	void runImpl() override;
};

} // namespace aion::chatserver::network::aion::clientpackets
