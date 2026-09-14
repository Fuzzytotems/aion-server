#pragma once

#include "aion/gameserver/skillengine/effect/SummonHomingEffect.xml.h"
#include "aion/gameserver/skillengine/model/fwd.h"

namespace aion::gameserver::skillengine::effect {

/** Java com.aionemu.gameserver.skillengine.effect.SummonHomingEffect. @author ATracer */
class SummonHomingEffect : public ::aion::gameserver::skillengine::effect::SummonEffect {
#include "aion/gameserver/skillengine/effect/SummonHomingEffect.xml.inc"
public:
	void applyEffect(model::Effect& effect) const override;
};

} // namespace aion::gameserver::skillengine::effect
