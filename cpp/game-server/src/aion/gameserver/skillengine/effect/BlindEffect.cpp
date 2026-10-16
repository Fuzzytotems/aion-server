#include "aion/gameserver/skillengine/effect/BlindEffect.h"

#include <cstdint>
#include <optional>

#include "aion/commons/utils/Rnd.h"
#include "aion/gameserver/controllers/attack/AttackStatus.h"
#include "aion/gameserver/controllers/effect/EffectController.h"
#include "aion/gameserver/controllers/observer/AttackStatusObserver.h"
#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/model/gameobjects/state/CreatureVisualState.h"
#include "aion/gameserver/model/gameobjects/state/CreatureVisualStateInfo.h"
#include "aion/gameserver/model/stats/container/StatEnum.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/skillengine/effect/AbnormalState.h"
#include "aion/gameserver/skillengine/model/Effect.h"

namespace aion::gameserver::skillengine::effect {

using gameserver::model::gameobjects::state::CreatureVisualState;

/**
 * Java: the anonymous AttackStatusObserver(value, AttackStatus.DODGE) of BlindEffect.startEffect (BlindEffect.java:38-45, fieldmap key
 * BlindEffect$1, no captures). `value` in its body is the observer's own inherited field, which the constructor set from the template's value
 * (Java resolves an inherited member before the enclosing template's field of the same name; the AlwaysDodgeEffect$1 precedent). The attacker
 * side of the observer: StatFunctions.checkIsDodgedHit asks the blinded attacker's ObserveController.checkAttackerStatus(DODGE), so each of its
 * physical attacks is dodged with `value` percent. Stored in the effected creature's ObserveController and, through the removal task of
 * Effect.addObserver, in Effect.observerRemoveTasks; removeObservers (from Effect.endEffect) removes it from both.
 */
struct BlindEffect_AttackStatusObserver final : controllers::observer::AttackStatusObserver {
	AION_MAKE_REF_FRIEND

	static runtime::Ref<BlindEffect_AttackStatusObserver> create(int32_t value) {
		return runtime::makeRef<BlindEffect_AttackStatusObserver>(value);
	}

	bool checkAttackerStatus(controllers::attack::AttackStatus /*status*/) override {
		// Java float < int: the int operand is converted to float
		return commons::utils::Rnd::chance() < static_cast<float>(value.get());
	}

protected:
	explicit BlindEffect_AttackStatusObserver(int32_t valueValue)
		: AttackStatusObserver(valueValue, controllers::attack::AttackStatus::DODGE) {}
	~BlindEffect_AttackStatusObserver() override = default;
};

void BlindEffect::applyEffect(model::Effect& effect) const {
	int32_t visualStateExcludingBlinking = effect.getEffected()->getVisualState() & ~getId(CreatureVisualState::BLINKING);
	if (visualStateExcludingBlinking < getId(CreatureVisualState::HIDE10))
		effect.getEffected()->getEffectController()->removeHideEffects();
	effect.addToEffectedController();
}

void BlindEffect::calculate(model::Effect& effect) const {
	EffectTemplate::calculate(effect, gameserver::model::stats::container::StatEnum::BLIND_RESISTANCE, std::nullopt);
}

// Anonymous class com.aionemu.gameserver.skillengine.effect.BlindEffect$1: the callback struct BlindEffect_AttackStatusObserver above
void BlindEffect::startEffect(model::Effect& effect) const {
	effect.setAbnormal(AbnormalState::BLIND);
	effect.getEffected()->getEffectController()->setAbnormal(AbnormalState::BLIND);
	effect.addObserver(*effect.getEffected(), *BlindEffect_AttackStatusObserver::create(value));
}

void BlindEffect::endEffect(model::Effect& effect) const {
	effect.getEffected()->getEffectController()->unsetAbnormal(AbnormalState::BLIND);
}

} // namespace aion::gameserver::skillengine::effect
