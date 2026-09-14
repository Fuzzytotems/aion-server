#pragma once

#include "aion/gameserver/skillengine/effect/ShieldMasteryEffect.xml.h"

namespace aion::gameserver::skillengine::effect {

/** Java com.aionemu.gameserver.skillengine.effect.ShieldMasteryEffect. @author VladimirZ */
class ShieldMasteryEffect : public ::aion::gameserver::skillengine::effect::BufEffect {
#include "aion/gameserver/skillengine/effect/ShieldMasteryEffect.xml.inc"
public:
};

} // namespace aion::gameserver::skillengine::effect
