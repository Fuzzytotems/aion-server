#pragma once

#include "aion/gameserver/skillengine/effect/SummonEffect.xml.h"

namespace aion::gameserver::skillengine::effect {

/** Java com.aionemu.gameserver.skillengine.effect.SummonEffect. @author Simple */
class SummonEffect : public ::aion::gameserver::skillengine::effect::EffectTemplate {
#include "aion/gameserver/skillengine/effect/SummonEffect.xml.inc"
public:
};

} // namespace aion::gameserver::skillengine::effect
