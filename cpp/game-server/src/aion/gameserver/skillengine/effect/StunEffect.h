#pragma once

#include "aion/gameserver/skillengine/effect/StunEffect.xml.h"

namespace aion::gameserver::skillengine::effect {

/** Java com.aionemu.gameserver.skillengine.effect.StunEffect. @author ATracer */
class StunEffect : public ::aion::gameserver::skillengine::effect::EffectTemplate {
#include "aion/gameserver/skillengine/effect/StunEffect.xml.inc"
public:
};

} // namespace aion::gameserver::skillengine::effect
