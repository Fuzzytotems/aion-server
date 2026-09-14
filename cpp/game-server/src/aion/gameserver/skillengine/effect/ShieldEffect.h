#pragma once

#include "aion/gameserver/skillengine/effect/ShieldEffect.xml.h"

namespace aion::gameserver::skillengine::effect {

/** Java com.aionemu.gameserver.skillengine.effect.ShieldEffect. @author ATracer, Wakizashi, Sippolo, kecimis */
class ShieldEffect : public ::aion::gameserver::skillengine::effect::EffectTemplate {
#include "aion/gameserver/skillengine/effect/ShieldEffect.xml.inc"
public:
};

} // namespace aion::gameserver::skillengine::effect
