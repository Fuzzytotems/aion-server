#pragma once

#include "aion/gameserver/skillengine/effect/ProcMPHealInstantEffect.xml.h"
#include "aion/gameserver/skillengine/model/fwd.h"

namespace aion::gameserver::skillengine::effect {

/** Java com.aionemu.gameserver.skillengine.effect.ProcMPHealInstantEffect. @author ATracer */
class ProcMPHealInstantEffect : public ::aion::gameserver::skillengine::effect::AbstractHealEffect {
#include "aion/gameserver/skillengine/effect/ProcMPHealInstantEffect.xml.inc"
public:
	void applyEffect(model::Effect& effect) const override;
};

} // namespace aion::gameserver::skillengine::effect
