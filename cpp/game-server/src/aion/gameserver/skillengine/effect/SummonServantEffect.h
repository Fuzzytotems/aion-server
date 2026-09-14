#pragma once

#include "aion/gameserver/skillengine/effect/SummonServantEffect.xml.h"
#include "aion/gameserver/skillengine/model/fwd.h"

namespace aion::gameserver::skillengine::effect {

/** Java com.aionemu.gameserver.skillengine.effect.SummonServantEffect. @author ATracer */
class SummonServantEffect : public ::aion::gameserver::skillengine::effect::SummonEffect {
#include "aion/gameserver/skillengine/effect/SummonServantEffect.xml.inc"
public:
	void applyEffect(model::Effect& effect) const override;
};

} // namespace aion::gameserver::skillengine::effect
