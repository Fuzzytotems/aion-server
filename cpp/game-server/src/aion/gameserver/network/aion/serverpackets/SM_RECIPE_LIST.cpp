#include "aion/gameserver/network/aion/serverpackets/SM_RECIPE_LIST.h"

#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"
#include "aion/gameserver/network/aion/serverpackets/detail/PacketSupport.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_RECIPE_LIST::SM_RECIPE_LIST(const std::unordered_set<int32_t>& recipeIdsValue)
	: AionServerPacket(opcodeOf<SM_RECIPE_LIST>), recipeIds(recipeIdsValue) {
}

void SM_RECIPE_LIST::writeImpl(AionConnection* con) {
	writeH(static_cast<int32_t>(recipeIds.size()));
	for (int32_t id : detail::javaHashSetOrder(recipeIds)) { // Java iterates the HashSet of RecipeList
		writeD(id);
		writeC(0);
	}
}

} // namespace aion::gameserver::network::aion::serverpackets
