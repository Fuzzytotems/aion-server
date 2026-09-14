#pragma once

#include "aion/gameserver/skillengine/effect/SwitchHostileEffect.xml.h"

namespace aion::gameserver::skillengine::effect {

/** Java com.aionemu.gameserver.skillengine.effect.SwitchHostileEffect. @author Luzien */
class SwitchHostileEffect : public ::aion::gameserver::skillengine::effect::EffectTemplate {
#include "aion/gameserver/skillengine/effect/SwitchHostileEffect.xml.inc"
public:
};

} // namespace aion::gameserver::skillengine::effect
