#include "aion/gameserver/questEngine/handlers/models/xmlQuest/events/OnTalkEvent.h"

#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/gameobjects/player/QuestStateList.h"
#include "aion/gameserver/questEngine/handlers/models/xmlQuest/QuestVar.h"
#include "aion/gameserver/questEngine/handlers/models/xmlQuest/conditions/QuestConditions.h"
#include "aion/gameserver/questEngine/model/QuestEnv.h"
#include "aion/gameserver/questEngine/model/QuestState.h"

namespace aion::gameserver::questEngine::handlers::models::xmlQuest::events {

// C++: Java's `for (QuestVar questVar : var)` throws NullPointerException for an <on_talk_event> without <var> children (JAXB leaves the list
// null); the bound list is empty then, and the loop answers false. The one <on_talk_event> of the data has two (docs/deviations/P5-06c.md).
bool OnTalkEvent::operate(model::QuestEnv& env) const {
	if (!conditions || conditions->checkConditionOfSet(env)) {
		runtime::Ptr<model::QuestState> qs = env.getPlayer()->getQuestStateList()->getQuestState(env.getQuestId());
		for (const QuestVar& questVar : var) {
			if (questVar.operate(env, qs))
				return true;
		}
	}
	return false;
}

} // namespace aion::gameserver::questEngine::handlers::models::xmlQuest::events
