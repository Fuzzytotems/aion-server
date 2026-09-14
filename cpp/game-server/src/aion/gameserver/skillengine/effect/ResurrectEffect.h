#pragma once

#include "aion/gameserver/skillengine/effect/ResurrectEffect.xml.h"

namespace aion::gameserver::skillengine::effect {

/** Java com.aionemu.gameserver.skillengine.effect.ResurrectEffect. @author ATracer */
class ResurrectEffect : public ::aion::gameserver::skillengine::effect::EffectTemplate {
#include "aion/gameserver/skillengine/effect/ResurrectEffect.xml.inc"
public:
};

} // namespace aion::gameserver::skillengine::effect
