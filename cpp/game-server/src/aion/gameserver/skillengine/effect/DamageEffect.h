#pragma once

#include "aion/gameserver/skillengine/effect/DamageEffect.xml.h"

namespace aion::gameserver::skillengine::effect {

/** Java com.aionemu.gameserver.skillengine.effect.DamageEffect. @author ATracer */
class DamageEffect : public ::aion::gameserver::skillengine::effect::EffectTemplate {
#include "aion/gameserver/skillengine/effect/DamageEffect.xml.inc"
public:
};

} // namespace aion::gameserver::skillengine::effect
