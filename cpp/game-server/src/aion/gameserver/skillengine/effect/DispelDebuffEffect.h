#pragma once

#include "aion/gameserver/skillengine/effect/DispelDebuffEffect.xml.h"

namespace aion::gameserver::skillengine::effect {

/** Java com.aionemu.gameserver.skillengine.effect.DispelDebuffEffect. @author ATracer */
class DispelDebuffEffect : public ::aion::gameserver::skillengine::effect::AbstractDispelEffect {
#include "aion/gameserver/skillengine/effect/DispelDebuffEffect.xml.inc"
public:
};

} // namespace aion::gameserver::skillengine::effect
