#include "aion/gameserver/skillengine/effect/AlwaysParryEffect.h"

#include <cstdint>

#include "aion/gameserver/controllers/attack/AttackStatus.h"
#include "aion/gameserver/controllers/observer/AttackStatusObserver.h"
#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/skillengine/model/Effect.h"

namespace aion::gameserver::skillengine::effect {

/**
 * Java: the anonymous AttackStatusObserver(value, AttackStatus.PARRY) of AlwaysParryEffect.startEffect (AlwaysParryEffect.java:25-36, fieldmap key
 * AlwaysParryEffect$1). `value` in its body is the observer's own inherited field (Java resolves an inherited member before the enclosing
 * template's field of the same name). Stored in the effected creature's ObserveController and, through the removal task of Effect.addObserver, in
 * Effect.observerRemoveTasks; removeObservers (from Effect.endEffect) removes it from both (cycles.toml "AlwaysParryEffect$1#effect": java-hook).
 */
struct AlwaysParryEffect_AttackStatusObserver final : controllers::observer::AttackStatusObserver {
	AION_MAKE_REF_FRIEND

	const runtime::Ref<model::Effect> effect; // captured param Effect effect (line 31)

	static runtime::Ref<AlwaysParryEffect_AttackStatusObserver> create(int32_t value, model::Effect& effect) {
		return runtime::makeRef<AlwaysParryEffect_AttackStatusObserver>(value, effect);
	}

	bool checkStatus(controllers::attack::AttackStatus statusValue) override {
		if (statusValue == controllers::attack::AttackStatus::PARRY) {
			// Java `--value` on the observer's int (wraps); java-race: a plain read-modify-write, like Java's
			value.set(static_cast<int32_t>(static_cast<uint32_t>(value.get()) - 1u));
			if (value.get() <= 0)
				effect->endEffect();
			return true;
		}
		return false;
	}

protected:
	AlwaysParryEffect_AttackStatusObserver(int32_t valueValue, model::Effect& effectValue)
		: AttackStatusObserver(valueValue, controllers::attack::AttackStatus::PARRY), effect(runtime::Ref<model::Effect>(effectValue)) {}
	~AlwaysParryEffect_AttackStatusObserver() override = default;
};

void AlwaysParryEffect::applyEffect(model::Effect& effect) const {
	effect.addToEffectedController();
}

// Anonymous class com.aionemu.gameserver.skillengine.effect.AlwaysParryEffect$1: the callback struct AlwaysParryEffect_AttackStatusObserver above
void AlwaysParryEffect::startEffect(model::Effect& effect) const {
	effect.addObserver(*effect.getEffected(), *AlwaysParryEffect_AttackStatusObserver::create(value, effect));
}

} // namespace aion::gameserver::skillengine::effect
