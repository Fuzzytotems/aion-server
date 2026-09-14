#pragma once

#include "aion/gameserver/skillengine/effect/BoostSkillCostEffect.xml.h"

namespace aion::gameserver::skillengine::effect {

/** Java com.aionemu.gameserver.skillengine.effect.BoostSkillCostEffect. @author Rama and Sippolo */
class BoostSkillCostEffect : public ::aion::gameserver::skillengine::effect::BufEffect {
#include "aion/gameserver/skillengine/effect/BoostSkillCostEffect.xml.inc"
public:
};

} // namespace aion::gameserver::skillengine::effect
