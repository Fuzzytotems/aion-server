#pragma once

#include "aion/gameserver/skillengine/effect/WeaponDualEffect.xml.h"

#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/skillengine/model/fwd.h"

namespace aion::gameserver::skillengine::effect {

/** Java com.aionemu.gameserver.skillengine.effect.WeaponDualEffect. */
class WeaponDualEffect : public ::aion::gameserver::skillengine::effect::BufEffect {
#include "aion/gameserver/skillengine/effect/WeaponDualEffect.xml.inc"
public:
	void startEffect(model::Effect& effect) const override;

	void endEffect(model::Effect& effect) const override;

	static bool hasDualWieldEffect(::aion::gameserver::model::gameobjects::player::Player& player);
};

} // namespace aion::gameserver::skillengine::effect
