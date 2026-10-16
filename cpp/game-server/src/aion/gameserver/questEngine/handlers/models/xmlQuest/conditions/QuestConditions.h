#pragma once

#include "aion/gameserver/questEngine/handlers/models/xmlQuest/conditions/QuestConditions.xml.h"

#include "aion/gameserver/questEngine/model/fwd.h"

namespace aion::gameserver::questEngine::handlers::models::xmlQuest::conditions {

/** Java com.aionemu.gameserver.questEngine.handlers.models.xmlQuest.conditions.QuestConditions. @author Mr. Poke */
class QuestConditions : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/questEngine/handlers/models/xmlQuest/conditions/QuestConditions.xml.inc"
public:
	/** Java: checkConditionOfSet(QuestEnv) (QuestConditions.java:29-47; header request m5d-h03) */
	bool checkConditionOfSet(model::QuestEnv& env) const;
};

} // namespace aion::gameserver::questEngine::handlers::models::xmlQuest::conditions
