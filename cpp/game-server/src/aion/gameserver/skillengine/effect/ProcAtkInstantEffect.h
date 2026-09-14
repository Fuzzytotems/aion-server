#pragma once

#include "aion/gameserver/skillengine/effect/ProcAtkInstantEffect.xml.h"

namespace aion::gameserver::skillengine::effect {

/** Java com.aionemu.gameserver.skillengine.effect.ProcAtkInstantEffect. @author Wakizashi */
class ProcAtkInstantEffect : public ::aion::gameserver::skillengine::effect::DamageEffect {
#include "aion/gameserver/skillengine/effect/ProcAtkInstantEffect.xml.inc"
public:
};

} // namespace aion::gameserver::skillengine::effect
