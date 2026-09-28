#pragma once

#include "aion/gameserver/skillengine/effect/NoResurrectPenaltyEffect.xml.h"

#include "aion/gameserver/skillengine/model/fwd.h"

namespace aion::gameserver::skillengine::effect {

/** Java com.aionemu.gameserver.skillengine.effect.NoResurrectPenaltyEffect. */
class NoResurrectPenaltyEffect : public ::aion::gameserver::skillengine::effect::BufEffect {
#include "aion/gameserver/skillengine/effect/NoResurrectPenaltyEffect.xml.inc"
public:
	using BufEffect::calculate; // C++ name hiding by the declaration below (hub-headers.md §9.1)

	void calculate(model::Effect& effect) const override;
};

} // namespace aion::gameserver::skillengine::effect
