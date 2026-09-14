#pragma once

#include "aion/gameserver/skillengine/effect/CondSkillLauncherEffect.xml.h"

namespace aion::gameserver::skillengine::effect {

/** Java com.aionemu.gameserver.skillengine.effect.CondSkillLauncherEffect. @author Sippolo */
class CondSkillLauncherEffect : public ::aion::gameserver::skillengine::effect::EffectTemplate {
#include "aion/gameserver/skillengine/effect/CondSkillLauncherEffect.xml.inc"
public:
};

} // namespace aion::gameserver::skillengine::effect
