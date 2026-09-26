#include "aion/gameserver/network/aion/serverpackets/SM_RECALLED_BY_OTHER.h"

#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_RECALLED_BY_OTHER::SM_RECALLED_BY_OTHER()
	: SM_RECALLED_BY_OTHER(std::nullopt, 0, 0) {
}

SM_RECALLED_BY_OTHER::SM_RECALLED_BY_OTHER(std::optional<std::string_view> casterNameValue, int32_t skillIdValue, int32_t secondsValue)
	: AionServerPacket(opcodeOf<SM_RECALLED_BY_OTHER>), casterName(casterNameValue), skillId(skillIdValue), seconds(secondsValue) {
}

void SM_RECALLED_BY_OTHER::writeImpl(AionConnection* con) {
	writeC(casterName ? 0 : 1); // 0 = open the window, 1 = close it
	writeS(casterName.value_or(std::string())); // Java null: writeS writes only the terminator
	writeH(skillId);
	writeH(seconds);
}

} // namespace aion::gameserver::network::aion::serverpackets
