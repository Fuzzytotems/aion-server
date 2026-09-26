#pragma once

#include "aion/gameserver/skillengine/effect/BackDashEffect.xml.h"

#include "aion/gameserver/skillengine/model/fwd.h"

namespace aion::gameserver::skillengine::effect {

/** Java com.aionemu.gameserver.skillengine.effect.BackDashEffect. @author ATracer */
class BackDashEffect : public ::aion::gameserver::skillengine::effect::DamageEffect {
#include "aion/gameserver/skillengine/effect/BackDashEffect.xml.inc"
public:
	using DamageEffect::calculate; // C++ name hiding by the declaration below (hub-headers.md §9.1)

	void calculate(model::Effect& effect) const override;
};

} // namespace aion::gameserver::skillengine::effect
