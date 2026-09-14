#include "aion/gameserver/network/aion/serverpackets/SM_RECIPE_COOLDOWN.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_RECIPE_COOLDOWN::SM_RECIPE_COOLDOWN(model::gameobjects::player::Player& player, int32_t modeValue)
	: AionServerPacket(opcodeOf<SM_RECIPE_COOLDOWN>), mode(modeValue) {
	AION_UNPORTED();
}

void SM_RECIPE_COOLDOWN::writeImpl(AionConnection* con) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::network::aion::serverpackets
