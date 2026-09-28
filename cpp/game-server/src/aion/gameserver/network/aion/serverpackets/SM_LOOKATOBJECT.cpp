#include "aion/gameserver/network/aion/serverpackets/SM_LOOKATOBJECT.h"

#include "aion/gameserver/model/gameobjects/VisibleObject.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_LOOKATOBJECT::SM_LOOKATOBJECT(model::gameobjects::VisibleObject& visibleObjectValue)
	: AionServerPacket(opcodeOf<SM_LOOKATOBJECT>), visibleObject(visibleObjectValue) {
	runtime::Ptr<model::gameobjects::VisibleObject> target = visibleObjectValue.getTarget(); // Java reads getTarget() twice
	targetObjectId = target == nullptr ? 0 : target->getObjectId();
	heading = visibleObjectValue.getHeading();
}

SM_LOOKATOBJECT::~SM_LOOKATOBJECT() = default;

void SM_LOOKATOBJECT::writeImpl(AionConnection* con) {
	writeD(visibleObject->getObjectId());
	writeD(targetObjectId);
	writeC(heading);
}

} // namespace aion::gameserver::network::aion::serverpackets
