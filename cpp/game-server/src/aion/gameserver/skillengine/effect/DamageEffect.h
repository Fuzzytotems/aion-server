#pragma once

#include "aion/gameserver/skillengine/effect/DamageEffect.xml.h"

#include "aion/gameserver/network/aion/serverpackets/SM_ATTACK_STATUS_LOG.h"
#include "aion/gameserver/network/aion/serverpackets/SM_ATTACK_STATUS_TYPE.h"
#include "aion/gameserver/skillengine/model/fwd.h"

namespace aion::gameserver::skillengine::effect {

/** Java com.aionemu.gameserver.skillengine.effect.DamageEffect. @author ATracer */
class DamageEffect : public ::aion::gameserver::skillengine::effect::EffectTemplate {
#include "aion/gameserver/skillengine/effect/DamageEffect.xml.inc"
public:
	void applyEffect(model::Effect& effect) const override;

private:
	void onAttack(model::Effect& effect, network::aion::serverpackets::SM_ATTACK_STATUS_TYPE type,
		network::aion::serverpackets::SM_ATTACK_STATUS_LOG log) const;

protected:
	void resolveMagicalCritical(model::Effect& effect) const override;

public:
	void calculateDamage(model::Effect& effect) const override;

	/**
	 * Determines whether movement-based modifiers (the attacker standing, moving forward or backward) should be applied to this damage effect
	 * during damage calculation. Specific implementations may override it to exclude themselves.
	 */
	virtual bool shouldApplyAttackerMovementModifier() const;

	virtual bool shouldApplyMagicalSkillBoostBonus(model::Effect& effect) const;

	virtual bool shouldUseKnowledge() const;

	virtual bool shouldUseBoostSpellAttackEffects() const;

	virtual bool shouldUseOneTimeBoostSkillAttack() const;
};

} // namespace aion::gameserver::skillengine::effect
