#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "aion/loginserver/network/gameserver/GsClientPacket.h"

namespace aion::loginserver::network::gameserver::clientpackets {

/**
 * This is authentication packet that gs will send to login server for registration.
 * <p>
 * Java: com.aionemu.loginserver.network.gameserver.clientpackets.CM_GS_AUTH
 *
 * @author -Nemesiss-
 */
class CM_GS_AUTH : public GsClientPacket {
protected:
	void readImpl() override;
	void runImpl() override;

private:
	/** Password for authentication */
	std::string password;
	/** Id of GameServer */
	int8_t gameServerId = 0;
	/** Maximum number of players that this Gameserver can accept. */
	int32_t maxPlayers = 0;
	int8_t minAccessLevel = 0;
	/** Port of this Gameserver. */
	int32_t port = 0;
	/** Default address for server */
	std::vector<uint8_t> ip;
};

} // namespace aion::loginserver::network::gameserver::clientpackets
