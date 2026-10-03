#include "aion/gameserver/skillengine/effect/SpellAtkDrainEffect.h"

#include <cstdint>

#include "aion/gameserver/controllers/CreatureController.h"
#include "aion/gameserver/controllers/ObserveController.h"
#include "aion/gameserver/controllers/attack/AttackUtil.h"
#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/model/stats/container/CreatureLifeStats.h"
#include "aion/gameserver/network/aion/serverpackets/SM_ATTACK_STATUS_LOG.h"
#include "aion/gameserver/network/aion/serverpackets/SM_ATTACK_STATUS_TYPE.h"
#include "aion/gameserver/skillengine/model/Effect.h"
#include "aion/gameserver/skillengine/model/HopType.h"
#include "aion/gameserver/skillengine/model/SkillTemplate.h"

namespace aion::gameserver::skillengine::effect {

using network::aion::serverpackets::SM_ATTACK_STATUS_LOG;
using network::aion::serverpackets::SM_ATTACK_STATUS_TYPE;

namespace {

/** Java int a * b (wraps on overflow) */
constexpr int32_t mulInt(int32_t a, int32_t b) noexcept {
	return static_cast<int32_t>(static_cast<uint32_t>(a) * static_cast<uint32_t>(b));
}

} // namespace

void SpellAtkDrainEffect::resolveMagicalCritical(model::Effect& effect) const {
	effect.rollMagicalCritical(position, calculateCritProbMod(effect)); // periodic damage ignores the apply_magical_critical flag
}

void SpellAtkDrainEffect::onPeriodicAction(model::Effect& effect) const {
	int32_t valueWithDelta = calculateBaseValue(effect);
	// Java passes the int to the float parameter (a widening conversion)
	int32_t damage = controllers::attack::AttackUtil::calculateMagicalOverTimeSkillResult(effect, static_cast<float>(valueWithDelta), this,
		effect.getSkillTemplate()->isApplyMagicalSkillBoostBonus());
	// Java passes the nullable hopType field; the C++ overload takes a HopType (SpellAttackEffect.cpp's note): its only reader,
	// AggroList.addDamage, asks `notifyAttack && hopType == HopType.DAMAGE`, which SKILLLV answers as Java's null does
	effect.getEffected()->getController().onAttack(effect, SM_ATTACK_STATUS_TYPE::DAMAGE, damage, true, SM_ATTACK_STATUS_LOG::SPELLATKDRAIN,
		hopType.value_or(model::HopType::SKILLLV), effect.isMagicalCritical(position));
	effect.getEffector()->getObserveController()->notifyAttackObservers(*effect.getEffected(), effect.getSkillId());
	// Drain (heal) portion of damage inflicted
	if (hpPercent != 0) {
		effect.getEffector()->getLifeStats()->increaseHp(SM_ATTACK_STATUS_TYPE::HP, mulInt(damage, hpPercent) / 100, effect,
			SM_ATTACK_STATUS_LOG::SPELLATKDRAIN);
	}
	if (mpPercent != 0) {
		effect.getEffector()->getLifeStats()->increaseMp(SM_ATTACK_STATUS_TYPE::MP, mulInt(damage, mpPercent) / 100, effect.getSkillId(),
			SM_ATTACK_STATUS_LOG::SPELLATKDRAIN);
	}
}

} // namespace aion::gameserver::skillengine::effect
