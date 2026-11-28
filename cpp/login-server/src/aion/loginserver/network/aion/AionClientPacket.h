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

	/**
	 * Reads the fields, then the padding to whole Blowfish blocks (up to 7 bytes) that the client puts between them and the checksum. Anything
	 * longer stays unread and gets reported.
	 */
	void readImpl() final;

	virtual void readFields() = 0;

	/** Send new AionServerPacket to connection that is owner of this packet. This method is equvalent to: getConnection()->sendPacket(msg); */
	void sendPacket(const AionServerPacket& msg) const;
};

} // namespace aion::loginserver::network::aion
