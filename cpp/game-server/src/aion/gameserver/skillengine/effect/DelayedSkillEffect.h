#pragma once

#include "aion/gameserver/skillengine/effect/DelayedSkillEffect.xml.h"

namespace aion::gameserver::skillengine::effect {

/** Java com.aionemu.gameserver.skillengine.effect.DelayedSkillEffect. @author kecimis, Cheatkiller */
class DelayedSkillEffect : public ::aion::gameserver::skillengine::effect::EffectTemplate {
#include "aion/gameserver/skillengine/effect/DelayedSkillEffect.xml.inc"
public:
};

} // namespace aion::gameserver::skillengine::effect
