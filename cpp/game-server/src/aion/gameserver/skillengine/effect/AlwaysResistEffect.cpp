#include "aion/gameserver/skillengine/effect/AlwaysResistEffect.h"

#include <cstdint>

#include "aion/gameserver/controllers/attack/AttackStatus.h"
#include "aion/gameserver/controllers/observer/AttackStatusObserver.h"
#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/skillengine/model/Effect.h"

namespace aion::gameserver::skillengine::effect {

/**
 * Java: the anonymous AttackStatusObserver(value, AttackStatus.RESIST) of AlwaysResistEffect.startEffect (AlwaysResistEffect.java:25-37, fieldmap
 * key AlwaysResistEffect$1). `value` in its body is the observer's own inherited field (Java resolves an inherited member before the enclosing
 * template's field of the same name). Stored in the effected creature's ObserveController and, through the removal task of Effect.addObserver, in
 * Effect.observerRemoveTasks; removeObservers (from Effect.endEffect) removes it from both (cycles.toml "AlwaysResistEffect$1#effect": java-hook).
 */
struct AlwaysResistEffect_AttackStatusObserver final : controllers::observer::AttackStatusObserver {
	AION_MAKE_REF_FRIEND

	const runtime::Ref<model::Effect> effect; // captured param Effect effect (line 31)

	static runtime::Ref<AlwaysResistEffect_AttackStatusObserver> create(int32_t value, model::Effect& effect) {
		return runtime::makeRef<AlwaysResistEffect_AttackStatusObserver>(value, effect);
	}

	bool checkStatus(controllers::attack::AttackStatus statusValue) override {
		if (statusValue == controllers::attack::AttackStatus::RESIST) {
			// Java `--value` on the observer's int (wraps); java-race: a plain read-modify-write, like Java's
			value.set(static_cast<int32_t>(static_cast<uint32_t>(value.get()) - 1u));
			if (value.get() <= 0)
				effect->endEffect();
			return true;
		}
		return false;
	}

protected:
	AlwaysResistEffect_AttackStatusObserver(int32_t valueValue, model::Effect& effectValue)
		: AttackStatusObserver(valueValue, controllers::attack::AttackStatus::RESIST), effect(runtime::Ref<model::Effect>(effectValue)) {}
	~AlwaysResistEffect_AttackStatusObserver() override = default;
};

void AlwaysResistEffect::applyEffect(model::Effect& effect) const {
	effect.addToEffectedController();
}

// Anonymous class com.aionemu.gameserver.skillengine.effect.AlwaysResistEffect$1: the callback struct AlwaysResistEffect_AttackStatusObserver above
void AlwaysResistEffect::startEffect(model::Effect& effect) const {
	effect.addObserver(*effect.getEffected(), *AlwaysResistEffect_AttackStatusObserver::create(value, effect));
}

} // namespace aion::gameserver::skillengine::effect
