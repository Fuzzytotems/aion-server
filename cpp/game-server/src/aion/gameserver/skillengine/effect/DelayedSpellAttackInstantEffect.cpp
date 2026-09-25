#include "aion/gameserver/skillengine/effect/DelayedSpellAttackInstantEffect.h"

#include <cstdint>

#include "aion/gameserver/controllers/CreatureController.h"
#include "aion/gameserver/controllers/ObserveController.h"
#include "aion/gameserver/controllers/attack/AttackUtil.h"
#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/network/aion/serverpackets/SM_ATTACK_STATUS_LOG.h"
#include "aion/gameserver/network/aion/serverpackets/SM_ATTACK_STATUS_TYPE.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/skillengine/model/Effect.h"
#include "aion/gameserver/skillengine/model/EffectReserved.h"
#include "aion/gameserver/skillengine/model/HopType.h"
#include "aion/gameserver/utils/ThreadPoolManager.h"

namespace aion::gameserver::skillengine::effect {

// Stored lambda com.aionemu.gameserver.skillengine.effect.DelayedSpellAttackInstantEffect@L30:44 (the delayed hit, pin {this, &effect}): a task
// that runs once after `delay` and is then released with its pin; nothing keeps its Future
void DelayedSpellAttackInstantEffect::applyEffect(model::Effect& effect) const {
	int32_t valueWithDelta = calculateBaseValue(effect);

	controllers::attack::AttackUtil::calculateSkillResult(effect, valueWithDelta, this, true); // ignores shields on retail
	const int32_t finalPosition = this->position;
	utils::ThreadPoolManager::getInstance().schedule(runtime::Pin{this, &effect}, [this, &effect, finalPosition] {
		// Java passes the nullable hopType field on; the Effect overloads of CreatureController.onAttack take a non-nullable HopType, and their only
		// reader, AggroList.addDamage, compares it with DAMAGE, so SKILLLV answers exactly what Java's null does there (the DamageEffect.onAttack
		// row of docs/deviations/P5-03.md)
		effect.getEffected()->getController().onAttack(effect, network::aion::serverpackets::SM_ATTACK_STATUS_TYPE::DELAYDAMAGE,
			effect.getReserveds(finalPosition)->getValue(), true, network::aion::serverpackets::SM_ATTACK_STATUS_LOG::DELAYEDSPELLATKINSTANT,
			hopType.value_or(model::HopType::SKILLLV));
		effect.getEffector()->getObserveController()->notifyAttackObservers(*effect.getEffected(), effect.getSkillId());
	}, delay);
}

void DelayedSpellAttackInstantEffect::calculateDamage(model::Effect& /*effect*/) const {
}

} // namespace aion::gameserver::skillengine::effect
