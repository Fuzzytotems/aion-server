#pragma once

#include "aion/gameserver/skillengine/effect/DispelEffect.xml.h"

namespace aion::gameserver::skillengine::effect {

/** Java com.aionemu.gameserver.skillengine.effect.DispelEffect. @author ATracer */
class DispelEffect : public ::aion::gameserver::skillengine::effect::EffectTemplate {
#include "aion/gameserver/skillengine/effect/DispelEffect.xml.inc"
public:
};

} // namespace aion::gameserver::skillengine::effect
