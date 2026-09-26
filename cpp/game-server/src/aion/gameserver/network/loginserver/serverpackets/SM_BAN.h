#pragma once

#include <cstdint>
#include <string>
#include <string_view>

#include "aion/gameserver/network/loginserver/LsServerPacket.h"
#include "aion/gameserver/network/loginserver/serverpackets/fwd.h"

namespace aion::gameserver::network::loginserver::serverpackets {

/**
 * The universal packet for account/IP bans
 *
 * @author Watson
 */
class SM_BAN : public LsServerPacket {
private:
	/** Ban type 1 = account 2 = IP 3 = Full ban (account and IP) */
	const int8_t type;
	/** Account to ban */
	const int32_t accountId;
	/** IP or mask to ban */
	const std::string ip;
	/** Time in minutes. 0 = infinity; If time < 0 then it is an unban command */
	const int32_t time;
	/** Object ID of Admin, who request the ban */
	const int32_t adminObjId;

public:
	SM_BAN(int8_t type, int32_t accountId, std::string_view ip, int32_t time, int32_t adminObjId);

protected:
	void writeImpl(LoginServerConnection* con, commons::utils::ByteBuffer& buf) override;
};

} // namespace aion::gameserver::network::loginserver::serverpackets
