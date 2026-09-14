#pragma once

#include <cstdint>

#include "aion/gameserver/network/aion/AionServerPacket.h"
#include "aion/gameserver/network/aion/fwd.h"
#include "aion/gameserver/network/aion/serverpackets/fwd.h"

namespace aion::gameserver::network::aion::serverpackets {

/**
 * I have no idea wtf is this
 *
 * @author -Nemesiss-
 */
class SM_TIME_CHECK : public AionServerPacket {
private:
	int32_t serverUpTime{};
	int32_t nanoTime{};
public:
	explicit SM_TIME_CHECK(int32_t nanoTime);
protected:
	void writeImpl(AionConnection* con) override;
};

} // namespace aion::gameserver::network::aion::serverpackets
