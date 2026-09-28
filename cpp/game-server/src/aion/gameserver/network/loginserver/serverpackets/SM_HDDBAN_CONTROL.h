#pragma once

#include <cstdint>
#include <string>
#include <string_view>

#include "aion/gameserver/network/loginserver/LsServerPacket.h"
#include "aion/gameserver/network/loginserver/serverpackets/fwd.h"
#include "aion/gameserver/services/ban/BanAction.h"

namespace aion::gameserver::network::loginserver::serverpackets {

/**
 * @author ViAl
 */
class SM_HDDBAN_CONTROL : public LsServerPacket {
private:
	services::ban::BanAction action;
	std::string serial;
	int64_t time;

public:
	SM_HDDBAN_CONTROL(services::ban::BanAction action, std::string_view address, int64_t time);

protected:
	void writeImpl(LoginServerConnection* con, commons::utils::ByteBuffer& buf) override;
};

} // namespace aion::gameserver::network::loginserver::serverpackets
