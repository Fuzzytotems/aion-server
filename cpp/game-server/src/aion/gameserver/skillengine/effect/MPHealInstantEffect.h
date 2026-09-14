#pragma once

#include "aion/gameserver/skillengine/effect/MPHealInstantEffect.xml.h"

namespace aion::gameserver::skillengine::effect {

/** Java com.aionemu.gameserver.skillengine.effect.MPHealInstantEffect. @author ATracer */
class MPHealInstantEffect : public ::aion::gameserver::skillengine::effect::AbstractHealEffect {
#include "aion/gameserver/skillengine/effect/MPHealInstantEffect.xml.inc"
public:
};

} // namespace aion::gameserver::skillengine::effect
