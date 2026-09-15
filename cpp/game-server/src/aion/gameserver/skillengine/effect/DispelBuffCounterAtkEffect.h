#pragma once

#include "aion/gameserver/skillengine/effect/DispelBuffCounterAtkEffect.xml.h"

#include "aion/gameserver/skillengine/model/fwd.h"

namespace aion::gameserver::skillengine::effect {

/** Java com.aionemu.gameserver.skillengine.effect.DispelBuffCounterAtkEffect. */
class DispelBuffCounterAtkEffect : public ::aion::gameserver::skillengine::effect::DamageEffect {
#include "aion/gameserver/skillengine/effect/DispelBuffCounterAtkEffect.xml.inc"
public:
	void applyEffect(model::Effect& effect) const override;

	bool shouldApplyAttackerMovementModifier() const override;

	bool shouldUseKnowledge() const override;

	bool shouldUseBoostSpellAttackEffects() const override;

	bool shouldUseOneTimeBoostSkillAttack() const override;
};

} // namespace aion::gameserver::skillengine::effect
