#pragma once

#include "aion/gameserver/questEngine/handlers/models/RelicRewardsData.xml.h"

namespace aion::gameserver::questEngine::handlers::models {

/** Java com.aionemu.gameserver.questEngine.handlers.models.RelicRewardsData. @author Bobobear, Pad */
class RelicRewardsData : public ::aion::gameserver::questEngine::handlers::models::XMLQuest {
#include "aion/gameserver/questEngine/handlers/models/RelicRewardsData.xml.inc"
public:
};

} // namespace aion::gameserver::questEngine::handlers::models
