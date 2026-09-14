#pragma once

#include "aion/gameserver/skillengine/effect/BufEffect.xml.h"

namespace aion::gameserver::skillengine::effect {

/** Java com.aionemu.gameserver.skillengine.effect.BufEffect. @author ATracer */
class BufEffect : public ::aion::gameserver::skillengine::effect::EffectTemplate {
#include "aion/gameserver/skillengine/effect/BufEffect.xml.inc"
public:
};

} // namespace aion::gameserver::skillengine::effect
