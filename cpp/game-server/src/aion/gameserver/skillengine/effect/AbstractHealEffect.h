#pragma once

#include "aion/gameserver/skillengine/effect/AbstractHealEffect.xml.h"

namespace aion::gameserver::skillengine::effect {

/** Java com.aionemu.gameserver.skillengine.effect.AbstractHealEffect. @author ATracer, Wakizashi, kecimis */
class AbstractHealEffect : public ::aion::gameserver::skillengine::effect::EffectTemplate {
#include "aion/gameserver/skillengine/effect/AbstractHealEffect.xml.inc"
public:
};

} // namespace aion::gameserver::skillengine::effect
