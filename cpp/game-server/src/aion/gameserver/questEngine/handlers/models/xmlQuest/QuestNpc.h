#pragma once

#include "aion/gameserver/questEngine/handlers/models/xmlQuest/QuestNpc.xml.h"

#include "aion/gameserver/questEngine/model/fwd.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"

namespace aion::gameserver::questEngine::handlers::models::xmlQuest {

/** Java com.aionemu.gameserver.questEngine.handlers.models.xmlQuest.QuestNpc. @author Mr. Poke */
class QuestNpc : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/questEngine/handlers/models/xmlQuest/QuestNpc.xml.inc"
public:
	/**
	 * Java: operate(QuestEnv, QuestState) (QuestNpc.java:28-39; header request m5d-h03). qs is nullable: QuestVar.operate hands on the
	 * player's quest state, null for a quest the player does not hold (OnTalkEvent.java:27; QuestVar.java:29 compares it with null).
	 */
	bool operate(model::QuestEnv& env, runtime::Ptr<model::QuestState> qs) const;
};

} // namespace aion::gameserver::questEngine::handlers::models::xmlQuest
