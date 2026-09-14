#pragma once

#include "aion/gameserver/skillengine/effect/SpinEffect.xml.h"

namespace aion::gameserver::skillengine::effect {

/** Java com.aionemu.gameserver.skillengine.effect.SpinEffect. @author ATracer */
class SpinEffect : public ::aion::gameserver::skillengine::effect::EffectTemplate {
#include "aion/gameserver/skillengine/effect/SpinEffect.xml.inc"
public:
};

} // namespace aion::gameserver::skillengine::effect
