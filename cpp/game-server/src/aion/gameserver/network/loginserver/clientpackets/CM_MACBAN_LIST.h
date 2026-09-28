#pragma once

#include <cstdint>

#include "aion/gameserver/network/loginserver/LsClientPacket.h"
#include "aion/gameserver/network/loginserver/clientpackets/fwd.h"

namespace aion::gameserver::network::loginserver::clientpackets {

/**
 * @author KID
 */
class CM_MACBAN_LIST : public LsClientPacket {
public:
	explicit CM_MACBAN_LIST(int32_t opCode);

protected:
	/** C++: runs on the IO strand like Java (the ban list is loaded while reading) */
	void readImpl() override;

	void runImpl() override {
		// ?
	}
};

} // namespace aion::gameserver::network::loginserver::clientpackets
