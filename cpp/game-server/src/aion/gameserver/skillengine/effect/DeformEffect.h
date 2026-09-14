#pragma once

#include "aion/gameserver/skillengine/effect/DeformEffect.xml.h"

namespace aion::gameserver::skillengine::effect {

/** Java com.aionemu.gameserver.skillengine.effect.DeformEffect. @author ATracer */
class DeformEffect : public ::aion::gameserver::skillengine::effect::TransformEffect {
#include "aion/gameserver/skillengine/effect/DeformEffect.xml.inc"
public:
};

} // namespace aion::gameserver::skillengine::effect
