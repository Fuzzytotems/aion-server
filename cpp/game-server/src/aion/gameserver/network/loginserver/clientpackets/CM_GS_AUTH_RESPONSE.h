#pragma once

#include <cstdint>

#include "aion/gameserver/network/loginserver/LsClientPacket.h"
#include "aion/gameserver/network/loginserver/clientpackets/fwd.h"

namespace aion::gameserver::network::loginserver::clientpackets {

/**
 * This packet is response for SM_GS_AUTH its notify Gameserver if registration was ok or what was wrong.
 *
 * @author -Nemesiss-, Neon
 */
class CM_GS_AUTH_RESPONSE : public LsClientPacket {
private:
	int32_t response = 0;
	int32_t serverCount = 0;

public:
	explicit CM_GS_AUTH_RESPONSE(int32_t opCode);

	void readImpl() override;

	void runImpl() override;
};

} // namespace aion::gameserver::network::loginserver::clientpackets
