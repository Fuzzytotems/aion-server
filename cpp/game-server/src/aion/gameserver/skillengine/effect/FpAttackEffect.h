#pragma once

#include "aion/gameserver/skillengine/effect/FpAttackEffect.xml.h"

#include "aion/gameserver/skillengine/model/fwd.h"

namespace aion::gameserver::skillengine::effect {

/** Java com.aionemu.gameserver.skillengine.effect.FpAttackEffect. @author Sippolo */
class FpAttackEffect : public ::aion::gameserver::skillengine::effect::AbstractOverTimeEffect {
#include "aion/gameserver/skillengine/effect/FpAttackEffect.xml.inc"
public:
	using AbstractOverTimeEffect::calculate; // C++ name hiding by the declaration below (hub-headers.md §9.1)

	void calculate(model::Effect& effect) const override;

	void onPeriodicAction(model::Effect& effect) const override;
};

} // namespace aion::gameserver::skillengine::effect
