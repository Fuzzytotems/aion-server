#include "aion/gameserver/network/aion/serverpackets/SM_PLAYER_REGION.h"

#include "aion/gameserver/dataholders/detail/JavaHashMapOrder.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"
#include "aion/gameserver/world/zone/ZoneName.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_PLAYER_REGION::SM_PLAYER_REGION(model::gameobjects::player::Player& player, const world::zone::ZoneName* subZoneValue)
	: AionServerPacket(opcodeOf<SM_PLAYER_REGION>), subZone(subZoneValue) {
	playerObjId = player.getObjectId();
	// subZone: player.getActiveRegion().getZones(player).stream().findFirst().get().getAreaTemplate().getZoneName()
}

void SM_PLAYER_REGION::writeImpl(AionConnection* con) {
	writeD(playerObjId);
	writeC(0);
	writeC(0);
	writeC(0);
	if (subZone == nullptr)
		throw runtime::NullPointerException("SM_PLAYER_REGION without a sub zone");
	writeD(dataholders::detail::javaHashCode(subZone->name())); // Java String.hashCode()
}

} // namespace aion::gameserver::network::aion::serverpackets
