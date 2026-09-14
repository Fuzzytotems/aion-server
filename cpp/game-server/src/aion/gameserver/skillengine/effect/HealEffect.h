#pragma once

#include "aion/gameserver/skillengine/effect/HealEffect.xml.h"

namespace aion::gameserver::skillengine::effect {

/** Java com.aionemu.gameserver.skillengine.effect.HealEffect. @author kecimis */
class HealEffect : public ::aion::gameserver::skillengine::effect::HealOverTimeEffect {
#include "aion/gameserver/skillengine/effect/HealEffect.xml.inc"
public:
};

} // namespace aion::gameserver::skillengine::effect
