#pragma once

#include "aion/gameserver/skillengine/effect/WeaponDualEffect.xml.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"

namespace aion::gameserver::skillengine::effect {

/** Java com.aionemu.gameserver.skillengine.effect.WeaponDualEffect. */
class WeaponDualEffect : public ::aion::gameserver::skillengine::effect::BufEffect {
#include "aion/gameserver/skillengine/effect/WeaponDualEffect.xml.inc"
public:
	static bool hasDualWieldEffect(::aion::gameserver::model::gameobjects::player::Player& player);
};

} // namespace aion::gameserver::skillengine::effect
