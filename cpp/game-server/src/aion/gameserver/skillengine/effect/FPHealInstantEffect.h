#pragma once

#include "aion/gameserver/skillengine/effect/FPHealInstantEffect.xml.h"

namespace aion::gameserver::skillengine::effect {

/** Java com.aionemu.gameserver.skillengine.effect.FPHealInstantEffect. @author ATracer */
class FPHealInstantEffect : public ::aion::gameserver::skillengine::effect::AbstractHealEffect {
#include "aion/gameserver/skillengine/effect/FPHealInstantEffect.xml.inc"
public:
};

} // namespace aion::gameserver::skillengine::effect
