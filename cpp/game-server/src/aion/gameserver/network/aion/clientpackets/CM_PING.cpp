#include "aion/gameserver/network/aion/clientpackets/CM_PING.h"

#include <memory>
#include <string>

#include "aion/commons/utils/TimeUtils.h"
#include "aion/gameserver/configs/main/SecurityConfig.h"
#include "aion/gameserver/handlers/HandlerRegistry.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/network/aion/AionConnection.h"
#include "aion/gameserver/network/aion/serverpackets/SM_PONG.h"
#include "aion/gameserver/utils/audit/AuditLogger.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets {

CM_PING::CM_PING(int32_t opcode, const StateSet& validStates) : AionClientPacket(opcode, validStates) {
}

void CM_PING::readImpl() {
	readH(); // unk
}

void CM_PING::runImpl() {
	const std::shared_ptr<AionConnection>& con = getConnection();
	int64_t lastPingMillis = con->getLastPingTime();
	int64_t nowMillis = commons::utils::currentTimeMillis();
	con->setLastPingTime(nowMillis);
	sendPacket(serverpackets::SM_PONG());
	if (lastPingMillis > 0) {
		int64_t pingInterval = nowMillis - lastPingMillis;
		if (pingInterval + 2000 < CLIENT_PING_INTERVAL) { // client timer cheat
			if (con->increaseAndGetPingFailCount() == 3) {
				// C++: the active player is null in AUTHED state; the Ptr overload of AuditLogger::log keeps Java's null behaviour (header request 5a-pre-4)
				if (configs::main::SecurityConfig::PINGCHECK_KICK.load()) {
					utils::audit::AuditLogger::log(con->getActivePlayer(), "possibly using time/speed hack (client ping interval: " + std::to_string(pingInterval) +
						"/" + std::to_string(CLIENT_PING_INTERVAL) + "), kicking player");
					con->close();
				} else {
					utils::audit::AuditLogger::log(con->getActivePlayer(), "possibly using time/speed hack (client ping interval: " + std::to_string(pingInterval) +
						"/" + std::to_string(CLIENT_PING_INTERVAL) + ")");
					con->resetPingFailCount();
				}
			}
		} else {
			con->resetPingFailCount();
		}
	}
}

AION_CLIENT_PACKET(CM_PING);

} // namespace aion::gameserver::network::aion::clientpackets
