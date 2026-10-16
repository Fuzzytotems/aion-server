#pragma once

#include <cstdint>
#include <vector>

#include "aion/gameserver/network/chatserver/CsClientPacket.h"
#include "aion/gameserver/network/chatserver/clientpackets/fwd.h"

namespace aion::gameserver::network::chatserver::clientpackets {

/**
 * @author ATracer
 */
class CM_CS_PLAYER_AUTH_RESPONSE : public CsClientPacket {
private:
	/** Player for which authentication was performed */
	int32_t playerId = 0;
	/** Token will be sent to client */
	std::vector<uint8_t> token;

public:
	explicit CM_CS_PLAYER_AUTH_RESPONSE(int32_t opcode);

protected:
	void readImpl() override;

	void runImpl() override;
};

} // namespace aion::gameserver::network::chatserver::clientpackets
