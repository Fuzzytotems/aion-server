#pragma once

#include <cstdint>

#include "aion/gameserver/network/aion/AionServerPacket.h"
#include "aion/gameserver/network/aion/fwd.h"
#include "aion/gameserver/network/aion/serverpackets/fwd.h"

namespace aion::gameserver::network::aion::serverpackets {

/**
 * @author Simple
 */
class SM_LOGIN_QUEUE : public AionServerPacket {
private:
	int32_t waitingPosition{};
	int32_t waitingTime{};
	int32_t waitingCount{};
	SM_LOGIN_QUEUE();
protected:
	void writeImpl(AionConnection* con) override;
};

} // namespace aion::gameserver::network::aion::serverpackets
