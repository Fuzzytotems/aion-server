#pragma once

#include "aion/gameserver/questEngine/handlers/models/ItemCollectingData.xml.h"

namespace aion::gameserver::questEngine::handlers::models {

/** Java com.aionemu.gameserver.questEngine.handlers.models.ItemCollectingData. @author MrPoke, Rolandas, Majka, Pad */
class ItemCollectingData : public ::aion::gameserver::questEngine::handlers::models::XMLQuest {
#include "aion/gameserver/questEngine/handlers/models/ItemCollectingData.xml.inc"
public:
};

} // namespace aion::gameserver::questEngine::handlers::models
