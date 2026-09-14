#pragma once

#include "aion/gameserver/skillengine/effect/BlindEffect.xml.h"

namespace aion::gameserver::skillengine::effect {

/** Java com.aionemu.gameserver.skillengine.effect.BlindEffect. @author ATracer */
class BlindEffect : public ::aion::gameserver::skillengine::effect::EffectTemplate {
#include "aion/gameserver/skillengine/effect/BlindEffect.xml.inc"
public:
};

} // namespace aion::gameserver::skillengine::effect
