#include "aion/gameserver/skillengine/effect/BleedEffect.h"

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

void BleedEffect::resolveMagicalCritical(model::Effect& effect) const {
	effect.rollMagicalCritical(position, calculateCritProbMod(effect)); // periodic damage ignores the apply_magical_critical flag
}

void BleedEffect::calculate(model::Effect& effect) const {
	AbstractOverTimeEffect::calculate(effect, gameserver::model::stats::container::StatEnum::BLEED_RESISTANCE, std::nullopt);
}

void BleedEffect::startEffect(model::Effect& effect) const {
	int32_t valueWithDelta = calculateBaseValue(effect);
	// Java passes the int to the float parameter (a widening conversion)
	int32_t finalDamage =
		controllers::attack::AttackUtil::calculateMagicalOverTimeSkillResult(effect, static_cast<float>(valueWithDelta), this, false);
	effect.setReserveds(*model::EffectReserved::create(position, finalDamage, model::EffectReserved::ResourceType::HP, true, false), true);
	AbstractOverTimeEffect::startEffect(effect, AbnormalState::BLEED);
}

void BleedEffect::endEffect(model::Effect& effect) const {
	AbstractOverTimeEffect::endEffect(effect, AbnormalState::BLEED);
}

void BleedEffect::onPeriodicAction(model::Effect& effect) const {
	runtime::Ptr<gameserver::model::gameobjects::Creature> effected = effect.getEffected();
	// Java passes the nullable hopType field on (37 of the 124 <bleed> templates have none). The Effect overloads of CreatureController.onAttack
	// take a non-nullable HopType (header request, docs/deviations/P5-03.md); their only reader, AggroList.addDamage, compares it with DAMAGE, so
	// SKILLLV answers exactly what Java's null does there.
	effected->getController().onAttack(effect, network::aion::serverpackets::SM_ATTACK_STATUS_TYPE::DAMAGE, effect.getReserveds(position)->getValue(),
		false, network::aion::serverpackets::SM_ATTACK_STATUS_LOG::BLEED, hopType.value_or(model::HopType::SKILLLV),
		effect.isMagicalCritical(position));
	effected->getObserveController()->notifyDotAttackedObservers(*effect.getEffector(), effect);
}

} // namespace aion::gameserver::skillengine::effect
