#pragma once

#include "aion/gameserver/questEngine/handlers/models/xmlQuest/QuestVar.xml.h"

#include "aion/gameserver/questEngine/model/fwd.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"

namespace aion::gameserver::questEngine::handlers::models::xmlQuest {

/** Java com.aionemu.gameserver.questEngine.handlers.models.xmlQuest.QuestVar. @author Mr. Poke */
class QuestVar : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/questEngine/handlers/models/xmlQuest/QuestVar.xml.inc"
public:
	/**
	 * Java: operate(QuestEnv, QuestState) (QuestVar.java:27-38; header request m5d-h03). qs is nullable (hub-headers.md §5.1 rule 2: the body
	 * compares it with null, QuestVar.java:29): OnTalkEvent.operate passes the player's quest state, null for a quest the player does not
	 * hold (OnTalkEvent.java:27).
	 */
	bool operate(model::QuestEnv& env, runtime::Ptr<model::QuestState> qs) const;
};

} // namespace aion::gameserver::questEngine::handlers::models::xmlQuest
