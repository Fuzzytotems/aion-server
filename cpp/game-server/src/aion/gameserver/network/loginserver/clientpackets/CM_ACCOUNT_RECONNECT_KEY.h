#pragma once

#include <cstdint>

#include "aion/gameserver/network/loginserver/LsClientPacket.h"
#include "aion/gameserver/network/loginserver/clientpackets/fwd.h"

namespace aion::gameserver::network::loginserver::clientpackets {

/**
 * In this packet LoginServer is sending response for SM_ACCOUNT_RECONNECT_KEY with account name and reconnectionKey.
 *
 * @author -Nemesiss-
 */
class CM_ACCOUNT_RECONNECT_KEY : public LsClientPacket {
public:
	explicit CM_ACCOUNT_RECONNECT_KEY(int32_t opCode);

private:
	/** accountId of account that will be reconnecting. */
	int32_t accountId = 0;
	/** ReconnectKey that will be used for authentication. */
	int32_t reconnectKey = 0;

public:
	void readImpl() override;

	void runImpl() override;
};

} // namespace aion::gameserver::network::loginserver::clientpackets
