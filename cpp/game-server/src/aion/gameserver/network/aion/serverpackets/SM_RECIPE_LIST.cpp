#include "aion/gameserver/network/aion/serverpackets/SM_RECIPE_LIST.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_RECIPE_LIST::SM_RECIPE_LIST(const std::unordered_set<int32_t>& recipeIdsValue)
	: AionServerPacket(opcodeOf<SM_RECIPE_LIST>), recipeIds(recipeIdsValue) {
}

void SM_RECIPE_LIST::writeImpl(AionConnection* con) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::network::aion::serverpackets
