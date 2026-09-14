#pragma once

#include "aion/gameserver/questEngine/handlers/models/Monster.xml.h"

namespace aion::gameserver::questEngine::handlers::models {

/** Java com.aionemu.gameserver.questEngine.handlers.models.Monster. @author MrPoke, vlog, Bobobear, Artur, Pad */
class Monster : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/questEngine/handlers/models/Monster.xml.inc"
public:
};

} // namespace aion::gameserver::questEngine::handlers::models
