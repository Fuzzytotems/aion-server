#include "aion/gameserver/network/aion/serverpackets/SM_INSTANCE_COUNT_INFO.h"

#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_INSTANCE_COUNT_INFO::SM_INSTANCE_COUNT_INFO(int32_t mapIdValue, int32_t instanceIdValue)
	: AionServerPacket(opcodeOf<SM_INSTANCE_COUNT_INFO>), mapId(mapIdValue), instanceId(instanceIdValue) {
}

void SM_INSTANCE_COUNT_INFO::writeImpl(AionConnection* con) {
	writeD(mapId);
	writeD(instanceId);
	writeD(1); // 1 solo 31 group 61 alliance unk for league
}

} // namespace aion::gameserver::network::aion::serverpackets
