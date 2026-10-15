#include "aion/gameserver/skillengine/effect/PoisonEffect.h"

#include <cstdint>
#include <optional>

#include "aion/gameserver/controllers/CreatureController.h"
#include "aion/gameserver/controllers/ObserveController.h"
#include "aion/gameserver/controllers/attack/AttackUtil.h"
#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/model/stats/container/StatEnum.h"
#include "aion/gameserver/network/aion/serverpackets/SM_ATTACK_STATUS_LOG.h"
#include "aion/gameserver/network/aion/serverpackets/SM_ATTACK_STATUS_TYPE.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/skillengine/effect/AbnormalState.h"
#include "aion/gameserver/skillengine/model/Effect.h"
#include "aion/gameserver/skillengine/model/EffectReserved.h"
#include "aion/gameserver/skillengine/model/HopType.h"

namespace aion::gameserver::skillengine::effect {

void PoisonEffect::resolveMagicalCritical(model::Effect& effect) const {
	effect.rollMagicalCritical(position, calculateCritProbMod(effect)); // periodic damage ignores the apply_magical_critical flag
}

void PoisonEffect::calculate(model::Effect& effect) const {
	AbstractOverTimeEffect::calculate(effect, gameserver::model::stats::container::StatEnum::POISON_RESISTANCE, std::nullopt);
}

void PoisonEffect::startEffect(model::Effect& effect) const {
	int32_t valueWithDelta = calculateBaseValue(effect);
	// Java passes the int to the float parameter (a widening conversion)
	int32_t finalDamage =
		controllers::attack::AttackUtil::calculateMagicalOverTimeSkillResult(effect, static_cast<float>(valueWithDelta), this, false);
	effect.setReserveds(*model::EffectReserved::create(position, finalDamage, model::EffectReserved::ResourceType::HP, true, false), true);
	AbstractOverTimeEffect::startEffect(effect, AbnormalState::POISON);
}

void PoisonEffect::endEffect(model::Effect& effect) const {
	AbstractOverTimeEffect::endEffect(effect, AbnormalState::POISON);
}

void PoisonEffect::onPeriodicAction(model::Effect& effect) const {
	runtime::Ptr<gameserver::model::gameobjects::Creature> effected = effect.getEffected();
	// Java passes the nullable hopType field on (none of the six godstone poisons carries one). The Effect overloads of CreatureController.onAttack
	// take a non-nullable HopType (header request, docs/deviations/P5-03.md, BleedEffect.onPeriodicAction's row); their only reader,
	// AggroList.addDamage, compares it with DAMAGE behind notifyAttack, which a tick passes false, so SKILLLV answers what Java's null does.
	effected->getController().onAttack(effect, network::aion::serverpackets::SM_ATTACK_STATUS_TYPE::DAMAGE, effect.getReserveds(position)->getValue(),
		false, network::aion::serverpackets::SM_ATTACK_STATUS_LOG::POISON, hopType.value_or(model::HopType::SKILLLV),
		effect.isMagicalCritical(position));
	effected->getObserveController()->notifyDotAttackedObservers(*effect.getEffector(), effect);
}

} // namespace aion::gameserver::skillengine::effect
