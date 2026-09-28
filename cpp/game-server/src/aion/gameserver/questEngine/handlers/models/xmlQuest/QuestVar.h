#pragma once

#include "aion/gameserver/questEngine/handlers/models/xmlQuest/QuestVar.xml.h"

namespace aion::gameserver::questEngine::handlers::models::xmlQuest {

/** Java com.aionemu.gameserver.questEngine.handlers.models.xmlQuest.QuestVar. @author Mr. Poke */
class QuestVar : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/questEngine/handlers/models/xmlQuest/QuestVar.xml.inc"
public:
};

} // namespace aion::gameserver::questEngine::handlers::models::xmlQuest
