#pragma once

#include <memory>

#include "aion/commons/network/packet/BaseClientPacket.h"
#include "aion/loginserver/network/gameserver/GsConnection.h"
#include "aion/loginserver/network/gameserver/GsServerPacket.h"

namespace aion::loginserver {
class GameServerInfo;
}

namespace aion::loginserver::network::gameserver {

/**
 * Base class for every GameServer -> LS Client Packet. The opcode of all game server client packets is 0 (like Java, the factory does not set
 * it).
 * <p>
 * Java: com.aionemu.loginserver.network.gameserver.GsClientPacket
 *
 * @author -Nemesiss-
 */
class GsClientPacket : public commons::network::packet::BaseClientPacket<GsConnection> {
public:
	/** run runImpl catching and logging exceptions ("error handling gs (&lt;ip&gt;) message &lt;packet&gt;"). */
	void run() final;

	/**
	 * C++ addition: true if GsConnection::processData runs this packet right away on the IO thread instead of queueing it on the
	 * PacketProcessor (see GsConnection). Only for packets whose runImpl is cheap, non-blocking and independent of the order of the other packets.
	 */
	virtual bool isRunOnReceive() const noexcept { return false; }

protected:
	GsClientPacket() noexcept : BaseClientPacket(0) {}

	/** Send new GsServerPacket to connection that is owner of this packet. This method is equivalent to: getConnection()->sendPacket(msg); */
	void sendPacket(std::shared_ptr<GsServerPacket> msg) const;

	/**
	 * C++ helper for Java's getConnection().getGameServerInfo(), which is never null for packets of authenticated game servers.
	 * @throws commons::utils::IllegalStateException if the connection has no GameServerInfo (anymore) (Java: NullPointerException)
	 */
	std::shared_ptr<GameServerInfo> getGameServerInfo() const;
};

} // namespace aion::loginserver::network::gameserver
