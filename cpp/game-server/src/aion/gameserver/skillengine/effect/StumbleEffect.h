#pragma once

#include "aion/gameserver/skillengine/effect/StumbleEffect.xml.h"

namespace aion::gameserver::skillengine::effect {

/** Java com.aionemu.gameserver.skillengine.effect.StumbleEffect. @author ATracer */
class StumbleEffect : public ::aion::gameserver::skillengine::effect::EffectTemplate {
#include "aion/gameserver/skillengine/effect/StumbleEffect.xml.inc"
public:
};

} // namespace aion::gameserver::skillengine::effect
