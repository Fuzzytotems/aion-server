#pragma once

#include "aion/gameserver/skillengine/effect/NoFlyEffect.xml.h"

namespace aion::gameserver::skillengine::effect {

/** Java com.aionemu.gameserver.skillengine.effect.NoFlyEffect. @author Sippolo */
class NoFlyEffect : public ::aion::gameserver::skillengine::effect::EffectTemplate {
#include "aion/gameserver/skillengine/effect/NoFlyEffect.xml.inc"
public:
};

} // namespace aion::gameserver::skillengine::effect
