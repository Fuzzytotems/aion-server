#include "aion/gameserver/skillengine/effect/HostileUpEffect.h"

#include <cstdint>
#include <optional>

#include "aion/gameserver/controllers/ObserveController.h"
#include "aion/gameserver/controllers/attack/AggroList.h"
#include "aion/gameserver/controllers/observer/DeathObserver.h"
#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/model/gameobjects/Npc.h"
#include "aion/gameserver/runtime/collections/Rc.h"
#include "aion/gameserver/runtime/fields/Atomic.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/sched/Future.h"
#include "aion/gameserver/runtime/sched/PinnedCallback.h"
#include "aion/gameserver/skillengine/model/Effect.h"
#include "aion/gameserver/utils/ThreadPoolManager.h"
#include "aion/gameserver/utils/stats/StatFunctions.h"

namespace aion::gameserver::skillengine::effect {

using controllers::observer::DeathObserver;
using gameserver::model::gameobjects::Creature;
using gameserver::model::gameobjects::Npc;
using runtime::Ptr;
using runtime::Ref;

namespace {

/** Java int a + b (wraps on overflow) */
constexpr int32_t addInt(int32_t a, int32_t b) noexcept {
	return static_cast<int32_t>(static_cast<uint32_t>(a) + static_cast<uint32_t>(b));
}

/** Java int a * b (wraps on overflow) */
constexpr int32_t mulInt(int32_t a, int32_t b) noexcept {
	return static_cast<int32_t>(static_cast<uint32_t>(a) * static_cast<uint32_t>(b));
}

/** Java int -a (wraps for Integer.MIN_VALUE) */
constexpr int32_t negInt(int32_t a) noexcept {
	return static_cast<int32_t>(0u - static_cast<uint32_t>(a));
}

} // namespace

// Stored lambda com.aionemu.gameserver.skillengine.effect.HostileUpEffect@L46:72 (the removal of the temporary hate after temp_duration, pin
// {&effect, &effected}) and lambda @L50:39 (the consumer of the DeathObserver attached once to the effector, capturing the task's Future). Neither
// goes through Effect.addObserver or Effect.setPeriodicTask, and cycles.toml has no row for either: the task's closure holds observerRef -> the
// DeathObserver -> its consumer's FutureRef -> the task -> the closure, a Ref cycle that lasts until temp_duration runs the task or the effector's
// death cancels it (a Future releases its callable when it runs or is cancelled, runtime/sched/Future.h); a row lint_concurrency asks for goes
// to the integrator (m5e-plan.md I-04). Java's template field tempHate is Effect::hostileUpTempHate here (HostileUpEffect.h), read by the task
// when it runs, as Java reads the field.
void HostileUpEffect::applyEffect(model::Effect& effect) const {
	Creature& effected = *effect.getEffected();
	if (runtime::as<Npc>(effected)) {
		int32_t totalHate = effect.getTauntHate();
		// FIXME some skills never broadcast regular hate. that's why the following check exists as a workaround, which should be removed once fixed
		// hate broadcasts in Effect.startEffect (if added to EffectController) and applyEffect (if there are no successEffects), so some never do
		if (effect.getSuccessEffects().size() == 1) // only this effect template is present, therefore we know regular hate will never broadcast
			totalHate = addInt(totalHate, effect.getEffectHate());
		effected.getAggroList().addHate(*effect.getEffector(), addInt(totalHate, effect.getHostileUpTempHate()));
		if (effect.getHostileUpTempHate() > 0) {
			Ref<runtime::Rc<runtime::AtomicReference<Ref<DeathObserver>>>> observerRef =
				runtime::Rc<runtime::AtomicReference<Ref<DeathObserver>>>::create();
			runtime::FutureRef task = utils::ThreadPoolManager::getInstance().schedule(runtime::Pin{&effect, &effected}, [&effect, &effected, observerRef] {
				effected.getAggroList().addHate(*effect.getEffector(), negInt(effect.getHostileUpTempHate()));
				// Java removeObserver(observerRef.get()): null only if the task runs before the observer is set, and removing null removes nothing
				if (Ptr<DeathObserver> observer = observerRef->get())
					effect.getEffector()->getObserveController()->removeObserver(*observer);
			}, tempDuration);
			observerRef->set(DeathObserver::create(runtime::PinnedCallback<void(Creature&)>(runtime::Pin(), [task](Creature&) { task->cancel(false); })));
			effect.getEffector()->getObserveController()->attach(*observerRef->get());
		}
	}
}

void HostileUpEffect::calculate(model::Effect& effect) const {
	if (!EffectTemplate::calculate(effect, std::nullopt, std::nullopt))
		return;
	effect.setTauntHate(calculateBaseValue(effect));
	effect.setHostileUpTempHate(addInt(tempValue, mulInt(tempDelta, effect.getSkillLevel())));
	if (effect.getHostileUpTempHate() > 0)
		effect.setHostileUpTempHate(utils::stats::StatFunctions::calculateHate(*effect.getEffected(), effect.getHostileUpTempHate()));
}

} // namespace aion::gameserver::skillengine::effect
