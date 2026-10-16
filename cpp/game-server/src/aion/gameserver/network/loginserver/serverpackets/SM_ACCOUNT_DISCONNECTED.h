#pragma once

#include <cstdint>

#include "aion/gameserver/network/loginserver/LsServerPacket.h"
#include "aion/gameserver/network/loginserver/serverpackets/fwd.h"

namespace aion::gameserver::network::loginserver::serverpackets {

/**
 * In this packet GameServer is informing LoginServer that some account is no longer on GameServer [ie was disconencted]
 *
 * @author -Nemesiss-
 */
class SM_ACCOUNT_DISCONNECTED : public LsServerPacket {
private:
	/** AccountId of account that is no longer on GameServer. */
	const int32_t accountId;

public:
	/** Constructs new instance of <tt>SM_ACCOUNT_DISCONNECTED </tt> packet. */
	explicit SM_ACCOUNT_DISCONNECTED(int32_t accountId);

protected:
	void writeImpl(LoginServerConnection* con, commons::utils::ByteBuffer& buf) override;
};

} // namespace aion::gameserver::network::loginserver::serverpackets
