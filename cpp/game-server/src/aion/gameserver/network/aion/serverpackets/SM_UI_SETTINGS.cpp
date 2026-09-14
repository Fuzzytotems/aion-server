#include "aion/gameserver/network/aion/serverpackets/SM_UI_SETTINGS.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_UI_SETTINGS::SM_UI_SETTINGS(std::span<const uint8_t> dataValue, int32_t typeValue)
	: AionServerPacket(opcodeOf<SM_UI_SETTINGS>), data(dataValue.begin(), dataValue.end()), type(typeValue) {
}

void SM_UI_SETTINGS::writeImpl(AionConnection* con) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::network::aion::serverpackets
