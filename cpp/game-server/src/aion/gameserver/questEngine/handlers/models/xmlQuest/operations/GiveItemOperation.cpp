#include "aion/gameserver/questEngine/handlers/models/xmlQuest/operations/GiveItemOperation.h"

#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/questEngine/model/QuestEnv.h"
#include "aion/gameserver/services/item/ItemService.h"

namespace aion::gameserver::questEngine::handlers::models::xmlQuest::operations {

void GiveItemOperation::doOperate(model::QuestEnv& env) const {
	services::item::ItemService::addItem(*env.getPlayer(), itemId, count, true);
}

} // namespace aion::gameserver::questEngine::handlers::models::xmlQuest::operations
