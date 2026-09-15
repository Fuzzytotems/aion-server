#include "aion/gameserver/network/aion/serverpackets/SM_BIND_POINT_INFO.h"

#include "aion/gameserver/model/gameobjects/Kisk.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"
#include "aion/gameserver/world/WorldPosition.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_BIND_POINT_INFO::SM_BIND_POINT_INFO(int32_t mapIdValue, float xValue, float yValue, float zValue)
	: AionServerPacket(opcodeOf<SM_BIND_POINT_INFO>), mapId(mapIdValue), x(xValue), y(yValue), z(zValue), bindPointType(0), kiskObjId(0) {
}

SM_BIND_POINT_INFO::SM_BIND_POINT_INFO(runtime::Ptr<model::gameobjects::Kisk> kisk) : AionServerPacket(opcodeOf<SM_BIND_POINT_INFO>) {
	if (!kisk || !kisk->isActive()) {
		this->mapId = 0;
		this->x = 0;
		this->y = 0;
		this->z = 0;
		this->kiskObjId = 0;
	} else {
		runtime::Ptr<world::WorldPosition> pos = kisk->getPosition();
		this->mapId = pos->getMapId();
		this->x = pos->getX();
		this->y = pos->getY();
		this->z = pos->getZ();
		this->kiskObjId = kisk->getObjectId();
	}
	this->bindPointType = 4;
}

void SM_BIND_POINT_INFO::writeImpl(AionConnection* con) {
	writeC(bindPointType); // 0 obelisk, 4 Kisk
	writeC(0x01); // unk
	writeD(mapId); // map id
	writeF(x); // x
	writeF(y); // y
	writeF(z); // z
	writeD(kiskObjId); // if 0 and in type = 4 will clear the current display
}

} // namespace aion::gameserver::network::aion::serverpackets
