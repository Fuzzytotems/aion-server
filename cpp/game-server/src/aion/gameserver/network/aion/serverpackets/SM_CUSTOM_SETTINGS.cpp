#include "aion/gameserver/network/aion/serverpackets/SM_CUSTOM_SETTINGS.h"

#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/gameobjects/player/PlayerSettings.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_CUSTOM_SETTINGS::SM_CUSTOM_SETTINGS(model::gameobjects::player::Player& player)
	: SM_CUSTOM_SETTINGS(player.getObjectId(), 1, player.getPlayerSettings()->getDisplay(), player.getPlayerSettings()->getDeny()) {
}

SM_CUSTOM_SETTINGS::SM_CUSTOM_SETTINGS(int32_t objectIdValue, int32_t unkValue, int32_t displayValue, int32_t denyValue)
	: AionServerPacket(opcodeOf<SM_CUSTOM_SETTINGS>), objectId(objectIdValue), unk(unkValue), display(displayValue), deny(denyValue) {
}

void SM_CUSTOM_SETTINGS::writeImpl(AionConnection* con) {
	writeD(objectId);
	writeC(unk); // unk
	writeH(display);
	writeH(deny);
}

} // namespace aion::gameserver::network::aion::serverpackets
