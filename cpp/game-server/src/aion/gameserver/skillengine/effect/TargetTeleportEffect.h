#pragma once

#include "aion/gameserver/skillengine/effect/TargetTeleportEffect.xml.h"

namespace aion::gameserver::skillengine::effect {

/** Java com.aionemu.gameserver.skillengine.effect.TargetTeleportEffect. @author Rolandas */
class TargetTeleportEffect : public ::aion::gameserver::skillengine::effect::EffectTemplate {
#include "aion/gameserver/skillengine/effect/TargetTeleportEffect.xml.inc"
public:
};

} // namespace aion::gameserver::skillengine::effect
