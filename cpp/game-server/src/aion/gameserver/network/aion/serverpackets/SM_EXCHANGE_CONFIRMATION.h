#pragma once

#include <cstdint>

#include "aion/gameserver/network/aion/AionServerPacket.h"
#include "aion/gameserver/network/aion/fwd.h"
#include "aion/gameserver/network/aion/serverpackets/fwd.h"

namespace aion::gameserver::network::aion::serverpackets {

/**
 * @author -Avol-
 */
class SM_EXCHANGE_CONFIRMATION : public AionServerPacket {
private:
	int32_t action{};

public:
	explicit SM_EXCHANGE_CONFIRMATION(int32_t action);

protected:
	void writeImpl(AionConnection* con) override;
};

} // namespace aion::gameserver::network::aion::serverpackets
