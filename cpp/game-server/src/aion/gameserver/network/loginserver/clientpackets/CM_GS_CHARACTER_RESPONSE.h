#pragma once

#include <cstdint>

#include "aion/gameserver/network/loginserver/LsClientPacket.h"
#include "aion/gameserver/network/loginserver/clientpackets/fwd.h"

namespace aion::gameserver::network::loginserver::clientpackets {

/**
 * @author cura
 */
class CM_GS_CHARACTER_RESPONSE : public LsClientPacket {
public:
	explicit CM_GS_CHARACTER_RESPONSE(int32_t opCode);

private:
	int32_t accountId = 0;

public:
	void readImpl() override;

	void runImpl() override;
};

} // namespace aion::gameserver::network::loginserver::clientpackets
