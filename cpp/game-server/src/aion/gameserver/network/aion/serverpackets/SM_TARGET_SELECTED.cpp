#include "aion/gameserver/network/aion/serverpackets/SM_TARGET_SELECTED.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_TARGET_SELECTED::SM_TARGET_SELECTED(runtime::Ptr<model::gameobjects::VisibleObject> target)
	: AionServerPacket(opcodeOf<SM_TARGET_SELECTED>) {
	AION_UNPORTED();
}

void SM_TARGET_SELECTED::writeImpl(AionConnection* con) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::network::aion::serverpackets
