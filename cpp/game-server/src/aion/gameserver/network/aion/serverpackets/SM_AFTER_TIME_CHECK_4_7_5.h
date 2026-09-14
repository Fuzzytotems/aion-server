#pragma once

#include "aion/gameserver/network/aion/AionServerPacket.h"
#include "aion/gameserver/network/aion/fwd.h"
#include "aion/gameserver/network/aion/serverpackets/fwd.h"

namespace aion::gameserver::network::aion::serverpackets {

/**
 * @author Ritsu
 */
class SM_AFTER_TIME_CHECK_4_7_5 : public AionServerPacket {
public:
	/** Java: implicit default constructor */
	SM_AFTER_TIME_CHECK_4_7_5();

protected:
	void writeImpl(AionConnection* con) override;
};

} // namespace aion::gameserver::network::aion::serverpackets
