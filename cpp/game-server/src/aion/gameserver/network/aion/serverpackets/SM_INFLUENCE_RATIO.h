#pragma once

#include "aion/gameserver/network/aion/AionServerPacket.h"
#include "aion/gameserver/network/aion/fwd.h"
#include "aion/gameserver/network/aion/serverpackets/fwd.h"

namespace aion::gameserver::network::aion::serverpackets {

/**
 * @author Nemiroff
 */
class SM_INFLUENCE_RATIO : public AionServerPacket {
public:
	/** Java: implicit default constructor */
	SM_INFLUENCE_RATIO();

protected:
	void writeImpl(AionConnection* con) override;
};

} // namespace aion::gameserver::network::aion::serverpackets
