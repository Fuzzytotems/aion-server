#pragma once

#include "aion/gameserver/skillengine/effect/HealInstantEffect.xml.h"

namespace aion::gameserver::skillengine::effect {

/** Java com.aionemu.gameserver.skillengine.effect.HealInstantEffect. @author ATracer, vlog, Sippolo, kecimis */
class HealInstantEffect : public ::aion::gameserver::skillengine::effect::AbstractHealEffect {
#include "aion/gameserver/skillengine/effect/HealInstantEffect.xml.inc"
public:
};

} // namespace aion::gameserver::skillengine::effect
