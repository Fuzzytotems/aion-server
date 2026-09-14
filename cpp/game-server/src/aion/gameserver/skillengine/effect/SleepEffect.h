#pragma once

#include "aion/gameserver/skillengine/effect/SleepEffect.xml.h"

namespace aion::gameserver::skillengine::effect {

/** Java com.aionemu.gameserver.skillengine.effect.SleepEffect. @author ATracer */
class SleepEffect : public ::aion::gameserver::skillengine::effect::EffectTemplate {
#include "aion/gameserver/skillengine/effect/SleepEffect.xml.inc"
public:
};

} // namespace aion::gameserver::skillengine::effect
