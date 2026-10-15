#pragma once

#include <cstdint>
#include <vector>

#include "aion/gameserver/network/chatserver/CsClientPacket.h"
#include "aion/gameserver/network/chatserver/clientpackets/fwd.h"

namespace aion::gameserver::network::chatserver::clientpackets {

/**
 * @author ATracer, Neon
 */
class CM_CS_AUTH_RESPONSE : public CsClientPacket {
private:
	int8_t response = 0;
	std::vector<uint8_t> ip;
	int32_t port = 0;

public:
	explicit CM_CS_AUTH_RESPONSE(int32_t opcode);

protected:
	void readImpl() override;

	void runImpl() override;
};

} // namespace aion::gameserver::network::chatserver::clientpackets
