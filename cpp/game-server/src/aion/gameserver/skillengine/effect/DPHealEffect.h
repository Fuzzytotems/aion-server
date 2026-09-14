#pragma once

#include "aion/gameserver/skillengine/effect/DPHealEffect.xml.h"

namespace aion::gameserver::skillengine::effect {

/** Java com.aionemu.gameserver.skillengine.effect.DPHealEffect. @author kecimis */
class DPHealEffect : public ::aion::gameserver::skillengine::effect::HealOverTimeEffect {
#include "aion/gameserver/skillengine/effect/DPHealEffect.xml.inc"
public:
};

} // namespace aion::gameserver::skillengine::effect
