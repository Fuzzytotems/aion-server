#pragma once

#include <cstdint>
#include <unordered_map>

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/model/templates/item/fwd.h"
#include "aion/gameserver/model/templates/recipe/fwd.h"
#include "aion/gameserver/services/craft/fwd.h"

namespace aion::gameserver::services::craft {

/**
 * C++: a static-only class (hub-headers.md §11.1).
 *
 * @author MrPoke, sphinx, synchro2, Evil_dnk
 */
class CraftService {
public:
	static void finishCrafting(model::gameobjects::player::Player& player, const model::templates::recipe::RecipeTemplate* recipetemplate, int32_t critCount, int32_t bonus);
	static void startCrafting(model::gameobjects::player::Player& player, int32_t recipeId, int32_t targetObjId, int32_t craftType, const std::unordered_map<int32_t, int64_t>& sendMaterialsData);
private:
	static bool checkCraft(model::gameobjects::player::Player& player, const model::templates::recipe::RecipeTemplate* recipeTemplate, int32_t skillId, runtime::Ptr<model::gameobjects::VisibleObject> target, const model::templates::item::ItemTemplate* itemTemplate, int32_t craftType, const std::unordered_map<int32_t, int64_t>& sendMaterialsData);
	static void sendCancelCraft(model::gameobjects::player::Player& player, int32_t skillId, int32_t targetObjId, const model::templates::item::ItemTemplate* itemTemplate);
	static int32_t getBonusReqItem(int32_t skillId);
};

} // namespace aion::gameserver::services::craft
