#pragma once

#include <cstdint>
#include <string>

#include "aion/chatserver/network/gameserver/GsClientPacket.h"

namespace aion::chatserver::network::gameserver::clientpackets {

/**
 * The game server's registration with its id and the password: answered with SM_GS_AUTH_RESPONSE.
 * <p>
 * Java: com.aionemu.chatserver.network.gameserver.clientpackets.CM_CS_AUTH
 *
 * @author ATracer
 */
class CM_CS_AUTH : public GsClientPacket {
public:
	CM_CS_AUTH() noexcept : GsClientPacket(0x00) {}

protected:
	void readImpl() override;
	void runImpl() override;

private:
	std::string password;
	int8_t gameServerId = 0;
};

} // namespace aion::chatserver::network::gameserver::clientpackets
