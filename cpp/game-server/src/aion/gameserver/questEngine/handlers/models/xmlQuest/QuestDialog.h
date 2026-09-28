#pragma once

#include "aion/gameserver/questEngine/handlers/models/xmlQuest/QuestDialog.xml.h"

#include "aion/gameserver/questEngine/model/fwd.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"

namespace aion::gameserver::questEngine::handlers::models::xmlQuest {

/** Java com.aionemu.gameserver.questEngine.handlers.models.xmlQuest.QuestDialog. @author Mr. Poke */
class QuestDialog : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/questEngine/handlers/models/xmlQuest/QuestDialog.xml.inc"
public:
	/**
	 * Java: operate(QuestEnv, QuestState) (QuestDialog.java:30-39; header request m5d-h03). qs is nullable: QuestNpc.operate hands on the
	 * quest state QuestVar.operate got, which is null for a quest the player does not hold (OnTalkEvent.java:27; QuestVar.java:29 compares it
	 * with null).
	 */
	bool operate(model::QuestEnv& env, runtime::Ptr<model::QuestState> qs) const;
};

} // namespace aion::gameserver::questEngine::handlers::models::xmlQuest
