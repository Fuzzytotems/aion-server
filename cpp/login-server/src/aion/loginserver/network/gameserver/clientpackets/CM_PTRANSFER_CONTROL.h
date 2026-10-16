#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "aion/loginserver/network/gameserver/GsClientPacket.h"

namespace aion::loginserver::network::gameserver::clientpackets {

/**
 * Player transfer messages from the game servers: 1 = request transfer (character data), 2 = error, 3 = ok, 4 = task stop.
 * <p>
 * Deviation: Java calls PlayerTransferService from readImpl, i.e. on the network thread (blocking it with database work); here readImpl only reads
 * the fields and runImpl calls the service on the packet processor. Exceptions of the service are therefore logged as "error handling gs"
 * instead of "Reading failed for packet".
 * <p>
 * Java: com.aionemu.loginserver.network.gameserver.clientpackets.CM_PTRANSFER_CONTROL
 *
 * @author KID
 */
class CM_PTRANSFER_CONTROL : public GsClientPacket {
protected:
	void readImpl() override;
	void runImpl() override;

private:
	int8_t actionId = 0;
	int32_t taskId = 0;
	std::string text;
	std::vector<uint8_t> db;
};

} // namespace aion::loginserver::network::gameserver::clientpackets
