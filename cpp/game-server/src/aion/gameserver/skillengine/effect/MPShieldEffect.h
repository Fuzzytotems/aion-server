#pragma once

#include "aion/gameserver/skillengine/effect/MPShieldEffect.xml.h"

namespace aion::gameserver::skillengine::effect {

/** Java com.aionemu.gameserver.skillengine.effect.MPShieldEffect. @author Cheatkiller */
class MPShieldEffect : public ::aion::gameserver::skillengine::effect::ShieldEffect {
#include "aion/gameserver/skillengine/effect/MPShieldEffect.xml.inc"
public:
};

} // namespace aion::gameserver::skillengine::effect
