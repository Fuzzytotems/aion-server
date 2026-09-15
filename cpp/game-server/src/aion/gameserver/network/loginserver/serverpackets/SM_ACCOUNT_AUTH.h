#pragma once

#include <cstdint>

#include "aion/gameserver/network/loginserver/LsServerPacket.h"
#include "aion/gameserver/network/loginserver/serverpackets/fwd.h"

namespace aion::gameserver::network::loginserver::serverpackets {

/**
 * In this packet Gameserver is asking if given account sessionKey is valid at Loginserver side. [if user that is authenticating on Gameserver is
 * already authenticated on Loginserver]
 *
 * @author -Nemesiss-
 */
class SM_ACCOUNT_AUTH : public LsServerPacket {
private:
	/** accountId [part of session key] */
	const int32_t accountId;
	/** loginOk [part of session key] */
	const int32_t loginOk;
	/** playOk1 [part of session key] */
	const int32_t playOk1;
	/** playOk2 [part of session key] */
	const int32_t playOk2;

public:
	/** Constructs new instance of <tt>SM_ACCOUNT_AUTH </tt> packet. */
	SM_ACCOUNT_AUTH(int32_t accountId, int32_t loginOk, int32_t playOk1, int32_t playOk2);

protected:
	void writeImpl(LoginServerConnection* con, commons::utils::ByteBuffer& buf) override;
};

} // namespace aion::gameserver::network::loginserver::serverpackets
