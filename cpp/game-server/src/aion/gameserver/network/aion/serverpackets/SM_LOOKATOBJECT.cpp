#include "aion/gameserver/network/aion/serverpackets/SM_LOOKATOBJECT.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/model/gameobjects/VisibleObject.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_LOOKATOBJECT::SM_LOOKATOBJECT(model::gameobjects::VisibleObject& visibleObjectValue)
	: AionServerPacket(opcodeOf<SM_LOOKATOBJECT>), visibleObject(visibleObjectValue) {
	AION_UNPORTED();
}

SM_LOOKATOBJECT::~SM_LOOKATOBJECT() = default;

void SM_LOOKATOBJECT::writeImpl(AionConnection* con) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::network::aion::serverpackets
