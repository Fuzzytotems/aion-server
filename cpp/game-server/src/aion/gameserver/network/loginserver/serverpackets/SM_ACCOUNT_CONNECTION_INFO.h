#pragma once

#include <cstdint>
#include <string>
#include <string_view>

#include "aion/gameserver/network/loginserver/LsServerPacket.h"
#include "aion/gameserver/network/loginserver/serverpackets/fwd.h"

namespace aion::gameserver::network::loginserver::serverpackets {

/**
 * @author ViAl, Neon
 */
class SM_ACCOUNT_CONNECTION_INFO : public LsServerPacket {
private:
	const int32_t accountId;
	const int64_t time;
	const std::string ip, mac, hddSerial;

public:
	SM_ACCOUNT_CONNECTION_INFO(int32_t accountId, int64_t time, std::string_view ip, std::string_view mac, std::string_view hddSerial);

protected:
	void writeImpl(LoginServerConnection* con, commons::utils::ByteBuffer& buf) override;
};

} // namespace aion::gameserver::network::loginserver::serverpackets
