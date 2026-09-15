#include "aion/gameserver/network/aion/serverpackets/SM_INSTANCE_STAGE_INFO.h"

#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_INSTANCE_STAGE_INFO::SM_INSTANCE_STAGE_INFO(int32_t typeValue, int32_t eventValue, int32_t unkValue)
	: AionServerPacket(opcodeOf<SM_INSTANCE_STAGE_INFO>), type(typeValue), event(eventValue), unk(unkValue) {
}

void SM_INSTANCE_STAGE_INFO::writeImpl(AionConnection* con) {
	writeC(type);
	writeD(0);
	writeH(event);
	writeH(unk);
}

} // namespace aion::gameserver::network::aion::serverpackets
