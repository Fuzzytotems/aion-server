#pragma once

#include "aion/gameserver/skillengine/effect/HiPassEffect.xml.h"

namespace aion::gameserver::skillengine::effect {

/** Java com.aionemu.gameserver.skillengine.effect.HiPassEffect. */
class HiPassEffect : public ::aion::gameserver::skillengine::effect::BufEffect {
#include "aion/gameserver/skillengine/effect/HiPassEffect.xml.inc"
public:
};

} // namespace aion::gameserver::skillengine::effect
