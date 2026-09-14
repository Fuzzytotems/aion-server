#include "aion/gameserver/network/aion/serverpackets/SM_SIEGE_LOCATION_STATE.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_SIEGE_LOCATION_STATE::SM_SIEGE_LOCATION_STATE(model::siege::SiegeLocation& location)
	: AionServerPacket(opcodeOf<SM_SIEGE_LOCATION_STATE>) {
	AION_UNPORTED();
}

SM_SIEGE_LOCATION_STATE::SM_SIEGE_LOCATION_STATE(int32_t locationIdValue, int32_t stateValue)
	: AionServerPacket(opcodeOf<SM_SIEGE_LOCATION_STATE>), locationId(locationIdValue), state(stateValue) {
}

void SM_SIEGE_LOCATION_STATE::writeImpl(AionConnection* con) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::network::aion::serverpackets
