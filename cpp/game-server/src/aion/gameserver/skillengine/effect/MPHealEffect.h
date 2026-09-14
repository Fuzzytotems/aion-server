#pragma once

#include "aion/gameserver/skillengine/effect/MPHealEffect.xml.h"

namespace aion::gameserver::skillengine::effect {

/** Java com.aionemu.gameserver.skillengine.effect.MPHealEffect. @author kecimis */
class MPHealEffect : public ::aion::gameserver::skillengine::effect::HealOverTimeEffect {
#include "aion/gameserver/skillengine/effect/MPHealEffect.xml.inc"
public:
};

} // namespace aion::gameserver::skillengine::effect
