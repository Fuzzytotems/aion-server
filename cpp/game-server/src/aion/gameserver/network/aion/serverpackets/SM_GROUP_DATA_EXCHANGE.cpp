#include "aion/gameserver/network/aion/serverpackets/SM_GROUP_DATA_EXCHANGE.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_GROUP_DATA_EXCHANGE::SM_GROUP_DATA_EXCHANGE(std::span<const uint8_t> byteDataValue, int32_t actionValue, int32_t unk2Value)
	: AionServerPacket(opcodeOf<SM_GROUP_DATA_EXCHANGE>),
	  byteData(reinterpret_cast<const int8_t*>(byteDataValue.data()), reinterpret_cast<const int8_t*>(byteDataValue.data()) + byteDataValue.size()),
	  action(actionValue), unk2(unk2Value) {
}

SM_GROUP_DATA_EXCHANGE::SM_GROUP_DATA_EXCHANGE(std::span<const uint8_t> byteDataValue)
	: AionServerPacket(opcodeOf<SM_GROUP_DATA_EXCHANGE>),
	  byteData(reinterpret_cast<const int8_t*>(byteDataValue.data()), reinterpret_cast<const int8_t*>(byteDataValue.data()) + byteDataValue.size()),
	  action(1) {
}

void SM_GROUP_DATA_EXCHANGE::writeImpl(AionConnection* con) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::network::aion::serverpackets
