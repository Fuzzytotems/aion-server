#pragma once

#include "aion/gameserver/skillengine/effect/BoostSkillCostEffect.xml.h"

#include "aion/gameserver/skillengine/model/fwd.h"

namespace aion::gameserver::skillengine::effect {

/** Java com.aionemu.gameserver.skillengine.effect.BoostSkillCostEffect. @author Rama and Sippolo */
class BoostSkillCostEffect : public ::aion::gameserver::skillengine::effect::BufEffect {
#include "aion/gameserver/skillengine/effect/BoostSkillCostEffect.xml.inc"
public:
	void startEffect(model::Effect& effect) const override;
};

} // namespace aion::gameserver::skillengine::effect
