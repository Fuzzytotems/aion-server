#pragma once

#include "aion/gameserver/skillengine/effect/ProcHealInstantEffect.xml.h"

namespace aion::gameserver::skillengine::effect {

/** Java com.aionemu.gameserver.skillengine.effect.ProcHealInstantEffect. @author ATracer, Sippolo */
class ProcHealInstantEffect : public ::aion::gameserver::skillengine::effect::AbstractHealEffect {
#include "aion/gameserver/skillengine/effect/ProcHealInstantEffect.xml.inc"
public:
};

} // namespace aion::gameserver::skillengine::effect
