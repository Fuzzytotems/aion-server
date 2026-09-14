#pragma once

#include "aion/gameserver/skillengine/effect/EffectTemplate.xml.h"

namespace aion::gameserver::skillengine::effect {

/** Java com.aionemu.gameserver.skillengine.effect.EffectTemplate. @author ATracer */
class EffectTemplate : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/skillengine/effect/EffectTemplate.xml.inc"
public:
};

} // namespace aion::gameserver::skillengine::effect
