#include "aion/gameserver/network/aion/serverpackets/SM_DELETE.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"
#include "aion/gameserver/model/animations/ObjectDeleteAnimation.h"

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
	: AionServerPacket(opcodeOf<SM_DELETE>) {
	AION_UNPORTED();
}

void SM_DELETE::writeImpl(AionConnection* con) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::network::aion::serverpackets
