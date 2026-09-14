#pragma once

#include "aion/gameserver/skillengine/effect/FallEffect.xml.h"

namespace aion::gameserver::skillengine::effect {

/** Java com.aionemu.gameserver.skillengine.effect.FallEffect. @author Sippolo */
class FallEffect : public ::aion::gameserver::skillengine::effect::EffectTemplate {
#include "aion/gameserver/skillengine/effect/FallEffect.xml.inc"
public:
};

} // namespace aion::gameserver::skillengine::effect
