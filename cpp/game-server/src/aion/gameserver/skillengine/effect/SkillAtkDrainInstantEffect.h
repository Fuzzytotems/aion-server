#pragma once

#include "aion/gameserver/skillengine/effect/SkillAtkDrainInstantEffect.xml.h"
#include "aion/gameserver/skillengine/model/fwd.h"

namespace aion::gameserver::skillengine::effect {

/** Java com.aionemu.gameserver.skillengine.effect.SkillAtkDrainInstantEffect. @author ATracer */
class SkillAtkDrainInstantEffect : public ::aion::gameserver::skillengine::effect::DamageEffect {
#include "aion/gameserver/skillengine/effect/SkillAtkDrainInstantEffect.xml.inc"
public:
	void applyEffect(model::Effect& effect) const override;
};

} // namespace aion::gameserver::skillengine::effect
