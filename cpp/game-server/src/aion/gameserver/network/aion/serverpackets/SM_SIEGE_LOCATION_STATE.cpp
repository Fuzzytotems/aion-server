#include "aion/gameserver/network/aion/serverpackets/SM_SIEGE_LOCATION_STATE.h"

#include "aion/gameserver/model/siege/SiegeLocation.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_SIEGE_LOCATION_STATE::SM_SIEGE_LOCATION_STATE(model::siege::SiegeLocation& location)
	: AionServerPacket(opcodeOf<SM_SIEGE_LOCATION_STATE>) {
	locationId = location.getLocationId();
	state = location.isVulnerable() ? 1 : 0;
}

SM_SIEGE_LOCATION_STATE::SM_SIEGE_LOCATION_STATE(int32_t locationIdValue, int32_t stateValue)
	: AionServerPacket(opcodeOf<SM_SIEGE_LOCATION_STATE>), locationId(locationIdValue), state(stateValue) {
}

void SM_SIEGE_LOCATION_STATE::writeImpl(AionConnection* con) {
	writeD(locationId);
	writeC(state);
}

} // namespace aion::gameserver::network::aion::serverpackets
