#include "aion/gameserver/network/aion/serverpackets/SM_UI_SETTINGS.h"

#include <vector>

#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_UI_SETTINGS::SM_UI_SETTINGS(std::span<const uint8_t> dataValue, int32_t typeValue)
	: AionServerPacket(opcodeOf<SM_UI_SETTINGS>), data(dataValue.begin(), dataValue.end()), type(typeValue) {
}

void SM_UI_SETTINGS::writeImpl(AionConnection* con) {
	writeC(type);
	writeH(0x1C00);
	writeB(data);
	if (0x1C00 > data.size())
		writeB(std::vector<uint8_t>(0x1C00 - data.size()));
}

} // namespace aion::gameserver::network::aion::serverpackets
