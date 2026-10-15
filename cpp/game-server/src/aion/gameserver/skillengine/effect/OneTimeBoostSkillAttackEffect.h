#pragma once

#include "aion/gameserver/skillengine/effect/OneTimeBoostSkillAttackEffect.xml.h"

#include "aion/gameserver/skillengine/model/fwd.h"

namespace aion::gameserver::skillengine::effect {

/** Java com.aionemu.gameserver.skillengine.effect.OneTimeBoostSkillAttackEffect. @author ATracer */
class OneTimeBoostSkillAttackEffect : public ::aion::gameserver::skillengine::effect::BufEffect {
#include "aion/gameserver/skillengine/effect/OneTimeBoostSkillAttackEffect.xml.inc"
public:
	void startEffect(model::Effect& effect) const override;

private:
	void removeEffect(model::Effect& effect) const;
};

} // namespace aion::gameserver::skillengine::effect
