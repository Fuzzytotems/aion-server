#pragma once

#include "aion/gameserver/skillengine/effect/PoisonEffect.xml.h"

namespace aion::gameserver::skillengine::effect {

/** Java com.aionemu.gameserver.skillengine.effect.PoisonEffect. @author ATracer, kecimis */
class PoisonEffect : public ::aion::gameserver::skillengine::effect::AbstractOverTimeEffect {
#include "aion/gameserver/skillengine/effect/PoisonEffect.xml.inc"
public:
};

} // namespace aion::gameserver::skillengine::effect
