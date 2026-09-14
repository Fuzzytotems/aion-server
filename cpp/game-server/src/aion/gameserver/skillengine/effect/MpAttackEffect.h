#pragma once

#include "aion/gameserver/skillengine/effect/MpAttackEffect.xml.h"

namespace aion::gameserver::skillengine::effect {

/** Java com.aionemu.gameserver.skillengine.effect.MpAttackEffect. @author Sippolo */
class MpAttackEffect : public ::aion::gameserver::skillengine::effect::AbstractOverTimeEffect {
#include "aion/gameserver/skillengine/effect/MpAttackEffect.xml.inc"
public:
};

} // namespace aion::gameserver::skillengine::effect
