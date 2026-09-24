#include "aion/gameserver/skillengine/effect/ProcAtkInstantEffect.h"

#include <cstdint>

#include "aion/gameserver/controllers/CreatureController.h"
#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/network/aion/serverpackets/SM_ATTACK_STATUS_LOG.h"
#include "aion/gameserver/network/aion/serverpackets/SM_ATTACK_STATUS_TYPE.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/skillengine/model/Effect.h"
#include "aion/gameserver/skillengine/model/EffectReserved.h"
#include "aion/gameserver/skillengine/model/HopType.h"
#include "aion/gameserver/skillengine/model/SkillTemplate.h"

namespace aion::gameserver::skillengine::effect {

using network::aion::serverpackets::SM_ATTACK_STATUS_LOG;
using network::aion::serverpackets::SM_ATTACK_STATUS_TYPE;

void ProcAtkInstantEffect::applyEffect(model::Effect& effect) const {
	int32_t damage = effect.getReserveds(position)->getValue();
	// Java passes the nullable hopType field on (the godstone procs and the material skills carry none, e.g. 8267 and 8302). The Effect overloads
	// of CreatureController.onAttack take a non-nullable HopType (header request, docs/deviations/P5-03.md, DamageEffect.onAttack's row); their
	// only reader, AggroList.addDamage, compares it with DAMAGE, so SKILLLV answers exactly what Java's null does there.
	effect.getEffected()->getController().onAttack(effect, SM_ATTACK_STATUS_TYPE::DAMAGE, damage, true, SM_ATTACK_STATUS_LOG::PROCATKINSTANT,
		hopType.value_or(model::HopType::SKILLLV));
}

bool ProcAtkInstantEffect::shouldApplyAttackerMovementModifier() const {
	return false;
}

int32_t ProcAtkInstantEffect::calculateBaseValue(model::Effect& effect) const {
	if (delta == 1 && effect.getSkillTemplate()->isProvoked())
		return value;
	else
		return DamageEffect::calculateBaseValue(effect);
}

bool ProcAtkInstantEffect::shouldUseBoostSpellAttackEffects() const {
	return false;
}

bool ProcAtkInstantEffect::shouldUseOneTimeBoostSkillAttack() const {
	return false;
}

} // namespace aion::gameserver::skillengine::effect
