#pragma once

#include "aion/gameserver/skillengine/effect/StatupEffect.xml.h"

namespace aion::gameserver::skillengine::effect {

/** Java com.aionemu.gameserver.skillengine.effect.StatupEffect. @author ATracer */
class StatupEffect : public ::aion::gameserver::skillengine::effect::BufEffect {
#include "aion/gameserver/skillengine/effect/StatupEffect.xml.inc"
public:
};

} // namespace aion::gameserver::skillengine::effect
