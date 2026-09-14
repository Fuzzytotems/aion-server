#pragma once

#include "aion/gameserver/skillengine/effect/SwitchHpMpEffect.xml.h"

namespace aion::gameserver::skillengine::effect {

/** Java com.aionemu.gameserver.skillengine.effect.SwitchHpMpEffect. @author ATracer */
class SwitchHpMpEffect : public ::aion::gameserver::skillengine::effect::EffectTemplate {
#include "aion/gameserver/skillengine/effect/SwitchHpMpEffect.xml.inc"
public:
};

} // namespace aion::gameserver::skillengine::effect
