#include "aion/gameserver/network/aion/serverpackets/SM_GATHERABLE_INFO.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"
#include "aion/gameserver/model/gameobjects/VisibleObject.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_GATHERABLE_INFO::SM_GATHERABLE_INFO(model::gameobjects::VisibleObject& visibleObjectValue)
	: AionServerPacket(opcodeOf<SM_GATHERABLE_INFO>), visibleObject(visibleObjectValue) {
}

SM_GATHERABLE_INFO::~SM_GATHERABLE_INFO() = default;

void SM_GATHERABLE_INFO::writeImpl(AionConnection* con) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::network::aion::serverpackets
