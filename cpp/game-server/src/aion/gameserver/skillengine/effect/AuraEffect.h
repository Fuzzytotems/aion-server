#pragma once

#include "aion/gameserver/skillengine/effect/AuraEffect.xml.h"

namespace aion::gameserver::skillengine::effect {

/** Java com.aionemu.gameserver.skillengine.effect.AuraEffect. @author ATracer, kecimis, xTz */
class AuraEffect : public ::aion::gameserver::skillengine::effect::EffectTemplate {
#include "aion/gameserver/skillengine/effect/AuraEffect.xml.inc"
public:
};

} // namespace aion::gameserver::skillengine::effect
