#include "aion/gameserver/network/aion/serverpackets/SM_LEGION_SEND_EMBLEM_DATA.h"

#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_LEGION_SEND_EMBLEM_DATA::SM_LEGION_SEND_EMBLEM_DATA(int32_t sizeValue, std::span<const uint8_t> dataValue)
	: AionServerPacket(opcodeOf<SM_LEGION_SEND_EMBLEM_DATA>), size(sizeValue), data(dataValue.begin(), dataValue.end()) {
}

void SM_LEGION_SEND_EMBLEM_DATA::writeImpl(AionConnection* con) {
	writeD(size);
	writeB(data);
}

} // namespace aion::gameserver::network::aion::serverpackets
