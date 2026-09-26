#pragma once

#include <cstdint>
#include <string>
#include <string_view>

#include "aion/gameserver/network/loginserver/LsServerPacket.h"
#include "aion/gameserver/network/loginserver/serverpackets/fwd.h"

namespace aion::gameserver::network::loginserver::serverpackets {

/**
 * @author KID
 */
class SM_MACBAN_CONTROL : public LsServerPacket {
private:
	int8_t type;
	std::string address;
	std::string details;
	int64_t time;

public:
	SM_MACBAN_CONTROL(int8_t type, std::string_view address, int64_t time, std::string_view details);

protected:
	void writeImpl(LoginServerConnection* con, commons::utils::ByteBuffer& buf) override;
};

} // namespace aion::gameserver::network::loginserver::serverpackets
