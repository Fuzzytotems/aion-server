#pragma once

#include "aion/gameserver/skillengine/effect/SummonSkillAreaEffect.xml.h"
#include "aion/gameserver/skillengine/model/fwd.h"

namespace aion::gameserver::skillengine::effect {

/** Java com.aionemu.gameserver.skillengine.effect.SummonSkillAreaEffect. @author ATracer */
class SummonSkillAreaEffect : public ::aion::gameserver::skillengine::effect::SummonServantEffect {
#include "aion/gameserver/skillengine/effect/SummonSkillAreaEffect.xml.inc"
public:
	void applyEffect(model::Effect& effect) const override;
};

} // namespace aion::gameserver::skillengine::effect
