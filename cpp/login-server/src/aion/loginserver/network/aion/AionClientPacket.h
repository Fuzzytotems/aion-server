#pragma once

#include <cstdint>
#include <memory>

#include "aion/commons/network/packet/BaseClientPacket.h"
#include "aion/loginserver/network/aion/AionServerPacket.h"
#include "aion/loginserver/network/aion/LoginConnection.h"

namespace aion::loginserver::network::aion {

/**
 * Base class for every Aion -> LS Client Packet
 * <p>
 * Java: com.aionemu.loginserver.network.aion.AionClientPacket
 *
 * @author -Nemesiss-
 */
class AionClientPacket : public commons::network::packet::BaseClientPacket<LoginConnection> {
public:
	/** run runImpl catching and logging exceptions ("error handling client (&lt;account name or ip&gt;) message &lt;packet&gt;"). */
	void run() final;

protected:
	/**
	 * Constructs new client packet.
	 *
	 * @param buf packet data
	 * @param client client
	 * @param opcode packet id
	 */
	AionClientPacket(commons::utils::ByteBuffer buf, std::shared_ptr<LoginConnection> client, int32_t opcode);

	/** Send new AionServerPacket to connection that is owner of this packet. This method is equvalent to: getConnection()->sendPacket(msg); */
	void sendPacket(std::shared_ptr<AionServerPacket> msg) const;
};

} // namespace aion::loginserver::network::aion
