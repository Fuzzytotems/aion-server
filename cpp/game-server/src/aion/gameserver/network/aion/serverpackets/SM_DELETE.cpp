#include "aion/gameserver/network/aion/serverpackets/SM_DELETE.h"

#include "aion/gameserver/model/animations/ObjectDeleteAnimation.h"
#include "aion/gameserver/model/animations/ObjectDeleteAnimationInfo.h"
#include "aion/gameserver/model/gameobjects/VisibleObject.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_DELETE::SM_DELETE(model::gameobjects::VisibleObject& object) : SM_DELETE(object, model::animations::ObjectDeleteAnimation::FADE_OUT, true) {
}

SM_DELETE::SM_DELETE(model::gameobjects::VisibleObject& object, bool inRange)
	: SM_DELETE(object, model::animations::ObjectDeleteAnimation::FADE_OUT, inRange) {
}

SM_DELETE::SM_DELETE(model::gameobjects::VisibleObject& object, model::animations::ObjectDeleteAnimation animation)
	: SM_DELETE(object, animation, true) {
}

SM_DELETE::SM_DELETE(model::gameobjects::VisibleObject& object, model::animations::ObjectDeleteAnimation animation, bool inRange)
	: AionServerPacket(opcodeOf<SM_DELETE>), objectId(object.getObjectId()),
	  animationId(inRange ? model::animations::getId(animation) : model::animations::getId(model::animations::ObjectDeleteAnimation::NONE)) {
}

void SM_DELETE::writeImpl(AionConnection* con) {
	writeD(objectId);
	writeC(animationId);
}

} // namespace aion::gameserver::network::aion::serverpackets
