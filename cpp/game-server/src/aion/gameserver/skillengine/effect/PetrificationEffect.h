#pragma once

#include "aion/gameserver/skillengine/effect/PetrificationEffect.xml.h"

namespace aion::gameserver::skillengine::effect {

/** Java com.aionemu.gameserver.skillengine.effect.PetrificationEffect. @author ATracer, kecimis */
class PetrificationEffect : public ::aion::gameserver::skillengine::effect::EffectTemplate {
#include "aion/gameserver/skillengine/effect/PetrificationEffect.xml.inc"
public:
};

} // namespace aion::gameserver::skillengine::effect
