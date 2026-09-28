#include "aion/gameserver/network/aion/serverpackets/SM_HEADING_UPDATE.h"

#include "aion/gameserver/model/gameobjects/VisibleObject.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_HEADING_UPDATE::SM_HEADING_UPDATE(model::gameobjects::VisibleObject& target)
	: AionServerPacket(opcodeOf<SM_HEADING_UPDATE>), objectId(target.getObjectId()), heading(target.getHeading()) {
}

void SM_HEADING_UPDATE::writeImpl(AionConnection* con) {
	writeD(objectId);
	writeC(heading);
}

} // namespace aion::gameserver::network::aion::serverpackets
