#include "aion/gameserver/network/aion/serverpackets/SM_SIEGE_LOCATION_INFO.h"

#include "aion/gameserver/model/siege/SiegeLocation.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"
#include "aion/gameserver/runtime/base/Unported.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_SIEGE_LOCATION_INFO::SM_SIEGE_LOCATION_INFO()
	: AionServerPacket(opcodeOf<SM_SIEGE_LOCATION_INFO>), infoType(0) {
	AION_UNPORTED();
}

SM_SIEGE_LOCATION_INFO::SM_SIEGE_LOCATION_INFO(model::siege::SiegeLocation& loc)
	: AionServerPacket(opcodeOf<SM_SIEGE_LOCATION_INFO>), infoType(1) {
	AION_UNPORTED();
}

SM_SIEGE_LOCATION_INFO::~SM_SIEGE_LOCATION_INFO() = default;

void SM_SIEGE_LOCATION_INFO::writeImpl(AionConnection* con) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::network::aion::serverpackets
