#include "aion/gameserver/network/aion/serverpackets/SM_LEARN_RECIPE.h"

#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_LEARN_RECIPE::SM_LEARN_RECIPE(int32_t recipeIdValue)
	: AionServerPacket(opcodeOf<SM_LEARN_RECIPE>), recipeId(recipeIdValue) {
}

void SM_LEARN_RECIPE::writeImpl(AionConnection* con) {
	writeD(recipeId);
	writeC(0); // 4.0
}

} // namespace aion::gameserver::network::aion::serverpackets
