#pragma once

#include <cstdint>
#include <memory>

#include "aion/chatserver/network/gameserver/GsConnection.h"
#include "aion/chatserver/network/gameserver/GsServerPacket.h"
#include "aion/commons/network/packet/BaseClientPacket.h"

namespace aion::chatserver::network::gameserver {

/**
 * Base class for every GameServer -&gt; CS Client Packet. The factory sets the connection and the buffer.
 * <p>
 * Java: com.aionemu.chatserver.network.gameserver.GsClientPacket
 *
 * @author KID
 */
class GsClientPacket : public commons::network::packet::BaseClientPacket<GsConnection> {
public:
	/** run runImpl catching and logging exceptions ("error handling gs (&lt;ip&gt;) message &lt;packet&gt;"). */
	void run() final;

protected:
	explicit GsClientPacket(int32_t opCode) noexcept : BaseClientPacket(opCode) {}

	/** Send new GsServerPacket to connection that is owner of this packet. This method is equivalent to: getConnection()->sendPacket(msg); */
	void sendPacket(std::shared_ptr<GsServerPacket> msg) const;
};

} // namespace aion::chatserver::network::gameserver
