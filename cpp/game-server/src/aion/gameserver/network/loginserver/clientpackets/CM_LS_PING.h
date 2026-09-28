#pragma once

#include <cstdint>

#include "aion/gameserver/network/loginserver/LsClientPacket.h"
#include "aion/gameserver/network/loginserver/clientpackets/fwd.h"

namespace aion::gameserver::network::loginserver::clientpackets {

/**
 * @author KID
 */
class CM_LS_PING : public LsClientPacket {
public:
	explicit CM_LS_PING(int32_t opCode);

protected:
	void readImpl() override {}

	void runImpl() override;
};

} // namespace aion::gameserver::network::loginserver::clientpackets
