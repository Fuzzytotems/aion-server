#pragma once

#include "aion/gameserver/skillengine/effect/ProcDPHealInstantEffect.xml.h"

#include "aion/gameserver/skillengine/model/fwd.h"

namespace aion::gameserver::skillengine::effect {

/** Java com.aionemu.gameserver.skillengine.effect.ProcDPHealInstantEffect. @author ATracer */
class ProcDPHealInstantEffect : public ::aion::gameserver::skillengine::effect::AbstractHealEffect {
#include "aion/gameserver/skillengine/effect/ProcDPHealInstantEffect.xml.inc"
public:
	void applyEffect(model::Effect& effect) const override;

	int32_t getCurrentStatValue(model::Effect& effect) const override;

	int32_t getMaxStatValue(model::Effect& effect) const override;
};

} // namespace aion::gameserver::skillengine::effect
