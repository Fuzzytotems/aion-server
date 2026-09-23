#pragma once

#include "aion/gameserver/skillengine/effect/DelayedSkillEffect.xml.h"
#include "aion/gameserver/skillengine/model/fwd.h"

namespace aion::gameserver::skillengine::effect {

/** Java com.aionemu.gameserver.skillengine.effect.DelayedSkillEffect. @author kecimis, Cheatkiller */
class DelayedSkillEffect : public ::aion::gameserver::skillengine::effect::EffectTemplate {
#include "aion/gameserver/skillengine/effect/DelayedSkillEffect.xml.inc"
public:
	void applyEffect(model::Effect& effect) const override;

	void endEffect(model::Effect& effect) const override;
};

} // namespace aion::gameserver::skillengine::effect
