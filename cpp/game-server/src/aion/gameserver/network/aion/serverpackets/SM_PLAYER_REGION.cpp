#include "aion/gameserver/network/aion/serverpackets/SM_PLAYER_REGION.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"
#include "aion/gameserver/world/zone/ZoneName.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_PLAYER_REGION::SM_PLAYER_REGION(model::gameobjects::player::Player& player, const world::zone::ZoneName* subZoneValue)
	: AionServerPacket(opcodeOf<SM_PLAYER_REGION>), subZone(subZoneValue) {
	AION_UNPORTED();
}

void SM_PLAYER_REGION::writeImpl(AionConnection* con) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::network::aion::serverpackets
