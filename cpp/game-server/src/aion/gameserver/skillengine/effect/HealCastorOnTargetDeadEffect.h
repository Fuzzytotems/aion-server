#pragma once

#include "aion/gameserver/skillengine/effect/HealCastorOnTargetDeadEffect.xml.h"
#include "aion/gameserver/skillengine/model/fwd.h"

namespace aion::gameserver::skillengine::effect {

/** Java com.aionemu.gameserver.skillengine.effect.HealCastorOnTargetDeadEffect. @author Sippolo */
class HealCastorOnTargetDeadEffect : public ::aion::gameserver::skillengine::effect::EffectTemplate {
#include "aion/gameserver/skillengine/effect/HealCastorOnTargetDeadEffect.xml.inc"
public:
	void applyEffect(model::Effect& effect) const override;

	void endEffect(model::Effect& effect) const override;
};

} // namespace aion::gameserver::skillengine::effect
