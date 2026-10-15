#include "aion/gameserver/skillengine/effect/ProtectEffect.h"

#include <optional>

#include "aion/gameserver/controllers/observer/ActionObserver.h"
#include "aion/gameserver/controllers/observer/AttackShieldObserver.h"
#include "aion/gameserver/controllers/observer/DeathObserver.h"
#include "aion/gameserver/controllers/observer/ObserverType.h"
#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/model/gameobjects/Summon.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/skillengine/model/Effect.h"

namespace aion::gameserver::skillengine::effect {

using controllers::observer::AttackShieldObserver;
using gameserver::model::gameobjects::Creature;
using runtime::Ref;

/**
 * Java: the anonymous ActionObserver(ObserverType.SUMMONRELEASE) of ProtectEffect.startEffect (ProtectEffect.java:32-37, fieldmap key
 * ProtectEffect$1): a protecting summon's release ends the effect. Stored in the effector's ObserveController and the effect's
 * observerRemoveTasks (Effect.addObserver); Effect.endEffect -> removeObservers removes it. It captures the effect.
 */
struct ProtectEffect_ActionObserver final : controllers::observer::ActionObserver {
	AION_MAKE_REF_FRIEND

	const Ref<model::Effect> effect; // captured final Effect effect

	static Ref<ProtectEffect_ActionObserver> create(model::Effect& effect) { return runtime::makeRef<ProtectEffect_ActionObserver>(effect); }

	void summonrelease() override { effect->endEffect(); }

protected:
	explicit ProtectEffect_ActionObserver(model::Effect& effectValue)
		: ActionObserver(controllers::observer::ObserverType::SUMMONRELEASE), effect(Ref<model::Effect>(effectValue)) {}
	~ProtectEffect_ActionObserver() override = default;
};

// Anonymous class com.aionemu.gameserver.skillengine.effect.ProtectEffect$1: the struct above
void ProtectEffect::startEffect(model::Effect& effect) const {
	Ref<AttackShieldObserver> asObserver =
		AttackShieldObserver::create(value, hitvalue, percent, false, effect, hitType, getType(), hitTypeProb, 0, radius, std::nullopt, 0);
	effect.addObserver(*effect.getEffected(), *asObserver);
	if (dynamic_cast<gameserver::model::gameobjects::Summon*>(&*effect.getEffector()) != nullptr) {
		effect.addObserver(*effect.getEffector(), *ProtectEffect_ActionObserver::create(effect));
	} else {
		// Java: lambda capturing effect (fieldmap callback ProtectEffect@L38:63), held by the DeathObserver until the observer is removed
		effect.addObserver(*effect.getEffector(), *controllers::observer::DeathObserver::create(
			runtime::PinnedCallback<void(Creature&)>(runtime::Pin{&effect}, [&effect](Creature&) { effect.endEffect(); })));
	}
}

void ProtectEffect::endEffect(model::Effect& /*effect*/) const {
	// Java: empty body
}

model::ShieldType ProtectEffect::getType() const {
	return model::ShieldType::PROTECT;
}

} // namespace aion::gameserver::skillengine::effect
