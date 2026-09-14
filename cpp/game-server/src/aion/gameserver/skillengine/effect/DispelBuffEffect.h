#pragma once

#include "aion/gameserver/skillengine/effect/DispelBuffEffect.xml.h"

namespace aion::gameserver::skillengine::effect {

/** Java com.aionemu.gameserver.skillengine.effect.DispelBuffEffect. @author ATracer */
class DispelBuffEffect : public ::aion::gameserver::skillengine::effect::AbstractDispelEffect {
#include "aion/gameserver/skillengine/effect/DispelBuffEffect.xml.inc"
public:
};

} // namespace aion::gameserver::skillengine::effect
