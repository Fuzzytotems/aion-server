#include "aion/gameserver/network/aion/serverpackets/SM_POSITION.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/model/gameobjects/VisibleObject.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_POSITION::SM_POSITION(model::gameobjects::VisibleObject& objectValue)
	: AionServerPacket(opcodeOf<SM_POSITION>), object(objectValue) {
}

SM_POSITION::~SM_POSITION() = default;

void SM_POSITION::writeImpl(AionConnection* con) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::network::aion::serverpackets
