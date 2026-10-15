#include "aion/gameserver/skillengine/effect/CaseHealEffect.h"

#include <cstdint>
#include <optional>

#include "aion/gameserver/controllers/observer/ActionObserver.h"
#include "aion/gameserver/controllers/observer/ObserverType.h"
#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/model/stats/calc/Stat2.h"
#include "aion/gameserver/model/stats/container/CreatureGameStats.h"
#include "aion/gameserver/model/stats/container/CreatureLifeStats.h"
#include "aion/gameserver/network/aion/serverpackets/SM_ATTACK_STATUS_LOG.h"
#include "aion/gameserver/network/aion/serverpackets/SM_ATTACK_STATUS_TYPE.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/skillengine/model/Effect.h"
#include "aion/gameserver/skillengine/model/HealType.h"

namespace aion::gameserver::skillengine::effect {

using network::aion::serverpackets::SM_ATTACK_STATUS_LOG;
using network::aion::serverpackets::SM_ATTACK_STATUS_TYPE;
using runtime::Ref;

/**
 * Java: the anonymous ActionObserver(ObserverType.HP_CHANGED) of CaseHealEffect.startEffect (CaseHealEffect.java:56-61, fieldmap key
 * CaseHealEffect$1), added when the first try did not heal: every HP change of the effected tries again. Stored in the effected creature's
 * ObserveController and, through the removal task of Effect.addObserver, in the effect's observerRemoveTasks; Effect.endEffect -> removeObservers
 * removes it from both. It captures the template (immutable static data) and the effect.
 */
struct CaseHealEffect_ActionObserver final : controllers::observer::ActionObserver {
	AION_MAKE_REF_FRIEND

	const CaseHealEffect* caseHealEffect; // captured this (immutable static data)
	const Ref<model::Effect> effect;      // captured final Effect effect

	static Ref<CaseHealEffect_ActionObserver> create(const CaseHealEffect& caseHealEffect, model::Effect& effect) {
		return runtime::makeRef<CaseHealEffect_ActionObserver>(caseHealEffect, effect);
	}

	void hpChanged(int32_t /*value*/) override { caseHealEffect->tryHeal(*effect); }

protected:
	CaseHealEffect_ActionObserver(const CaseHealEffect& caseHealEffectValue, model::Effect& effectValue)
		: ActionObserver(controllers::observer::ObserverType::HP_CHANGED), caseHealEffect(&caseHealEffectValue), effect(Ref<model::Effect>(effectValue)) {}
	~CaseHealEffect_ActionObserver() override = default;
};

namespace {

/** Java `switch (type)` on a null HealType throws NullPointerException; the data always sets it */
model::HealType switchedOn(const std::optional<model::HealType>& type) {
	if (!type)
		throw runtime::NullPointerException("CaseHealEffect.type is null");
	return *type;
}

} // namespace

int32_t CaseHealEffect::getCurrentStatValue(model::Effect& effect) const {
	switch (switchedOn(type)) {
		case model::HealType::HP:
			return effect.getEffected()->getLifeStats()->getCurrentHp();
		case model::HealType::MP:
			return effect.getEffected()->getLifeStats()->getCurrentMp();
		default:
			return 0;
	}
}

int32_t CaseHealEffect::getMaxStatValue(model::Effect& effect) const {
	switch (switchedOn(type)) {
		case model::HealType::HP:
			return effect.getEffected()->getGameStats()->getMaxHp()->getCurrent();
		case model::HealType::MP:
			return effect.getEffected()->getGameStats()->getMaxMp()->getCurrent();
		default:
			return 0;
	}
}

void CaseHealEffect::applyEffect(model::Effect& effect) const {
	effect.addToEffectedController();
}

// Anonymous class com.aionemu.gameserver.skillengine.effect.CaseHealEffect$1: the struct above
void CaseHealEffect::startEffect(model::Effect& effect) const {
	if (tryHeal(effect))
		return;
	effect.addObserver(*effect.getEffected(), *CaseHealEffect_ActionObserver::create(*this, effect));
}

bool CaseHealEffect::tryHeal(model::Effect& effect) const {
	const int32_t currentValue = getCurrentStatValue(effect);
	const int32_t maxCurValue = getMaxStatValue(effect);
	// only heal if the current value is at or below the given percentage
	if (static_cast<float>(currentValue) <= (static_cast<float>(maxCurValue * condValue) / 100.0f)) {
		if (type == model::HealType::HP)
			effect.getEffected()->getLifeStats()->increaseHp(SM_ATTACK_STATUS_TYPE::HP, calculateHealValue(effect, *type), effect,
				SM_ATTACK_STATUS_LOG::CASEHEAL);
		else if (type == model::HealType::MP)
			effect.getEffected()->getLifeStats()->increaseMp(SM_ATTACK_STATUS_TYPE::MP, calculateHealValue(effect, *type), effect.getSkillId(),
				SM_ATTACK_STATUS_LOG::CASEHEAL);
		effect.endEffect();
		return true;
	}
	return false;
}

bool CaseHealEffect::allowHpHealBoost(model::Effect& /*effect*/) const {
	return false;
}

bool CaseHealEffect::allowHpHealSkillDeboost(model::Effect& /*effect*/) const {
	return false;
}

} // namespace aion::gameserver::skillengine::effect
