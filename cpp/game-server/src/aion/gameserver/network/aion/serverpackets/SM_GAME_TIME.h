#pragma once

#include "aion/gameserver/network/aion/AionServerPacket.h"
#include "aion/gameserver/network/aion/fwd.h"
#include "aion/gameserver/network/aion/serverpackets/fwd.h"

namespace aion::gameserver::network::aion::serverpackets {

/**
 * Sends the current time in the server in minutes since 1/1/00 00:00:00
 *
 * @author Ben
 */
class SM_GAME_TIME : public AionServerPacket {
public:
	/** Java: implicit default constructor */
	SM_GAME_TIME();

protected:
	void writeImpl(AionConnection* con) override;
};

} // namespace aion::gameserver::network::aion::serverpackets
