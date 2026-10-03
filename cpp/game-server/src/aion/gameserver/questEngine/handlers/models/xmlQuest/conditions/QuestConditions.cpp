#include "aion/gameserver/questEngine/handlers/models/xmlQuest/conditions/QuestConditions.h"

#include "aion/gameserver/questEngine/handlers/models/xmlQuest/conditions/QuestCondition.h"
#include "aion/gameserver/questEngine/model/QuestEnv.h"

namespace aion::gameserver::questEngine::handlers::models::xmlQuest::conditions {

// C++: Java's `for (QuestCondition cond : conditions)` throws NullPointerException for a <conditions> without children (JAXB leaves the list
// null); the bound list is empty then, and the set answers its start value (true for AND, false for OR). Every <conditions> of the data has
// one (docs/deviations/P5-06c.md).
bool QuestConditions::checkConditionOfSet(model::QuestEnv& env) const {
	bool inCondition = (operate == model::ConditionUnionType::AND);
	for (const std::unique_ptr<QuestCondition>& cond : conditions) {
		bool bCond = cond->doCheck(env);
		switch (operate) {
			case model::ConditionUnionType::AND:
				if (!bCond)
					return false;
				inCondition = inCondition && bCond;
				break;
			case model::ConditionUnionType::OR:
				if (bCond)
					return true;
				inCondition = inCondition || bCond;
				break;
		}
	}
	return inCondition;
}

} // namespace aion::gameserver::questEngine::handlers::models::xmlQuest::conditions
