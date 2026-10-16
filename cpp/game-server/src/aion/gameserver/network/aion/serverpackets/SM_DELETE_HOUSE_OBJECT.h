#pragma once

#include <cstdint>

#include "aion/gameserver/network/aion/AionServerPacket.h"
#include "aion/gameserver/network/aion/fwd.h"
#include "aion/gameserver/network/aion/serverpackets/fwd.h"

namespace aion::gameserver::network::aion::serverpackets {

/**
 * @author Rolandas
 */
class SM_DELETE_HOUSE_OBJECT : public AionServerPacket {
private:
	int32_t itemObjectId{};

public:
	explicit SM_DELETE_HOUSE_OBJECT(int32_t itemObjectId);

protected:
	void writeImpl(AionConnection* con) override;
};

} // namespace aion::gameserver::network::aion::serverpackets
