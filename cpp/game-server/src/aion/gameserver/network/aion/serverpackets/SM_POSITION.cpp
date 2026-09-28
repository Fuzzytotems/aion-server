#include "aion/gameserver/network/aion/serverpackets/SM_POSITION.h"

#include "aion/gameserver/model/gameobjects/VisibleObject.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_POSITION::SM_POSITION(model::gameobjects::VisibleObject& objectValue)
	: AionServerPacket(opcodeOf<SM_POSITION>), object(objectValue) {
}

SM_POSITION::~SM_POSITION() = default;

void SM_POSITION::writeImpl(AionConnection* con) {
	writeD(object->getObjectId());
	writeF(object->getX());
	writeF(object->getY());
	writeF(object->getZ());
	writeC(object->getHeading());
}

} // namespace aion::gameserver::network::aion::serverpackets
