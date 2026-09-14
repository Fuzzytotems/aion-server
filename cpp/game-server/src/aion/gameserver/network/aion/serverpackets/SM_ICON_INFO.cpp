#include "aion/gameserver/network/aion/serverpackets/SM_ICON_INFO.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_ICON_INFO::SM_ICON_INFO(int32_t buffIdValue, bool displayValue)
	: AionServerPacket(opcodeOf<SM_ICON_INFO>), buffId(buffIdValue), display(displayValue) {
}

void SM_ICON_INFO::writeImpl(AionConnection* con) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::network::aion::serverpackets
