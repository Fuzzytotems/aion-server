#pragma once

#include "aion/gameserver/skillengine/effect/SkillLauncherEffect.xml.h"

namespace aion::gameserver::skillengine::effect {

/** Java com.aionemu.gameserver.skillengine.effect.SkillLauncherEffect. @author ATracer */
class SkillLauncherEffect : public ::aion::gameserver::skillengine::effect::EffectTemplate {
#include "aion/gameserver/skillengine/effect/SkillLauncherEffect.xml.inc"
public:
};

} // namespace aion::gameserver::skillengine::effect
