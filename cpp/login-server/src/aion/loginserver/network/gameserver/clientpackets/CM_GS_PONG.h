#pragma once

#include "aion/loginserver/network/gameserver/GsClientPacket.h"

namespace aion::loginserver::network::gameserver::clientpackets {

/**
 * Answer to SM_PING (see PingPongTask).
 * <p>
 * Java: com.aionemu.loginserver.network.gameserver.clientpackets.CM_GS_PONG
 *
 * @author KID
 */
class CM_GS_PONG : public GsClientPacket {
public:
	/**
	 * Runs on the IO thread (an atomic store), so a pong is counted even while an earlier, slow packet of the same game server (e.g. a large
	 * CM_ACCOUNT_LIST) still runs. Java runs it on its cached thread pool concurrently with the other packets; queued behind them, the pongs
	 * would wait and the PingPongTask would close a healthy game server.
	 */
	bool isRunOnReceive() const noexcept override { return true; }

protected:
	void readImpl() override {}
	void runImpl() override { getConnection()->getPingPongTask().onReceivePong(); }
};

} // namespace aion::loginserver::network::gameserver::clientpackets
