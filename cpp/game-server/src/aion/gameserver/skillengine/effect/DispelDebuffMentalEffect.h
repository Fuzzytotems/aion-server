#pragma once

#include "aion/gameserver/skillengine/effect/DispelDebuffMentalEffect.xml.h"
#include "aion/gameserver/skillengine/model/fwd.h"

namespace aion::gameserver::skillengine::effect {

/** Java com.aionemu.gameserver.skillengine.effect.DispelDebuffMentalEffect. @author ATracer */
class DispelDebuffMentalEffect : public ::aion::gameserver::skillengine::effect::AbstractDispelEffect {
#include "aion/gameserver/skillengine/effect/DispelDebuffMentalEffect.xml.inc"
public:
	void applyEffect(model::Effect& effect) const override;
};

} // namespace aion::gameserver::skillengine::effect
