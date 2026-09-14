#pragma once

#include "aion/gameserver/skillengine/effect/DashEffect.xml.h"

namespace aion::gameserver::skillengine::effect {

/** Java com.aionemu.gameserver.skillengine.effect.DashEffect. @author ATracer */
class DashEffect : public ::aion::gameserver::skillengine::effect::DamageEffect {
#include "aion/gameserver/skillengine/effect/DashEffect.xml.inc"
public:
};

} // namespace aion::gameserver::skillengine::effect
