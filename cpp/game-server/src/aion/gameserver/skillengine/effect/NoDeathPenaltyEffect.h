#pragma once

#include "aion/gameserver/skillengine/effect/NoDeathPenaltyEffect.xml.h"

#include "aion/gameserver/skillengine/model/fwd.h"

namespace aion::gameserver::skillengine::effect {

/** Java com.aionemu.gameserver.skillengine.effect.NoDeathPenaltyEffect. */
class NoDeathPenaltyEffect : public ::aion::gameserver::skillengine::effect::BufEffect {
#include "aion/gameserver/skillengine/effect/NoDeathPenaltyEffect.xml.inc"
public:
	using BufEffect::calculate; // C++ name hiding by the declaration below (hub-headers.md §9.1)

	void calculate(model::Effect& effect) const override;
};

} // namespace aion::gameserver::skillengine::effect
