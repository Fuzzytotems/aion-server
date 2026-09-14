#pragma once

#include "aion/gameserver/skillengine/effect/SilenceEffect.xml.h"

namespace aion::gameserver::skillengine::effect {

/** Java com.aionemu.gameserver.skillengine.effect.SilenceEffect. @author ATracer */
class SilenceEffect : public ::aion::gameserver::skillengine::effect::EffectTemplate {
#include "aion/gameserver/skillengine/effect/SilenceEffect.xml.inc"
public:
};

} // namespace aion::gameserver::skillengine::effect
