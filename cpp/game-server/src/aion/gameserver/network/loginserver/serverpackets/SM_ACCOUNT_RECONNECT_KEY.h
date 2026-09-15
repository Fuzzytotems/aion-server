#pragma once

#include <cstdint>

#include "aion/gameserver/network/loginserver/LsServerPacket.h"
#include "aion/gameserver/network/loginserver/serverpackets/fwd.h"

namespace aion::gameserver::network::loginserver::serverpackets {

/**
 * This packet is sended by GameServer when player is requesting fast reconnect to login server. LoginServer in response will send reconectKey.
 *
 * @author -Nemesiss-
 */
class SM_ACCOUNT_RECONNECT_KEY : public LsServerPacket {
private:
	/** AccountId of client that is requested reconnection to LoginServer. */
	const int32_t accountId;

public:
	/** Constructs new instance of <tt>SM_ACCOUNT_RECONNECT_KEY </tt> packet. */
	explicit SM_ACCOUNT_RECONNECT_KEY(int32_t accountId);

protected:
	void writeImpl(LoginServerConnection* con, commons::utils::ByteBuffer& buf) override;
};

} // namespace aion::gameserver::network::loginserver::serverpackets
