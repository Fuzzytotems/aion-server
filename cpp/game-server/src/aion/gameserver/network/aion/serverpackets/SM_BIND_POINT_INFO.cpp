#include "aion/gameserver/network/aion/serverpackets/SM_BIND_POINT_INFO.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_BIND_POINT_INFO::SM_BIND_POINT_INFO(int32_t mapIdValue, float xValue, float yValue, float zValue)
	: AionServerPacket(opcodeOf<SM_BIND_POINT_INFO>), mapId(mapIdValue), x(xValue), y(yValue), z(zValue), bindPointType(0), kiskObjId(0) {
}

SM_BIND_POINT_INFO::SM_BIND_POINT_INFO(runtime::Ptr<model::gameobjects::Kisk> kisk) : AionServerPacket(opcodeOf<SM_BIND_POINT_INFO>) {
	AION_UNPORTED();
}

void SM_BIND_POINT_INFO::writeImpl(AionConnection* con) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::network::aion::serverpackets
