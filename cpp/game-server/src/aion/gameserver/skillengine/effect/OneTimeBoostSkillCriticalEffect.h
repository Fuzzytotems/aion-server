#pragma once

#include "aion/gameserver/skillengine/effect/OneTimeBoostSkillCriticalEffect.xml.h"
#include "aion/gameserver/skillengine/model/fwd.h"

namespace aion::gameserver::skillengine::effect {

/** Java com.aionemu.gameserver.skillengine.effect.OneTimeBoostSkillCriticalEffect. @author Sippolo */
class OneTimeBoostSkillCriticalEffect : public ::aion::gameserver::skillengine::effect::EffectTemplate {
#include "aion/gameserver/skillengine/effect/OneTimeBoostSkillCriticalEffect.xml.inc"
public:
	void applyEffect(model::Effect& effect) const override;

	void startEffect(model::Effect& effect) const override;
};

} // namespace aion::gameserver::skillengine::effect
