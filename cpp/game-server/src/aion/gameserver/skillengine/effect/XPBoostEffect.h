#pragma once

#include "aion/gameserver/skillengine/effect/XPBoostEffect.xml.h"

namespace aion::gameserver::skillengine::effect {

/** Java com.aionemu.gameserver.skillengine.effect.XPBoostEffect. @author antness */
class XPBoostEffect : public ::aion::gameserver::skillengine::effect::BufEffect {
#include "aion/gameserver/skillengine/effect/XPBoostEffect.xml.inc"
public:
};

} // namespace aion::gameserver::skillengine::effect
