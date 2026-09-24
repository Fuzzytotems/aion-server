#include "aion/gameserver/skillengine/effect/DamageEffect.h"

#include <cstdint>

#include "aion/gameserver/controllers/CreatureController.h"
#include "aion/gameserver/controllers/ObserveController.h"
#include "aion/gameserver/controllers/attack/AttackUtil.h"
#include "aion/gameserver/model/SkillElement.h"
#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/skillengine/model/ActivationAttribute.h"
#include "aion/gameserver/skillengine/model/Effect.h"
#include "aion/gameserver/skillengine/model/EffectReserved.h"
#include "aion/gameserver/skillengine/model/HopType.h"
#include "aion/gameserver/skillengine/model/SkillTemplate.h"

namespace aion::gameserver::skillengine::effect {

using network::aion::serverpackets::SM_ATTACK_STATUS_LOG;
using network::aion::serverpackets::SM_ATTACK_STATUS_TYPE;

void DamageEffect::applyEffect(model::Effect& effect) const {
	if (effect.getSkillTemplate()->getActivationAttribute() == model::ActivationAttribute::PROVOKED) {
		onAttack(effect, SM_ATTACK_STATUS_TYPE::DAMAGE, SM_ATTACK_STATUS_LOG::PROCATKINSTANT);
	} else {
		onAttack(effect, SM_ATTACK_STATUS_TYPE::REGULAR, SM_ATTACK_STATUS_LOG::REGULAR);
		effect.getEffector()->getObserveController()->notifyAttackObservers(*effect.getEffected(), effect.getSkillId());
	}
}

void DamageEffect::onAttack(model::Effect& effect, network::aion::serverpackets::SM_ATTACK_STATUS_TYPE type,
	network::aion::serverpackets::SM_ATTACK_STATUS_LOG log) const {
	// Java passes the nullable hopType field on (149 of the 2,413 <skillatk> and 255 of the 3,278 <spellatkinstant> templates have none). The Effect
	// overloads of CreatureController.onAttack take a non-nullable HopType (header request, docs/deviations/P5-03.md); their only reader,
	// AggroList.addDamage, compares it with DAMAGE, so SKILLLV answers exactly what Java's null does there.
	effect.getEffected()->getController().onAttack(effect, type, effect.getReserveds(this->position)->getValue(), true, log,
		hopType.value_or(model::HopType::SKILLLV));
}

void DamageEffect::resolveMagicalCritical(model::Effect& effect) const {
	if (element != gameserver::model::SkillElement::NONE && effect.getSkillTemplate()->isApplyMagicalCritical())
		effect.rollMagicalCritical(position, calculateCritProbMod(effect));
}

void DamageEffect::calculateDamage(model::Effect& effect) const {
	int32_t valueWithDelta = calculateBaseValue(effect);
	controllers::attack::AttackUtil::calculateSkillResult(effect, valueWithDelta, this, false);
}

bool DamageEffect::shouldApplyAttackerMovementModifier() const {
	return true;
}

bool DamageEffect::shouldApplyMagicalSkillBoostBonus(model::Effect& effect) const {
	return effect.getSkillTemplate()->isApplyMagicalSkillBoostBonus();
}

bool DamageEffect::shouldUseKnowledge() const {
	return true;
}

bool DamageEffect::shouldUseBoostSpellAttackEffects() const {
	return true;
}

bool DamageEffect::shouldUseOneTimeBoostSkillAttack() const {
	return true;
}

} // namespace aion::gameserver::skillengine::effect
