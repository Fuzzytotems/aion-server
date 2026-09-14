#pragma once

#include "aion/gameserver/skillengine/effect/SignetEffect.xml.h"

namespace aion::gameserver::skillengine::effect {

/** Java com.aionemu.gameserver.skillengine.effect.SignetEffect. @author ATracer */
class SignetEffect : public ::aion::gameserver::skillengine::effect::EffectTemplate {
#include "aion/gameserver/skillengine/effect/SignetEffect.xml.inc"
public:
};

} // namespace aion::gameserver::skillengine::effect
