#include "aion/gameserver/skillengine/effect/OneTimeBoostSkillCriticalEffect.h"

#include <cstdint>

#include "aion/gameserver/controllers/attack/AttackStatus.h"
#include "aion/gameserver/controllers/observer/AttackerCriticalStatus.h"
#include "aion/gameserver/controllers/observer/AttackerCriticalStatusObserver.h"
#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/skillengine/model/Effect.h"

namespace aion::gameserver::skillengine::effect {

using controllers::attack::AttackStatus;
using controllers::observer::AttackerCriticalStatus;
using runtime::Ref;

/**
 * Java: the anonymous AttackerCriticalStatusObserver(AttackStatus.CRITICAL, count, value, percent) of OneTimeBoostSkillCriticalEffect.startEffect
 * (OneTimeBoostSkillCriticalEffect.java:32-47, fieldmap key OneTimeBoostSkillCriticalEffect$1): the effected's next `count` skill attacks are
 * critical; the last one ends the effect. Stored in the effected creature's ObserveController and the effect's observerRemoveTasks
 * (Effect.addObserver); Effect.endEffect -> removeObservers removes it. It captures the effect.
 */
struct OneTimeBoostSkillCriticalEffect_AttackerCriticalStatusObserver final : controllers::observer::AttackerCriticalStatusObserver {
	AION_MAKE_REF_FRIEND

	const Ref<model::Effect> effect; // captured final Effect effect

	static Ref<OneTimeBoostSkillCriticalEffect_AttackerCriticalStatusObserver> create(int32_t count, int32_t value, bool percent,
		model::Effect& effect) {
		return runtime::makeRef<OneTimeBoostSkillCriticalEffect_AttackerCriticalStatusObserver>(count, value, percent, effect);
	}

	Ref<AttackerCriticalStatus> checkAttackerCriticalStatus(AttackStatus stat, bool isSkill) override {
		if (stat == status.get() && isSkill) {
			if (getCount() <= 1)
				effect->endEffect();
			else
				decreaseCount();
			acStatus.get()->setResult(true);
		} else
			acStatus.get()->setResult(false);
		return acStatus.get();
	}

protected:
	OneTimeBoostSkillCriticalEffect_AttackerCriticalStatusObserver(int32_t countValue, int32_t valueValue, bool percentValue,
		model::Effect& effectValue)
		: AttackerCriticalStatusObserver(AttackStatus::CRITICAL, countValue, valueValue, percentValue), effect(Ref<model::Effect>(effectValue)) {}
	~OneTimeBoostSkillCriticalEffect_AttackerCriticalStatusObserver() override = default;
};

void OneTimeBoostSkillCriticalEffect::applyEffect(model::Effect& effect) const {
	effect.addToEffectedController();
}

// Anonymous class com.aionemu.gameserver.skillengine.effect.OneTimeBoostSkillCriticalEffect$1: the struct above
void OneTimeBoostSkillCriticalEffect::startEffect(model::Effect& effect) const {
	effect.addObserver(*effect.getEffected(), *OneTimeBoostSkillCriticalEffect_AttackerCriticalStatusObserver::create(count, value, percent, effect));
}

} // namespace aion::gameserver::skillengine::effect
