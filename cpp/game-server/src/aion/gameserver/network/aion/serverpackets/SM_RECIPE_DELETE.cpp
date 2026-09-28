#include "aion/gameserver/network/aion/serverpackets/SM_RECIPE_DELETE.h"

#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_RECIPE_DELETE::SM_RECIPE_DELETE(int32_t recipeIdValue)
	: AionServerPacket(opcodeOf<SM_RECIPE_DELETE>), recipeId(recipeIdValue) {
}

void SM_RECIPE_DELETE::writeImpl(AionConnection* con) {
	writeD(recipeId);
}

} // namespace aion::gameserver::network::aion::serverpackets
