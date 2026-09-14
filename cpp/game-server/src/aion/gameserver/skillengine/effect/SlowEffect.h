#pragma once

#include "aion/gameserver/skillengine/effect/SlowEffect.xml.h"

namespace aion::gameserver::skillengine::effect {

/** Java com.aionemu.gameserver.skillengine.effect.SlowEffect. @author ATracer */
class SlowEffect : public ::aion::gameserver::skillengine::effect::BufEffect {
#include "aion/gameserver/skillengine/effect/SlowEffect.xml.inc"
public:
};

} // namespace aion::gameserver::skillengine::effect
