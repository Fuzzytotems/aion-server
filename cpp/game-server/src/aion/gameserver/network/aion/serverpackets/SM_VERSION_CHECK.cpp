#include "aion/gameserver/network/aion/serverpackets/SM_VERSION_CHECK.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_VERSION_CHECK::SM_VERSION_CHECK(model::EventTheme cityDecorationValue)
	: SM_VERSION_CHECK(INTERNAL_VERSION, cityDecorationValue) {
}

SM_VERSION_CHECK::SM_VERSION_CHECK(int32_t versionValue, model::EventTheme cityDecorationValue)
	: AionServerPacket(opcodeOf<SM_VERSION_CHECK>), version(versionValue), cityDecoration(cityDecorationValue) {
}

void SM_VERSION_CHECK::writeImpl(AionConnection* con) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::network::aion::serverpackets
