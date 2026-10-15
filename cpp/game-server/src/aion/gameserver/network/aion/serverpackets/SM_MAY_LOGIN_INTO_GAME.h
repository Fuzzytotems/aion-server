#pragma once

#include "aion/gameserver/network/aion/AionServerPacket.h"
#include "aion/gameserver/network/aion/fwd.h"
#include "aion/gameserver/network/aion/serverpackets/fwd.h"

namespace aion::gameserver::network::aion::serverpackets {

/**
 * This packet is response for CM_MAY_LOGIN_INTO_GAME
 *
 * @author -Nemesiss-
 */
class SM_MAY_LOGIN_INTO_GAME : public AionServerPacket {
public:
	SM_MAY_LOGIN_INTO_GAME();
protected:
	void writeImpl(AionConnection* con) override;
};

} // namespace aion::gameserver::network::aion::serverpackets
