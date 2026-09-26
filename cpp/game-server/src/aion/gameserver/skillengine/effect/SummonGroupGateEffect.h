#pragma once

#include "aion/gameserver/skillengine/effect/SummonGroupGateEffect.xml.h"
#include "aion/gameserver/skillengine/model/fwd.h"

namespace aion::gameserver::skillengine::effect {

/** Java com.aionemu.gameserver.skillengine.effect.SummonGroupGateEffect. @author LokiReborn, Neon */
class SummonGroupGateEffect : public ::aion::gameserver::skillengine::effect::SummonEffect {
#include "aion/gameserver/skillengine/effect/SummonGroupGateEffect.xml.inc"
public:
	void applyEffect(model::Effect& effect) const override;
};

} // namespace aion::gameserver::skillengine::effect
