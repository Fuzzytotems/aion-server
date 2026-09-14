#pragma once

#include "aion/gameserver/questEngine/handlers/models/MonsterHuntData.xml.h"

namespace aion::gameserver::questEngine::handlers::models {

/** Java com.aionemu.gameserver.questEngine.handlers.models.MonsterHuntData. @author MrPoke, Bobobear, Pad */
class MonsterHuntData : public ::aion::gameserver::questEngine::handlers::models::XMLQuest {
#include "aion/gameserver/questEngine/handlers/models/MonsterHuntData.xml.inc"
public:
};

} // namespace aion::gameserver::questEngine::handlers::models
