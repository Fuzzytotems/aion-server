#include "aion/gameserver/network/aion/serverpackets/SM_SHOW_BRAND.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_SHOW_BRAND::SM_SHOW_BRAND(int32_t iconId, int32_t targetObjectId)
	: AionServerPacket(opcodeOf<SM_SHOW_BRAND>) {
	AION_UNPORTED();
}

SM_SHOW_BRAND::SM_SHOW_BRAND(const std::unordered_map<int32_t, int32_t>& targetIdsByIconIdValue)
	: AionServerPacket(opcodeOf<SM_SHOW_BRAND>) {
	AION_UNPORTED();
}

void SM_SHOW_BRAND::writeImpl(AionConnection* con) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::network::aion::serverpackets
