#pragma once

#include "aion/gameserver/skillengine/effect/ReflectorEffect.xml.h"

namespace aion::gameserver::skillengine::effect {

/** Java com.aionemu.gameserver.skillengine.effect.ReflectorEffect. @author ginho1, Wakizashi, kecimis, Neon */
class ReflectorEffect : public ::aion::gameserver::skillengine::effect::ShieldEffect {
#include "aion/gameserver/skillengine/effect/ReflectorEffect.xml.inc"
public:
};

} // namespace aion::gameserver::skillengine::effect
