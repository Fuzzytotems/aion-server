#pragma once

#include "aion/gameserver/skillengine/effect/NoReduceSpellATKInstantEffect.xml.h"

#include "aion/gameserver/skillengine/model/fwd.h"

namespace aion::gameserver::skillengine::effect {

/** Java com.aionemu.gameserver.skillengine.effect.NoReduceSpellATKInstantEffect. @author Sippolo */
class NoReduceSpellATKInstantEffect : public ::aion::gameserver::skillengine::effect::DamageEffect {
#include "aion/gameserver/skillengine/effect/NoReduceSpellATKInstantEffect.xml.inc"
public:
	bool shouldApplyAttackerMovementModifier() const override;

	bool shouldApplyMagicalSkillBoostBonus(model::Effect& effect) const override;

	bool shouldUseKnowledge() const override;

	bool shouldUseBoostSpellAttackEffects() const override;

	bool shouldUseOneTimeBoostSkillAttack() const override;
};

} // namespace aion::gameserver::skillengine::effect
