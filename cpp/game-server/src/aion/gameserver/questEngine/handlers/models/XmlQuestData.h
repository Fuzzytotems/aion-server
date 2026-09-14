#pragma once

#include "aion/gameserver/questEngine/handlers/models/XmlQuestData.xml.h"

namespace aion::gameserver::questEngine::handlers::models {

/** Java com.aionemu.gameserver.questEngine.handlers.models.XmlQuestData. @author Mr. Poke, Pad */
class XmlQuestData : public ::aion::gameserver::questEngine::handlers::models::XMLQuest {
#include "aion/gameserver/questEngine/handlers/models/XmlQuestData.xml.inc"
public:
};

} // namespace aion::gameserver::questEngine::handlers::models
