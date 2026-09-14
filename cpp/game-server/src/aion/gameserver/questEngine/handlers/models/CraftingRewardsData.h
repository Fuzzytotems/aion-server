#pragma once

#include "aion/gameserver/questEngine/handlers/models/CraftingRewardsData.xml.h"

namespace aion::gameserver::questEngine::handlers::models {

/** Java com.aionemu.gameserver.questEngine.handlers.models.CraftingRewardsData. @author Bobobear, Pad */
class CraftingRewardsData : public ::aion::gameserver::questEngine::handlers::models::XMLQuest {
#include "aion/gameserver/questEngine/handlers/models/CraftingRewardsData.xml.inc"
public:
};

} // namespace aion::gameserver::questEngine::handlers::models
