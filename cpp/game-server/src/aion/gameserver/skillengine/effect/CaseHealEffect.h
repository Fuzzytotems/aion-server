#pragma once

#include "aion/gameserver/skillengine/effect/CaseHealEffect.xml.h"

namespace aion::gameserver::skillengine::effect {

/** Java com.aionemu.gameserver.skillengine.effect.CaseHealEffect. @author kecimis */
class CaseHealEffect : public ::aion::gameserver::skillengine::effect::AbstractHealEffect {
#include "aion/gameserver/skillengine/effect/CaseHealEffect.xml.inc"
public:
};

} // namespace aion::gameserver::skillengine::effect
