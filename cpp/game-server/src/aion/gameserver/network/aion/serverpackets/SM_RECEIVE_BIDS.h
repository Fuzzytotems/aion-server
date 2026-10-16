#pragma once

#include <cstdint>

#include "aion/gameserver/network/aion/AionServerPacket.h"
#include "aion/gameserver/network/aion/fwd.h"
#include "aion/gameserver/network/aion/serverpackets/fwd.h"

namespace aion::gameserver::network::aion::serverpackets {

/**
 * Tells the client that auction related data has changed. The client will send CM_GET_HOUSE_BIDS if needed.
 *
 * @author Rolandas
 */
class SM_RECEIVE_BIDS : public AionServerPacket {
private:
	int32_t unk{};
public:
	explicit SM_RECEIVE_BIDS(int32_t unk);
protected:
	void writeImpl(AionConnection* con) override;
};

} // namespace aion::gameserver::network::aion::serverpackets
