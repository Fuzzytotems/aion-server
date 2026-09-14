#pragma once

#include "aion/gameserver/skillengine/effect/TargetChangeEffect.xml.h"

namespace aion::gameserver::skillengine::effect {

/** Java com.aionemu.gameserver.skillengine.effect.TargetChangeEffect. @author Bobobear */
class TargetChangeEffect : public ::aion::gameserver::skillengine::effect::EffectTemplate {
#include "aion/gameserver/skillengine/effect/TargetChangeEffect.xml.inc"
public:
};

} // namespace aion::gameserver::skillengine::effect
