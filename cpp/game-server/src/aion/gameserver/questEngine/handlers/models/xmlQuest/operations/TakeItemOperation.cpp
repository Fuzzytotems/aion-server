#include "aion/gameserver/questEngine/handlers/models/xmlQuest/operations/TakeItemOperation.h"

#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/items/storage/Storage.h"
#include "aion/gameserver/questEngine/model/QuestEnv.h"

namespace aion::gameserver::questEngine::handlers::models::xmlQuest::operations {

void TakeItemOperation::doOperate(model::QuestEnv& env) const {
	env.getPlayer()->getInventory().decreaseByItemId(itemId, count);
}

} // namespace aion::gameserver::questEngine::handlers::models::xmlQuest::operations
