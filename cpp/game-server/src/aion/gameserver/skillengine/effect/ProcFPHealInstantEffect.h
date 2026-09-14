#pragma once

#include "aion/gameserver/skillengine/effect/ProcFPHealInstantEffect.xml.h"
#include "aion/gameserver/skillengine/model/fwd.h"

namespace aion::gameserver::skillengine::effect {

/** Java com.aionemu.gameserver.skillengine.effect.ProcFPHealInstantEffect. @author ATracer */
class ProcFPHealInstantEffect : public ::aion::gameserver::skillengine::effect::AbstractHealEffect {
#include "aion/gameserver/skillengine/effect/ProcFPHealInstantEffect.xml.inc"
public:
	void applyEffect(model::Effect& effect) const override;
};

} // namespace aion::gameserver::skillengine::effect
