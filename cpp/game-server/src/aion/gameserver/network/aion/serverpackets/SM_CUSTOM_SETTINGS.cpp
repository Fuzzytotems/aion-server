#include "aion/gameserver/network/aion/serverpackets/SM_CUSTOM_SETTINGS.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_CUSTOM_SETTINGS::SM_CUSTOM_SETTINGS(model::gameobjects::player::Player& player) : AionServerPacket(opcodeOf<SM_CUSTOM_SETTINGS>) {
	AION_UNPORTED();
}

SM_CUSTOM_SETTINGS::SM_CUSTOM_SETTINGS(int32_t objectIdValue, int32_t unkValue, int32_t displayValue, int32_t denyValue)
	: AionServerPacket(opcodeOf<SM_CUSTOM_SETTINGS>), objectId(objectIdValue), unk(unkValue), display(displayValue), deny(denyValue) {
}

void SM_CUSTOM_SETTINGS::writeImpl(AionConnection* con) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::network::aion::serverpackets
