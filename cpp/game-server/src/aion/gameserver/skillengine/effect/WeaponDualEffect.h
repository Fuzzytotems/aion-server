#pragma once

#include "aion/gameserver/skillengine/effect/WeaponDualEffect.xml.h"

namespace aion::gameserver::skillengine::effect {

/** Java com.aionemu.gameserver.skillengine.effect.WeaponDualEffect. */
class WeaponDualEffect : public ::aion::gameserver::skillengine::effect::BufEffect {
#include "aion/gameserver/skillengine/effect/WeaponDualEffect.xml.inc"
public:
};

} // namespace aion::gameserver::skillengine::effect
