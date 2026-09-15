#pragma once

#include <cstdint>

#include "aion/gameserver/network/loginserver/LsClientPacket.h"
#include "aion/gameserver/network/loginserver/clientpackets/fwd.h"

namespace aion::gameserver::network::loginserver::clientpackets {

/**
 * This packet is request kicking player.
 *
 * @author -Nemesiss-, Neon
 */
class CM_REQUEST_KICK_ACCOUNT : public LsClientPacket {
public:
	explicit CM_REQUEST_KICK_ACCOUNT(int32_t opCode);

private:
	/** account id of account that login server request to kick. */
	int32_t accountId = 0;
	bool notifyDoubleLogin = false;

public:
	void readImpl() override;

	void runImpl() override;
};

} // namespace aion::gameserver::network::loginserver::clientpackets
