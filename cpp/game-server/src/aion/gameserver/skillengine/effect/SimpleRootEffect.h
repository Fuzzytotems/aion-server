#pragma once

#include "aion/gameserver/skillengine/effect/SimpleRootEffect.xml.h"

namespace aion::gameserver::skillengine::effect {

/** Java com.aionemu.gameserver.skillengine.effect.SimpleRootEffect. @author VladimirZ, Cheatkiller */
class SimpleRootEffect : public ::aion::gameserver::skillengine::effect::EffectTemplate {
#include "aion/gameserver/skillengine/effect/SimpleRootEffect.xml.inc"
public:
};

} // namespace aion::gameserver::skillengine::effect
