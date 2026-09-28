#pragma once

#include "aion/gameserver/questEngine/handlers/models/xmlQuest/QuestDialog.xml.h"

namespace aion::gameserver::questEngine::handlers::models::xmlQuest {

/** Java com.aionemu.gameserver.questEngine.handlers.models.xmlQuest.QuestDialog. @author Mr. Poke */
class QuestDialog : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/questEngine/handlers/models/xmlQuest/QuestDialog.xml.inc"
public:
};

} // namespace aion::gameserver::questEngine::handlers::models::xmlQuest
