#pragma once

#include "aion/gameserver/skillengine/effect/UtilityEffect.xml.h"

namespace aion::gameserver::skillengine::effect {

/** Java com.aionemu.gameserver.skillengine.effect.UtilityEffect. @author Bobobear */
class UtilityEffect : public ::aion::gameserver::skillengine::effect::EffectTemplate {
#include "aion/gameserver/skillengine/effect/UtilityEffect.xml.inc"
public:
};

} // namespace aion::gameserver::skillengine::effect
