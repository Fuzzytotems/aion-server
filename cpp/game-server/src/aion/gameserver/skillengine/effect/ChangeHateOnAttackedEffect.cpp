#include "aion/gameserver/skillengine/effect/ChangeHateOnAttackedEffect.h"

#include <cstdint>

#include "aion/gameserver/controllers/attack/AggroList.h"
#include "aion/gameserver/controllers/observer/ActionObserver.h"
#include "aion/gameserver/controllers/observer/ObserverType.h"
#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/model/gameobjects/Npc.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/skillengine/model/Effect.h"

namespace aion::gameserver::skillengine::effect {

using gameserver::model::gameobjects::Creature;
using runtime::Ref;

namespace {

/** Java int a + b (wraps on overflow) */
constexpr int32_t addInt(int32_t a, int32_t b) noexcept {
	return static_cast<int32_t>(static_cast<uint32_t>(a) + static_cast<uint32_t>(b));
}

} // namespace

/**
 * Java: the anonymous ActionObserver(ObserverType.ATTACKED) of ChangeHateOnAttackedEffect.startEffect (ChangeHateOnAttackedEffect.java:35-42,
 * fieldmap key ChangeHateOnAttackedEffect$1): an npc that attacks the effected adds finalValue hate to it. Stored in the effected creature's
 * ObserveController and the effect's observerRemoveTasks (Effect.addObserver); Effect.endEffect -> removeObservers removes it. It captures the
 * effect and the local finalValue.
 */
struct ChangeHateOnAttackedEffect_ActionObserver final : controllers::observer::ActionObserver {
	AION_MAKE_REF_FRIEND

	const Ref<model::Effect> effect; // captured final Effect effect
	const int32_t finalValue;        // captured local final int finalValue

	static Ref<ChangeHateOnAttackedEffect_ActionObserver> create(model::Effect& effect, int32_t finalValue) {
		return runtime::makeRef<ChangeHateOnAttackedEffect_ActionObserver>(effect, finalValue);
	}

	void attacked(Creature& creature, int32_t /*skillId*/) override {
		if (dynamic_cast<gameserver::model::gameobjects::Npc*>(&creature) != nullptr)
			creature.getAggroList().addHate(*effect->getEffected(), finalValue);
	}

protected:
	ChangeHateOnAttackedEffect_ActionObserver(model::Effect& effectValue, int32_t finalValueValue)
		: ActionObserver(controllers::observer::ObserverType::ATTACKED), effect(Ref<model::Effect>(effectValue)), finalValue(finalValueValue) {}
	~ChangeHateOnAttackedEffect_ActionObserver() override = default;
};

void ChangeHateOnAttackedEffect::applyEffect(model::Effect& effect) const {
	effect.addToEffectedController();
}

// Anonymous class com.aionemu.gameserver.skillengine.effect.ChangeHateOnAttackedEffect$1: the struct above
void ChangeHateOnAttackedEffect::startEffect(model::Effect& effect) const {
	// TODO: maybe this isn't correct formula?
	const int32_t finalValue = addInt(value1, value2);
	effect.addObserver(*effect.getEffected(), *ChangeHateOnAttackedEffect_ActionObserver::create(effect, finalValue));
}

} // namespace aion::gameserver::skillengine::effect
