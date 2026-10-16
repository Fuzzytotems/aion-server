#include "aion/gameserver/questEngine/handlers/models/xmlQuest/conditions/NpcIdCondition.h"

#include "aion/gameserver/model/gameobjects/Npc.h"
#include "aion/gameserver/model/gameobjects/VisibleObject.h"
#include "aion/gameserver/questEngine/model/QuestEnv.h"

namespace aion::gameserver::questEngine::handlers::models::xmlQuest::conditions {

bool NpcIdCondition::doCheck(model::QuestEnv& env) const {
	int32_t id = 0;
	runtime::Ptr<gameserver::model::gameobjects::VisibleObject> visibleObject = env.getVisibleObject();
	if (runtime::Ptr<gameserver::model::gameobjects::Npc> npc = runtime::as<gameserver::model::gameobjects::Npc>(visibleObject)) {
		id = npc->getNpcId();
	}
	switch (getOp()) {
		case model::ConditionOperation::EQUAL:
			return id == values;
		case model::ConditionOperation::GREATER:
			return id > values;
		case model::ConditionOperation::GREATER_EQUAL:
			return id >= values;
		case model::ConditionOperation::LESSER:
			return id < values;
		case model::ConditionOperation::LESSER_EQUAL:
			return id <= values;
		case model::ConditionOperation::NOT_EQUAL:
			return id != values;
		default:
			return false;
	}
}

} // namespace aion::gameserver::questEngine::handlers::models::xmlQuest::conditions
