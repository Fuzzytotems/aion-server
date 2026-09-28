#pragma once

#include <cstdint>
#include <string>

#include "aion/gameserver/model/account/fwd.h"
#include "aion/gameserver/network/loginserver/LsServerPacket.h"
#include "aion/gameserver/network/loginserver/serverpackets/fwd.h"

namespace aion::gameserver::network::loginserver::serverpackets {

/**
 * @author ViAl
 */
class SM_CHANGE_ALLOWED_HDD_SERIAL : public LsServerPacket {
private:
	int32_t accountId;
	std::string hddSerial;

public:
	explicit SM_CHANGE_ALLOWED_HDD_SERIAL(model::account::Account& playerAccount);

protected:
	void writeImpl(LoginServerConnection* con, commons::utils::ByteBuffer& buf) override;
};

} // namespace aion::gameserver::network::loginserver::serverpackets
