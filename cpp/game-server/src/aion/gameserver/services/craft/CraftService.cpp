#include "aion/gameserver/services/craft/CraftService.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/commons/logging/LoggerFactory.h"

namespace aion::gameserver::services::craft {

static const auto log = commons::logging::LoggerFactory::getLogger("CRAFT_LOG");

// anonymous ItemUpdatePredicate at CraftService.java:73 (fieldmap key CraftService$1); argument 5 of addItem(); storage: sync
void CraftService::finishCrafting(model::gameobjects::player::Player& player, const model::templates::recipe::RecipeTemplate* recipetemplate, int32_t critCount, int32_t bonus) {
	AION_UNPORTED();
}

void CraftService::startCrafting(model::gameobjects::player::Player& player, int32_t recipeId, int32_t targetObjId, int32_t craftType, const std::unordered_map<int32_t, int64_t>& sendMaterialsData) {
	AION_UNPORTED();
}

bool CraftService::checkCraft(model::gameobjects::player::Player& player, const model::templates::recipe::RecipeTemplate* recipeTemplate, int32_t skillId, runtime::Ptr<model::gameobjects::VisibleObject> target, const model::templates::item::ItemTemplate* itemTemplate, int32_t craftType, const std::unordered_map<int32_t, int64_t>& sendMaterialsData) {
	AION_UNPORTED();
}

void CraftService::sendCancelCraft(model::gameobjects::player::Player& player, int32_t skillId, int32_t targetObjId, const model::templates::item::ItemTemplate* itemTemplate) {
	AION_UNPORTED();
}

int32_t CraftService::getBonusReqItem(int32_t skillId) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::services::craft
