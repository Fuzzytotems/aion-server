#pragma once

#include "aion/gameserver/skillengine/effect/ProcAtkInstantEffect.xml.h"

#include "aion/gameserver/skillengine/model/fwd.h"

namespace aion::gameserver::skillengine::effect {

/** Java com.aionemu.gameserver.skillengine.effect.ProcAtkInstantEffect. @author Wakizashi */
class ProcAtkInstantEffect : public ::aion::gameserver::skillengine::effect::DamageEffect {
#include "aion/gameserver/skillengine/effect/ProcAtkInstantEffect.xml.inc"
public:
	void applyEffect(model::Effect& effect) const override;

	bool shouldApplyAttackerMovementModifier() const override;

protected:
	int32_t calculateBaseValue(model::Effect& effect) const override;

public:
	bool shouldUseBoostSpellAttackEffects() const override;

	bool shouldUseOneTimeBoostSkillAttack() const override;
};

} // namespace aion::gameserver::skillengine::effect
