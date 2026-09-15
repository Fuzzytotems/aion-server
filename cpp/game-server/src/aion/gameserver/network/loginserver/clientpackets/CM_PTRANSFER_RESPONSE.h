#pragma once

#include <cstdint>

#include "aion/gameserver/network/loginserver/LsClientPacket.h"
#include "aion/gameserver/network/loginserver/clientpackets/fwd.h"

namespace aion::gameserver::network::loginserver::clientpackets {

/**
 * @author KID
 */
class CM_PTRANSFER_RESPONSE : public LsClientPacket {
public:
	explicit CM_PTRANSFER_RESPONSE(int32_t opCode);

protected:
	/** C++: runs on the IO strand like Java (the transfer is processed while reading) */
	void readImpl() override;

	void runImpl() override {}
};

} // namespace aion::gameserver::network::loginserver::clientpackets
