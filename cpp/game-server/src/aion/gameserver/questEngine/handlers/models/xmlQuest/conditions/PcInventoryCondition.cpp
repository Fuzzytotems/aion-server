#include "aion/gameserver/questEngine/handlers/models/xmlQuest/conditions/PcInventoryCondition.h"

#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/items/storage/Storage.h"
#include "aion/gameserver/questEngine/model/QuestEnv.h"

namespace aion::gameserver::questEngine::handlers::models::xmlQuest::conditions {

bool PcInventoryCondition::doCheck(model::QuestEnv& env) const {
	runtime::Ptr<gameserver::model::gameobjects::player::Player> player = env.getPlayer();
	int64_t itemCount = player->getInventory().getItemCountByItemId(itemId);
	switch (getOp()) {
		case model::ConditionOperation::EQUAL:
			return itemCount == count;
		case model::ConditionOperation::GREATER:
			return itemCount > count;
		case model::ConditionOperation::GREATER_EQUAL:
			return itemCount >= count;
		case model::ConditionOperation::LESSER:
			return itemCount < count;
		case model::ConditionOperation::LESSER_EQUAL:
			return itemCount <= count;
		case model::ConditionOperation::NOT_EQUAL:
			return itemCount != count;
		default:
			return false;
	}
}

} // namespace aion::gameserver::questEngine::handlers::models::xmlQuest::conditions
