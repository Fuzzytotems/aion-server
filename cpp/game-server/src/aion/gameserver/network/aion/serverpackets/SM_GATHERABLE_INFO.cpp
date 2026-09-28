#include "aion/gameserver/network/aion/serverpackets/SM_GATHERABLE_INFO.h"

#include "aion/gameserver/model/gameobjects/StaticDoor.h"
#include "aion/gameserver/model/gameobjects/VisibleObject.h"
#include "aion/gameserver/model/templates/VisibleObjectTemplate.h"
#include "aion/gameserver/model/templates/spawns/SpawnTemplate.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_GATHERABLE_INFO::SM_GATHERABLE_INFO(model::gameobjects::VisibleObject& visibleObjectValue)
	: AionServerPacket(opcodeOf<SM_GATHERABLE_INFO>), visibleObject(visibleObjectValue) {
}

SM_GATHERABLE_INFO::~SM_GATHERABLE_INFO() = default;

void SM_GATHERABLE_INFO::writeImpl(AionConnection* con) {
	writeF(visibleObject->getX());
	writeF(visibleObject->getY());
	writeF(visibleObject->getZ());
	writeD(visibleObject->getObjectId());
	writeD(visibleObject->getSpawn()->getStaticId());
	writeD(visibleObject->getObjectTemplate()->getTemplateId());
	if (runtime::Ptr<model::gameobjects::StaticDoor> door = runtime::as<model::gameobjects::StaticDoor>(visibleObject)) {
		if (door->isOpen()) {
			writeH(0x09);
		} else {
			writeH(0x0A);
		}
	} else {
		writeH(1);
	}
	writeC(visibleObject->getSpawn()->getHeading());
	writeD(visibleObject->getObjectTemplate()->getL10nId());
	writeH(0);
	writeH(0);
	writeH(0);
	writeC(100); // unk
}

} // namespace aion::gameserver::network::aion::serverpackets
