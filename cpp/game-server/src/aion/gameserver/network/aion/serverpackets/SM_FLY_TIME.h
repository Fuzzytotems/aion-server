#pragma once

#include <cstdint>

#include "aion/gameserver/network/aion/AionServerPacket.h"
#include "aion/gameserver/network/aion/fwd.h"
#include "aion/gameserver/network/aion/serverpackets/fwd.h"

namespace aion::gameserver::network::aion::serverpackets {

/**
 * @author Nemiroff
 */
class SM_FLY_TIME : public AionServerPacket {
private:
	int32_t currentFp{};
	int32_t maxFp{};

public:
	SM_FLY_TIME(int32_t currentFp, int32_t maxFp);

protected:
	void writeImpl(AionConnection* con) override;
};

} // namespace aion::gameserver::network::aion::serverpackets
