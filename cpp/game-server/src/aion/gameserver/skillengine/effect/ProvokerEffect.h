#pragma once

#include "aion/gameserver/skillengine/effect/ProvokerEffect.xml.h"

namespace aion::gameserver::skillengine::effect {

/** Java com.aionemu.gameserver.skillengine.effect.ProvokerEffect. @author ATracer, kecimis */
class ProvokerEffect : public ::aion::gameserver::skillengine::effect::ShieldEffect {
#include "aion/gameserver/skillengine/effect/ProvokerEffect.xml.inc"
public:
};

} // namespace aion::gameserver::skillengine::effect
