#pragma once

#include "aion/gameserver/skillengine/effect/StaggerEffect.xml.h"

namespace aion::gameserver::skillengine::effect {

/** Java com.aionemu.gameserver.skillengine.effect.StaggerEffect. @author ATracer */
class StaggerEffect : public ::aion::gameserver::skillengine::effect::EffectTemplate {
#include "aion/gameserver/skillengine/effect/StaggerEffect.xml.inc"
public:
};

} // namespace aion::gameserver::skillengine::effect
