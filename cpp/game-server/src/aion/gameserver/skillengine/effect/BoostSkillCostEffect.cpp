#include "aion/gameserver/skillengine/effect/BoostSkillCostEffect.h"

#include "aion/gameserver/controllers/observer/ActionObserver.h"
#include "aion/gameserver/controllers/observer/ObserverType.h"
#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/skillengine/model/Effect.h"
#include "aion/gameserver/skillengine/model/Skill.h"

namespace aion::gameserver::skillengine::effect {

using runtime::Ref;

/**
 * Java: the anonymous ActionObserver(ObserverType.BOOSTSKILLCOST) of BoostSkillCostEffect.startEffect (BoostSkillCostEffect.java:28-34, fieldmap
 * key BoostSkillCostEffect$1): every skill of the effected gets the template's value as its boost of the skill cost. It captures only the
 * template (immutable static data: its protected value, through the friend line of BoostSkillCostEffect.h). Stored in the effected creature's
 * ObserveController and the effect's observerRemoveTasks (Effect.addObserver); Effect.endEffect -> removeObservers removes it.
 */
struct BoostSkillCostEffect_ActionObserver final : controllers::observer::ActionObserver {
	AION_MAKE_REF_FRIEND

	const BoostSkillCostEffect* boostSkillCostEffect; // captured this (immutable static data)

	static Ref<BoostSkillCostEffect_ActionObserver> create(const BoostSkillCostEffect& boostSkillCostEffect) {
		return runtime::makeRef<BoostSkillCostEffect_ActionObserver>(boostSkillCostEffect);
	}

	void boostSkillCost(model::Skill& skill) override { skill.setBoostSkillCost(boostSkillCostEffect->value); }

protected:
	explicit BoostSkillCostEffect_ActionObserver(const BoostSkillCostEffect& boostSkillCostEffectValue)
		: ActionObserver(controllers::observer::ObserverType::BOOSTSKILLCOST), boostSkillCostEffect(&boostSkillCostEffectValue) {}
	~BoostSkillCostEffect_ActionObserver() override = default;
};

// Anonymous class com.aionemu.gameserver.skillengine.effect.BoostSkillCostEffect$1: the struct above
void BoostSkillCostEffect::startEffect(model::Effect& effect) const {
	BufEffect::startEffect(effect);
	effect.addObserver(*effect.getEffected(), *BoostSkillCostEffect_ActionObserver::create(*this));
}

} // namespace aion::gameserver::skillengine::effect
