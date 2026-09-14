#pragma once

#include "aion/gameserver/skillengine/effect/FPHealEffect.xml.h"

namespace aion::gameserver::skillengine::effect {

/** Java com.aionemu.gameserver.skillengine.effect.FPHealEffect. @author kecimis */
class FPHealEffect : public ::aion::gameserver::skillengine::effect::HealOverTimeEffect {
#include "aion/gameserver/skillengine/effect/FPHealEffect.xml.inc"
public:
};

} // namespace aion::gameserver::skillengine::effect
