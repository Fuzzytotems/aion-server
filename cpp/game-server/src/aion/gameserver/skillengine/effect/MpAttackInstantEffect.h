#pragma once

#include "aion/gameserver/skillengine/effect/MpAttackInstantEffect.xml.h"

namespace aion::gameserver::skillengine::effect {

/** Java com.aionemu.gameserver.skillengine.effect.MpAttackInstantEffect. @author Sippolo */
class MpAttackInstantEffect : public ::aion::gameserver::skillengine::effect::EffectTemplate {
#include "aion/gameserver/skillengine/effect/MpAttackInstantEffect.xml.inc"
public:
};

} // namespace aion::gameserver::skillengine::effect
