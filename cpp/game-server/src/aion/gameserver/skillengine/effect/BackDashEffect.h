#pragma once

#include "aion/gameserver/skillengine/effect/BackDashEffect.xml.h"

namespace aion::gameserver::skillengine::effect {

/** Java com.aionemu.gameserver.skillengine.effect.BackDashEffect. @author ATracer */
class BackDashEffect : public ::aion::gameserver::skillengine::effect::DamageEffect {
#include "aion/gameserver/skillengine/effect/BackDashEffect.xml.inc"
public:
};

} // namespace aion::gameserver::skillengine::effect
