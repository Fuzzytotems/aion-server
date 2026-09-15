#pragma once

#include <cstdint>

#include "aion/gameserver/network/loginserver/LsClientPacket.h"
#include "aion/gameserver/network/loginserver/clientpackets/fwd.h"

namespace aion::gameserver::network::loginserver::clientpackets {

/**
 * @author ViAl
 */
class CM_HDD_BANLIST : public LsClientPacket {
private:
	int32_t count = 0;

public:
	explicit CM_HDD_BANLIST(int32_t opCode);

protected:
	/** C++: runs on the IO strand like Java (the ban list is loaded while reading) */
	void readImpl() override;

	void runImpl() override;
};

} // namespace aion::gameserver::network::loginserver::clientpackets
