#pragma once

#include <cstdint>
#include <string>

#include "aion/gameserver/network/loginserver/LsClientPacket.h"
#include "aion/gameserver/network/loginserver/clientpackets/fwd.h"

namespace aion::gameserver::network::loginserver::clientpackets {

/**
 * @author Watson
 */
class CM_BAN_RESPONSE : public LsClientPacket {
public:
	explicit CM_BAN_RESPONSE(int32_t opCode);

private:
	int8_t type = 0;
	int32_t accountId = 0;
	std::string ip;
	int32_t time = 0;
	int32_t adminObjId = 0;
	bool result = false;

public:
	void readImpl() override;

	void runImpl() override;
};

} // namespace aion::gameserver::network::loginserver::clientpackets
