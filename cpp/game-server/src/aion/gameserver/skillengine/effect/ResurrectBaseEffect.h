#pragma once

#include "aion/gameserver/skillengine/effect/ResurrectBaseEffect.xml.h"

namespace aion::gameserver::skillengine::effect {

/** Java com.aionemu.gameserver.skillengine.effect.ResurrectBaseEffect. */
class ResurrectBaseEffect : public ::aion::gameserver::skillengine::effect::ResurrectEffect {
#include "aion/gameserver/skillengine/effect/ResurrectBaseEffect.xml.inc"
public:
};

} // namespace aion::gameserver::skillengine::effect
