#pragma once

#include <cstdint>

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/network/aion/AionServerPacket.h"
#include "aion/gameserver/network/aion/fwd.h"
#include "aion/gameserver/network/aion/serverpackets/fwd.h"
#include "aion/gameserver/world/zone/fwd.h"

namespace aion::gameserver::network::aion::serverpackets {

/**
 * @author Rolandas
 */
class SM_PLAYER_REGION : public AionServerPacket {
private:
	int32_t playerObjId{};
	const world::zone::ZoneName* subZone{};
public:
	SM_PLAYER_REGION(model::gameobjects::player::Player& player, const world::zone::ZoneName* subZone);
protected:
	void writeImpl(AionConnection* con) override;
};

} // namespace aion::gameserver::network::aion::serverpackets
