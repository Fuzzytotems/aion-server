#pragma once

#include "aion/gameserver/skillengine/effect/HideEffect.xml.h"

namespace aion::gameserver::skillengine::effect {

/** Java com.aionemu.gameserver.skillengine.effect.HideEffect. @author Sweetkr, Cura */
class HideEffect : public ::aion::gameserver::skillengine::effect::BufEffect {
#include "aion/gameserver/skillengine/effect/HideEffect.xml.inc"
public:
};

} // namespace aion::gameserver::skillengine::effect
