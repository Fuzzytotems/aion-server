#pragma once

#include "aion/gameserver/skillengine/effect/LimitedReduceDamageEffect.xml.h"

namespace aion::gameserver::skillengine::effect {

/** Java com.aionemu.gameserver.skillengine.effect.LimitedReduceDamageEffect. @author Bobobear */
class LimitedReduceDamageEffect : public ::aion::gameserver::skillengine::effect::EffectTemplate {
#include "aion/gameserver/skillengine/effect/LimitedReduceDamageEffect.xml.inc"
public:
};

} // namespace aion::gameserver::skillengine::effect
