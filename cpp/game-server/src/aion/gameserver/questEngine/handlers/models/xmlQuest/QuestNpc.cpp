#include "aion/gameserver/questEngine/handlers/models/xmlQuest/QuestNpc.h"

#include "aion/gameserver/model/gameobjects/Npc.h"
#include "aion/gameserver/model/gameobjects/VisibleObject.h"
#include "aion/gameserver/questEngine/handlers/models/xmlQuest/QuestDialog.h"
#include "aion/gameserver/questEngine/model/QuestEnv.h"
#include "aion/gameserver/questEngine/model/QuestState.h"

namespace aion::gameserver::questEngine::handlers::models::xmlQuest {

// C++: Java's `for (QuestDialog questDialog : dialog)` throws NullPointerException for an <npc> without <dialog> children (JAXB leaves the list
// null); the bound list is empty then, and the loop answers false. Every <npc> of the data has a <dialog> (docs/deviations/P5-06c.md).
bool QuestNpc::operate(model::QuestEnv& env, runtime::Ptr<model::QuestState> qs) const {
	int32_t npcId = -1;
	if (runtime::Ptr<gameserver::model::gameobjects::Npc> npc = runtime::as<gameserver::model::gameobjects::Npc>(env.getVisibleObject()))
		npcId = npc->getNpcId();
	if (npcId != id)
		return false;
	for (const QuestDialog& questDialog : dialog) {
		if (questDialog.operate(env, qs))
			return true;
	}
	return false;
}

} // namespace aion::gameserver::questEngine::handlers::models::xmlQuest
