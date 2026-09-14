#include "aion/gameserver/network/aion/serverpackets/SM_HEADING_UPDATE.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_HEADING_UPDATE::SM_HEADING_UPDATE(model::gameobjects::VisibleObject& target) : AionServerPacket(opcodeOf<SM_HEADING_UPDATE>) {
	AION_UNPORTED();
}

void SM_HEADING_UPDATE::writeImpl(AionConnection* con) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::network::aion::serverpackets
