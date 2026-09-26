#pragma once

#include "aion/gameserver/skillengine/effect/DelayedSpellAttackInstantEffect.xml.h"
#include "aion/gameserver/skillengine/model/fwd.h"

namespace aion::gameserver::skillengine::effect {

/** Java com.aionemu.gameserver.skillengine.effect.DelayedSpellAttackInstantEffect. @author ATracer */
class DelayedSpellAttackInstantEffect : public ::aion::gameserver::skillengine::effect::DamageEffect {
#include "aion/gameserver/skillengine/effect/DelayedSpellAttackInstantEffect.xml.inc"
public:
	void applyEffect(model::Effect& effect) const override;

	void calculateDamage(model::Effect& effect) const override;
};

} // namespace aion::gameserver::skillengine::effect
