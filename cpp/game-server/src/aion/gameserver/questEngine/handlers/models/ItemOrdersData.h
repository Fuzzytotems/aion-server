#pragma once

#include "aion/gameserver/questEngine/handlers/models/ItemOrdersData.xml.h"

namespace aion::gameserver::questEngine::handlers::models {

/** Java com.aionemu.gameserver.questEngine.handlers.models.ItemOrdersData. @author Bobobear, Pad */
class ItemOrdersData : public ::aion::gameserver::questEngine::handlers::models::XMLQuest {
#include "aion/gameserver/questEngine/handlers/models/ItemOrdersData.xml.inc"
public:
};

} // namespace aion::gameserver::questEngine::handlers::models
