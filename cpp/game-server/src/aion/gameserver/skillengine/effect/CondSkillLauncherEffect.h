#pragma once

#include "aion/gameserver/skillengine/effect/CondSkillLauncherEffect.xml.h"
#include "aion/gameserver/skillengine/model/fwd.h"

namespace aion::gameserver::skillengine::effect {

/** Java com.aionemu.gameserver.skillengine.effect.CondSkillLauncherEffect. @author Sippolo */
class CondSkillLauncherEffect : public ::aion::gameserver::skillengine::effect::EffectTemplate {
#include "aion/gameserver/skillengine/effect/CondSkillLauncherEffect.xml.inc"
public:
	void applyEffect(model::Effect& effect) const override;

	void startEffect(model::Effect& effect) const override;
};

} // namespace aion::gameserver::skillengine::effect
