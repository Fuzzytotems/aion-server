#pragma once

#include <cstdint>

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/network/aion/AionServerPacket.h"
#include "aion/gameserver/network/aion/fwd.h"
#include "aion/gameserver/network/aion/serverpackets/fwd.h"

namespace aion::gameserver::network::aion::serverpackets {

/**
 * @author Cheatkiller
 */
class SM_RIDE_ROBOT : public AionServerPacket {
private:
	int32_t robotId{};
	int32_t objectId{};
public:
	explicit SM_RIDE_ROBOT(model::gameobjects::player::Player& player);
	SM_RIDE_ROBOT(model::gameobjects::player::Player& player, int32_t robotId);
protected:
	void writeImpl(AionConnection* con) override;
};

} // namespace aion::gameserver::network::aion::serverpackets
