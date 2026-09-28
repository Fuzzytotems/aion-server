#include "aion/gameserver/network/aion/serverpackets/SM_ICON_INFO.h"

#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_ICON_INFO::SM_ICON_INFO(int32_t buffIdValue, bool displayValue)
	: AionServerPacket(opcodeOf<SM_ICON_INFO>), buffId(buffIdValue), display(displayValue) {
}

void SM_ICON_INFO::writeImpl(AionConnection* con) {
	writeD(0); // unk
	writeD(buffId);
	writeC(display ? 1 : 0);
}

} // namespace aion::gameserver::network::aion::serverpackets
