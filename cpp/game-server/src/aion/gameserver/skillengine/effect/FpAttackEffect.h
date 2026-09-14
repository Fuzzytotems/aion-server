#pragma once

#include "aion/gameserver/skillengine/effect/FpAttackEffect.xml.h"

namespace aion::gameserver::skillengine::effect {

/** Java com.aionemu.gameserver.skillengine.effect.FpAttackEffect. @author Sippolo */
class FpAttackEffect : public ::aion::gameserver::skillengine::effect::AbstractOverTimeEffect {
#include "aion/gameserver/skillengine/effect/FpAttackEffect.xml.inc"
public:
};

} // namespace aion::gameserver::skillengine::effect
