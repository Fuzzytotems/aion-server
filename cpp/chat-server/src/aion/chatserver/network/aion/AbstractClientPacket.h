#pragma once

#include <cstdint>
#include <memory>

#include "aion/commons/network/packet/BaseClientPacket.h"

namespace aion::chatserver::model {
class ChatClient;
}

namespace aion::chatserver::network::aion {

class AionConnection;

/**
 * Base class of the packets received from Aion clients, created by ClientPacketHandler for their connection.
 * <p>
 * Java: com.aionemu.chatserver.network.aion.AbstractClientPacket
 *
 * @author ATracer
 */
class AbstractClientPacket : public commons::network::packet::BaseClientPacket<AionConnection> {
public:
	/** Runs runImpl(), logging any exception ("Could not execute &lt;packet&gt; from &lt;connection&gt;"). */
	void run() final;

protected:
	/** @param buf the packet body, positioned after the opcode */
	AbstractClientPacket(commons::utils::ByteBuffer buf, std::shared_ptr<AionConnection> connection, int32_t opCode);

	/**
	 * C++ helper for Java's getConnection().getChatClient(), which is never null for the packets of an authenticated client.
	 * @throws commons::utils::IllegalStateException if the connection has no ChatClient (Java: NullPointerException)
	 */
	std::shared_ptr<model::ChatClient> getChatClient() const;
};

} // namespace aion::chatserver::network::aion
