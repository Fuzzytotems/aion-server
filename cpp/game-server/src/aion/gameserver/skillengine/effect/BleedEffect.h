#pragma once

#include "aion/gameserver/skillengine/effect/BleedEffect.xml.h"

namespace aion::gameserver::skillengine::effect {

/** Java com.aionemu.gameserver.skillengine.effect.BleedEffect. @author ATracer, kecimis */
class BleedEffect : public ::aion::gameserver::skillengine::effect::AbstractOverTimeEffect {
#include "aion/gameserver/skillengine/effect/BleedEffect.xml.inc"
public:
};

} // namespace aion::gameserver::skillengine::effect
