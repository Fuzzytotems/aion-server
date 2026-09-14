#pragma once

#include "aion/gameserver/questEngine/handlers/models/WorkOrdersData.xml.h"

namespace aion::gameserver::questEngine::handlers::models {

/** Java com.aionemu.gameserver.questEngine.handlers.models.WorkOrdersData. @author Mr. Poke, Bobobear, Pad */
class WorkOrdersData : public ::aion::gameserver::questEngine::handlers::models::XMLQuest {
#include "aion/gameserver/questEngine/handlers/models/WorkOrdersData.xml.inc"
public:
};

} // namespace aion::gameserver::questEngine::handlers::models
