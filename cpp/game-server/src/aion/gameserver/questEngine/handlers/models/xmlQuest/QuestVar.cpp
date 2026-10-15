#include "aion/gameserver/questEngine/handlers/models/xmlQuest/QuestVar.h"

#include "aion/gameserver/questEngine/handlers/models/xmlQuest/QuestNpc.h"
#include "aion/gameserver/questEngine/model/QuestEnv.h"
#include "aion/gameserver/questEngine/model/QuestState.h"
#include "aion/gameserver/questEngine/model/QuestVars.h"

namespace aion::gameserver::questEngine::handlers::models::xmlQuest {

// C++: Java's `for (QuestNpc questNpc : npc)` throws NullPointerException for a <var> without <npc> children (JAXB leaves the list null); the
// bound list is empty then, and the loop answers false. Every <var> of the data has an <npc> (docs/deviations/P5-06c.md).
bool QuestVar::operate(model::QuestEnv& env, runtime::Ptr<model::QuestState> qs) const {
	int32_t var = -1;
	if (qs)
		var = qs->getQuestVars()->getQuestVars();
	if (var != value)
		return false;
	for (const QuestNpc& questNpc : npc) {
		if (questNpc.operate(env, qs))
			return true;
	}
	return false;
}

} // namespace aion::gameserver::questEngine::handlers::models::xmlQuest
