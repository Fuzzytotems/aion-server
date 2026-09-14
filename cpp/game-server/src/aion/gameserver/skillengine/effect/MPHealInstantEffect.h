#pragma once

#include "aion/gameserver/skillengine/effect/MPHealInstantEffect.xml.h"
#include "aion/gameserver/skillengine/model/fwd.h"

namespace aion::gameserver::skillengine::effect {

/** Java com.aionemu.gameserver.skillengine.effect.MPHealInstantEffect. @author ATracer */
class MPHealInstantEffect : public ::aion::gameserver::skillengine::effect::AbstractHealEffect {
#include "aion/gameserver/skillengine/effect/MPHealInstantEffect.xml.inc"
public:
	void applyEffect(model::Effect& effect) const override;
};

} // namespace aion::gameserver::skillengine::effect
