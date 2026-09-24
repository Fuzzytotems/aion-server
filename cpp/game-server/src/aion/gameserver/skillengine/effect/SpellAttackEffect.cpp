#include "aion/gameserver/skillengine/effect/SpellAttackEffect.h"

#include <cstdint>
#include <string>

#include "aion/gameserver/controllers/CreatureController.h"
#include "aion/gameserver/controllers/ObserveController.h"
#include "aion/gameserver/controllers/attack/AttackUtil.h"
#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/network/aion/serverpackets/SM_ATTACK_STATUS_LOG.h"
#include "aion/gameserver/network/aion/serverpackets/SM_ATTACK_STATUS_TYPE.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/skillengine/model/Effect.h"
#include "aion/gameserver/skillengine/model/EffectReserved.h"
#include "aion/gameserver/skillengine/model/HopType.h"
#include "aion/gameserver/skillengine/model/SkillTemplate.h"

namespace aion::gameserver::skillengine::effect {

using gameserver::model::gameobjects::Creature;
using network::aion::serverpackets::SM_ATTACK_STATUS_LOG;
using network::aion::serverpackets::SM_ATTACK_STATUS_TYPE;
using runtime::Ptr;
using runtime::Ref;

namespace {

/** Java's implicit null check of a dereference of a static template pointer (a plain C++ dereference of nullptr is undefined) */
template <class T>
const T& nonNull(const T* value, const char* what) {
	if (value == nullptr)
		throw runtime::NullPointerException(std::string(what) + " is null");
	return *value;
}

} // namespace

void SpellAttackEffect::resolveMagicalCritical(model::Effect& effect) const {
	effect.rollMagicalCritical(position, calculateCritProbMod(effect)); // periodic damage ignores the apply_magical_critical flag
}

void SpellAttackEffect::startEffect(model::Effect& effect) const {
	int32_t valueWithDelta = calculateBaseValue(effect);
	int32_t finalDamage = controllers::attack::AttackUtil::calculateMagicalOverTimeSkillResult(effect, static_cast<float>(valueWithDelta), this,
		nonNull(effect.getSkillTemplate(), "skillTemplate").isApplyMagicalSkillBoostBonus());
	Ref<model::EffectReserved> reserved = model::EffectReserved::create(position, finalDamage, model::EffectReserved::ResourceType::HP, true, false);
	effect.setReserveds(*reserved, true);
	AbstractOverTimeEffect::startEffect(effect);
}

void SpellAttackEffect::onPeriodicAction(model::Effect& effect) const {
	Ptr<Creature> effected = effect.getEffected();
	// Java passes the nullable hopType field (260 of the 601 <spellatk> of skill_templates.xml have no hoptype). The C++ overload takes a
	// HopType (header request, docs/deviations/P5-04.md); with notifyAttack false the private onAttack hands it only to AggroList.addDamage,
	// which reads it behind `notifyAttack && hopType == HopType.DAMAGE` (AggroList.java:47), so a null field passes SKILLLV, which is the same
	// "not DAMAGE" there.
	effected->getController().onAttack(effect, SM_ATTACK_STATUS_TYPE::DAMAGE, effect.getReserveds(position)->getValue(), false,
		SM_ATTACK_STATUS_LOG::SPELLATK, hopType.value_or(model::HopType::SKILLLV), effect.isMagicalCritical(position));
	effected->getObserveController()->notifyDotAttackedObservers(*effect.getEffector(), effect);
}

} // namespace aion::gameserver::skillengine::effect
