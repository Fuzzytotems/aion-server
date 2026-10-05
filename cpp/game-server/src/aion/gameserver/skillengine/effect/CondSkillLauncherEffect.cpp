#include "aion/gameserver/skillengine/effect/CondSkillLauncherEffect.h"

#include <algorithm>
#include <cstdint>
#include <optional>
#include <vector>

#include "aion/gameserver/controllers/observer/ActionObserver.h"
#include "aion/gameserver/controllers/observer/ObserverType.h"
#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/model/stats/container/CreatureLifeStats.h"
#include "aion/gameserver/runtime/collections/ArrayDeque.h"
#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/sync/Monitor.h"
#include "aion/gameserver/skillengine/SkillEngine.h"
#include "aion/gameserver/skillengine/model/ActivationAttribute.h"
#include "aion/gameserver/skillengine/model/Effect.h"
#include "aion/gameserver/skillengine/model/SkillTemplate.h"

namespace aion::gameserver::skillengine::effect {

using runtime::Ptr;
using runtime::Ref;

namespace {

/** Java int a * b (wraps on overflow) */
constexpr int32_t mulInt(int32_t a, int32_t b) noexcept {
	return static_cast<int32_t>(static_cast<uint32_t>(a) * static_cast<uint32_t>(b));
}

/** C++ only (the lock-order correction): the launcher observers whose reports the current thread is running, to tell a nested report */
thread_local std::vector<const void*> runningObservers;

} // namespace

/**
 * Java: the anonymous ActionObserver(ObserverType.HP_CHANGED) of CondSkillLauncherEffect.startEffect (CondSkillLauncherEffect.java:35-59,
 * fieldmap key CondSkillLauncherEffect$1), with its own field conditionalEffect: the effect of skill_id it applied while the effected's HP is at or
 * below value % of its maximum. Stored in the effected creature's ObserveController and, through the removal task of Effect.addObserver, in
 * Effect.observerRemoveTasks; Effect.endEffect -> removeObservers removes it from both and ObserveController.removeObserver calls onRemoved,
 * which ends the conditional effect (cycles.toml "CondSkillLauncherEffect$1#effect": java-hook; ".conditionalEffect": accepted, the observer is
 * held only through Effect.addObserver).
 * <p>
 * `value` in the body is the template's (ActionObserver has no field of that name): its public generated getter answers the field, which no
 * subclass overrides. Java's body also reads the template's protected skillId through the captured this; CondSkillLauncherEffect.h has no friend
 * line for this struct, so startEffect hands the value to the constructor: the same value at the same moment, because the template never changes
 * after load (docs/deviations/P5-03.md, header request `friend struct CondSkillLauncherEffect_ActionObserver;`, the FearEffect$1 precedent).
 */
struct CondSkillLauncherEffect_ActionObserver final : controllers::observer::ActionObserver {
	AION_MAKE_REF_FRIEND

	const CondSkillLauncherEffect* condSkillLauncherEffect; // captured this CondSkillLauncherEffect this (line 41), immortal static data
	const Ref<model::Effect> effect;						// captured param Effect effect (line 41)
	runtime::Field<Ref<model::Effect>> conditionalEffect{}; // Effect conditionalEffect (line 37)
	// C++ only, the lock-order correction below: whether a thread is running reports, and the reports other threads left meanwhile
	runtime::Field<bool> running{};
	runtime::ArrayDeque<int8_t> pendingReports{AION_LOCK_CLASS(CondSkillLauncherEffect_ActionObserver::pendingReports)};
	const int32_t skillId; // fieldmap: C++ only, condSkillLauncherEffect->skillId (protected, no friend line), immutable static data

	static Ref<CondSkillLauncherEffect_ActionObserver> create(const CondSkillLauncherEffect& condSkillLauncherEffect, int32_t skillId,
		model::Effect& effect) {
		return runtime::makeRef<CondSkillLauncherEffect_ActionObserver>(condSkillLauncherEffect, skillId, effect);
	}

	/**
	 * Correction of the Java code (owner's decision 2026-10-04, both branches; docs/deviations/P5-03.md). Java runs hpChanged's decision and
	 * its Effect call inside `synchronized (this)` (CondSkillLauncherEffect.java:42-51), so it takes an Effect's monitor (Effect.startEffect /
	 * endEffect are synchronized) while holding the observer's, and every effect that changes the effected's max HP takes them the other way
	 * round (Effect.startEffect -> CreatureGameStats.checkMaxHPChanged -> notifyHPChangeObservers -> this): two threads doing both deadlock
	 * (lockdep reported it in the M5e gate). The monitor did two things, and both are kept without holding it across an Effect call:
	 * - it serialized the reports of different threads: here one thread runs the reports, and a report arriving from another thread meanwhile
	 *   is queued and run by it next, in order, instead of blocking (Java's reporting thread would wait and then run the same body);
	 * - it let a report of the running thread re-enter (a nested hpChanged during the launch or the end): here that report runs inline, as
	 *   Java's reentrant monitor runs it, with Java's state at that point (conditionalEffect is still null during a launch and still set
	 *   during an end).
	 * The monitor now guards only `running` and the queue. An exception of a report is rethrown after `running` is cleared (the queued reports
	 * of other threads are dropped with it; the next report re-evaluates the HP).
	 */
	// lint: L7 the owner's lock-order correction (above): one monitor section to claim the reports, one per report to take the next
	void hpChanged(int32_t hpValue) override {
		// Java int arithmetic: value * maxHp wraps, the division truncates toward zero
		bool hpAtOrBelowThreshold = hpValue <= mulInt(condSkillLauncherEffect->getValue(), effect->getEffected()->getLifeStats()->getMaxHp()) / 100;
		if (std::find(runningObservers.begin(), runningObservers.end(), this) != runningObservers.end()) {
			report(hpAtOrBelowThreshold); // a nested report of the running thread: inline, as Java's reentrant monitor
			return;
		}
		bool queued = false;
		SYNCHRONIZED(*this) {
			if (running.get()) {
				pendingReports.add(static_cast<int8_t>(hpAtOrBelowThreshold));
				queued = true;
			} else {
				running.set(true);
			}
		}
		if (queued)
			return;
		runningObservers.push_back(this);
		try {
			bool next = hpAtOrBelowThreshold;
			bool more = true;
			while (more) {
				report(next);
				SYNCHRONIZED(*this) {
					if (std::optional<int8_t> pending = pendingReports.poll())
						next = *pending != 0;
					else {
						running.set(false);
						more = false;
					}
				}
			}
		} catch (...) {
			SYNCHRONIZED(*this) {
				pendingReports.clear();
				running.set(false);
			}
			std::erase(runningObservers, this);
			throw;
		}
		std::erase(runningObservers, this);
	}

	void onRemoved() override {
		// Java reads the field twice outside the monitor; one read here, so a concurrent reset between the test and the call is no null call
		if (Ptr<model::Effect> conditional = conditionalEffect.get())
			conditional->endEffect();
	}

private:
	/** The body of Java's synchronized block (CondSkillLauncherEffect.java:43-50), statement for statement, without the monitor */
	void report(bool hpAtOrBelowThreshold) {
		if (hpAtOrBelowThreshold && !conditionalEffect.get()) {
			bool permanent = effect->getSkillTemplate()->getActivationAttribute() == model::ActivationAttribute::PASSIVE;
			// passive skills like Determination have no time limit
			std::optional<int32_t> duration = permanent ? std::optional<int32_t>(0) : std::nullopt;
			conditionalEffect.set(
				SkillEngine::getInstance().applyEffectDirectly(skillId, *effect->getEffected(), *effect->getEffected(), duration, nullptr));
		} else if (!hpAtOrBelowThreshold && conditionalEffect.get()) {
			conditionalEffect.get()->endEffect();
			conditionalEffect.set(nullptr);
		}
	}

protected:
	CondSkillLauncherEffect_ActionObserver(const CondSkillLauncherEffect& condSkillLauncherEffectValue, int32_t skillIdValue,
		model::Effect& effectValue)
		: ActionObserver(controllers::observer::ObserverType::HP_CHANGED), condSkillLauncherEffect(&condSkillLauncherEffectValue),
		  effect(Ref<model::Effect>(effectValue)), skillId(skillIdValue) {}
	~CondSkillLauncherEffect_ActionObserver() override = default;
};

// TODO what if you fall? effect is not applied? what if you use skill that consume hp?
void CondSkillLauncherEffect::applyEffect(model::Effect& effect) const {
	effect.addToEffectedController();
}

// Anonymous class com.aionemu.gameserver.skillengine.effect.CondSkillLauncherEffect$1: the callback struct CondSkillLauncherEffect_ActionObserver
// above
void CondSkillLauncherEffect::startEffect(model::Effect& effect) const {
	effect.addObserver(*effect.getEffected(), *CondSkillLauncherEffect_ActionObserver::create(*this, skillId, effect));
}

} // namespace aion::gameserver::skillengine::effect
