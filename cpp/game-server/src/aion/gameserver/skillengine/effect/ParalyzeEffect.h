#pragma once

#include "aion/gameserver/skillengine/effect/ParalyzeEffect.xml.h"

namespace aion::gameserver::skillengine::effect {

/** Java com.aionemu.gameserver.skillengine.effect.ParalyzeEffect. @author ATracer */
class ParalyzeEffect : public ::aion::gameserver::skillengine::effect::EffectTemplate {
#include "aion/gameserver/skillengine/effect/ParalyzeEffect.xml.inc"
public:
};

} // namespace aion::gameserver::skillengine::effect
