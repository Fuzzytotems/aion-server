#pragma once

#include "aion/gameserver/skillengine/effect/StatdownEffect.xml.h"

namespace aion::gameserver::skillengine::effect {

/** Java com.aionemu.gameserver.skillengine.effect.StatdownEffect. @author ATracer */
class StatdownEffect : public ::aion::gameserver::skillengine::effect::BufEffect {
#include "aion/gameserver/skillengine/effect/StatdownEffect.xml.inc"
public:
};

} // namespace aion::gameserver::skillengine::effect
