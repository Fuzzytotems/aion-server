#pragma once

#include "aion/gameserver/skillengine/effect/BuffStunEffect.xml.h"

namespace aion::gameserver::skillengine::effect {

/** Java com.aionemu.gameserver.skillengine.effect.BuffStunEffect. @author kecimis */
class BuffStunEffect : public ::aion::gameserver::skillengine::effect::StunEffect {
#include "aion/gameserver/skillengine/effect/BuffStunEffect.xml.inc"
public:
};

} // namespace aion::gameserver::skillengine::effect
