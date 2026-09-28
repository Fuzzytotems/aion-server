#include "aion/gameserver/network/aion/clientpackets/CM_RECIPE_DELETE.h"

#include "aion/gameserver/handlers/HandlerRegistry.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/gameobjects/player/RecipeList.h"
#include "aion/gameserver/network/aion/AionConnection.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets {

CM_RECIPE_DELETE::CM_RECIPE_DELETE(int32_t opcode, const StateSet& validStates) : AionClientPacket(opcode, validStates) {
}

// Java CM_RECIPE_DELETE.java:21-23
void CM_RECIPE_DELETE::readImpl() {
	recipeId = readD();
}

// Java CM_RECIPE_DELETE.java:26-29
void CM_RECIPE_DELETE::runImpl() {
	// Java checks neither the player nor the recipe list: a null is its NullPointerException (Ptr's operator->)
	const runtime::Ptr<model::gameobjects::player::Player> player = getConnection()->getActivePlayer();
	player->getRecipeList()->deleteRecipe(*player, recipeId);
}

AION_CLIENT_PACKET(CM_RECIPE_DELETE);

} // namespace aion::gameserver::network::aion::clientpackets
