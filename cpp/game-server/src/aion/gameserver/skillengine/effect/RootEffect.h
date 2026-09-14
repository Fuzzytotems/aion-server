#pragma once

#include "aion/gameserver/skillengine/effect/RootEffect.xml.h"

namespace aion::gameserver::skillengine::effect {

/** Java com.aionemu.gameserver.skillengine.effect.RootEffect. @author ATracer */
class RootEffect : public ::aion::gameserver::skillengine::effect::EffectTemplate {
#include "aion/gameserver/skillengine/effect/RootEffect.xml.inc"
public:
};

} // namespace aion::gameserver::skillengine::effect
