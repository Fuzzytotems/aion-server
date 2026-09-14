#include "aion/gameserver/network/aion/serverpackets/SM_CHARACTER_SELECT.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_CHARACTER_SELECT::SM_CHARACTER_SELECT(int32_t typeValue) : AionServerPacket(opcodeOf<SM_CHARACTER_SELECT>), type(typeValue) {
}

SM_CHARACTER_SELECT::SM_CHARACTER_SELECT(int32_t typeValue, int16_t messageTypeValue, int32_t wrongCountValue)
	: AionServerPacket(opcodeOf<SM_CHARACTER_SELECT>), type(typeValue), messageType(messageTypeValue), wrongCount(wrongCountValue) {
}

void SM_CHARACTER_SELECT::writeImpl(AionConnection* con) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::network::aion::serverpackets
