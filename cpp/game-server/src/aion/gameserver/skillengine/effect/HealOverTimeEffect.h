#pragma once

#include "aion/gameserver/skillengine/effect/HealOverTimeEffect.xml.h"

namespace aion::gameserver::skillengine::effect {

/** Java com.aionemu.gameserver.skillengine.effect.HealOverTimeEffect. @author ATracer, kecimis */
class HealOverTimeEffect : public ::aion::gameserver::skillengine::effect::AbstractOverTimeEffect {
#include "aion/gameserver/skillengine/effect/HealOverTimeEffect.xml.inc"
public:
};

} // namespace aion::gameserver::skillengine::effect
