#pragma once

#include "aion/gameserver/questEngine/handlers/models/SkillUseData.xml.h"

namespace aion::gameserver::questEngine::handlers::models {

/** Java com.aionemu.gameserver.questEngine.handlers.models.SkillUseData. @author vlog, Bobobear, Pad, Neon */
class SkillUseData : public ::aion::gameserver::questEngine::handlers::models::XMLQuest {
#include "aion/gameserver/questEngine/handlers/models/SkillUseData.xml.inc"
public:
};

} // namespace aion::gameserver::questEngine::handlers::models
