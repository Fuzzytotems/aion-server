#pragma once

#include "aion/gameserver/questEngine/handlers/models/xmlQuest/QuestNpc.xml.h"

namespace aion::gameserver::questEngine::handlers::models::xmlQuest {

/** Java com.aionemu.gameserver.questEngine.handlers.models.xmlQuest.QuestNpc. @author Mr. Poke */
class QuestNpc : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/questEngine/handlers/models/xmlQuest/QuestNpc.xml.inc"
public:
};

} // namespace aion::gameserver::questEngine::handlers::models::xmlQuest
