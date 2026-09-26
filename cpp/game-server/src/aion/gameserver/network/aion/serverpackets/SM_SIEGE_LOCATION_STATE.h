#pragma once

#include <cstdint>

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/model/siege/fwd.h"
#include "aion/gameserver/network/aion/AionServerPacket.h"
#include "aion/gameserver/network/aion/fwd.h"
#include "aion/gameserver/network/aion/serverpackets/fwd.h"

namespace aion::gameserver::network::aion::serverpackets {

/**
 * @author Source
 */
class SM_SIEGE_LOCATION_STATE : public AionServerPacket {
private:
	int32_t locationId{};
	int32_t state{};
public:
	explicit SM_SIEGE_LOCATION_STATE(model::siege::SiegeLocation& location);
	SM_SIEGE_LOCATION_STATE(int32_t locationId, int32_t state);
protected:
	void writeImpl(AionConnection* con) override;
};

} // namespace aion::gameserver::network::aion::serverpackets
