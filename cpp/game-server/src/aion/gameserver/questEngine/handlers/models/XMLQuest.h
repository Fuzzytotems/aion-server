#pragma once

#include "aion/gameserver/questEngine/handlers/models/XMLQuest.xml.h"

namespace aion::gameserver::questEngine::handlers::models {

/** Java com.aionemu.gameserver.questEngine.handlers.models.XMLQuest. @author MrPoke, Hilgert, Pad */
class XMLQuest : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/questEngine/handlers/models/XMLQuest.xml.inc"
public:
};

} // namespace aion::gameserver::questEngine::handlers::models
