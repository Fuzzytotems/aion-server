#pragma once

#include "aion/gameserver/skillengine/effect/BindEffect.xml.h"

namespace aion::gameserver::skillengine::effect {

/** Java com.aionemu.gameserver.skillengine.effect.BindEffect. @author ATracer */
class BindEffect : public ::aion::gameserver::skillengine::effect::EffectTemplate {
#include "aion/gameserver/skillengine/effect/BindEffect.xml.inc"
public:
};

} // namespace aion::gameserver::skillengine::effect
