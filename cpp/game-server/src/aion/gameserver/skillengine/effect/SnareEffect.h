#pragma once

#include "aion/gameserver/skillengine/effect/SnareEffect.xml.h"

namespace aion::gameserver::skillengine::effect {

/** Java com.aionemu.gameserver.skillengine.effect.SnareEffect. @author ATracer */
class SnareEffect : public ::aion::gameserver::skillengine::effect::BufEffect {
#include "aion/gameserver/skillengine/effect/SnareEffect.xml.inc"
public:
};

} // namespace aion::gameserver::skillengine::effect
