#include "aion/gameserver/skillengine/effect/OneTimeBoostSkillAttackEffect.h"

#include <cstdint>

#include "aion/gameserver/controllers/effect/EffectController.h"
#include "aion/gameserver/controllers/observer/AttackCalcObserver.h"
#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/skillengine/model/Effect.h"
#include "aion/gameserver/skillengine/model/SkillType.h"
#include "aion/gameserver/utils/ThreadPoolManager.h"

namespace aion::gameserver::skillengine::effect {

using runtime::Ref;

/**
 * Java: the anonymous AttackCalcObserver of OneTimeBoostSkillAttackEffect.startEffect (OneTimeBoostSkillAttackEffect.java:33-55, fieldmap key
 * OneTimeBoostSkillAttackEffect$1): the first `count` skill attacks of the effected (physical ones for PHYSICAL, magical ones for MAGICAL, both
 * for ALL, counted together) are multiplied by `percent`; the one that reaches the count schedules the effect's removal. Stored in the effected
 * creature's ObserveController and, through the removal task of Effect.addObserver, in the effect's observerRemoveTasks; Effect.endEffect ->
 * removeObservers removes it from both (cycles.toml "OneTimeBoostSkillAttackEffect$1#effect": java-hook).
 * <p>
 * Java's body reads the template's private `count` and `type` and calls its private `removeEffect` through the captured this.
 * OneTimeBoostSkillAttackEffect.h has no friend line for this struct (m5e-plan.md §7 expects one), so startEffect, which may read them, hands
 * the two values and a pointer to the member function to the constructor: the same values Java reads, because the template is immutable
 * static data after load, and the same call (docs/deviations/P5-04.md, header request `friend struct
 * OneTimeBoostSkillAttackEffect_AttackCalcObserver;`).
 */
struct OneTimeBoostSkillAttackEffect_AttackCalcObserver final : controllers::observer::AttackCalcObserver {
	AION_MAKE_REF_FRIEND

	/** The template's private removeEffect, taken by startEffect (the member function Java calls) */
	using RemoveEffect = void (OneTimeBoostSkillAttackEffect::*)(model::Effect&) const;

	const OneTimeBoostSkillAttackEffect* oneTimeBoostSkillAttackEffect; // captured this OneTimeBoostSkillAttackEffect this (line 39)
	const Ref<model::Effect> effect; // captured param Effect effect (line 41)
	const float percent; // captured local float percent (line 42)
	runtime::Field<int32_t> boostCount{}; // int boostCount (line 35) [non-final scalar]
	const int32_t count; // fieldmap: C++ only, oneTimeBoostSkillAttackEffect->count (private, no friend line), immutable static data
	const model::SkillType type; // fieldmap: C++ only, oneTimeBoostSkillAttackEffect->type (private, no friend line), immutable static data
	const RemoveEffect removeEffect; // fieldmap: C++ only, &OneTimeBoostSkillAttackEffect::removeEffect (private, no friend line)

	static Ref<OneTimeBoostSkillAttackEffect_AttackCalcObserver> create(const OneTimeBoostSkillAttackEffect& oneTimeBoostSkillAttackEffect,
		model::Effect& effect, float percent, int32_t count, model::SkillType type, RemoveEffect removeEffect) {
		return runtime::makeRef<OneTimeBoostSkillAttackEffect_AttackCalcObserver>(oneTimeBoostSkillAttackEffect, effect, percent, count, type,
			removeEffect);
	}

	float getBasePhysicalDamageMultiplier(bool isSkill) override {
		if (isSkill && type != model::SkillType::MAGICAL && postIncrementBoostCount() < count) {
			if (boostCount.get() == count)
				(oneTimeBoostSkillAttackEffect->*removeEffect)(*effect);
			return percent;
		}
		return 1.0f;
	}

	float getBaseMagicalDamageMultiplier() override {
		if (type != model::SkillType::PHYSICAL && postIncrementBoostCount() < count) {
			if (boostCount.get() == count)
				(oneTimeBoostSkillAttackEffect->*removeEffect)(*effect);
			return percent;
		}
		return 1.0f;
	}

protected:
	OneTimeBoostSkillAttackEffect_AttackCalcObserver(const OneTimeBoostSkillAttackEffect& oneTimeBoostSkillAttackEffectValue,
		model::Effect& effectValue, float percentValue, int32_t countValue, model::SkillType typeValue, RemoveEffect removeEffectValue)
		: oneTimeBoostSkillAttackEffect(&oneTimeBoostSkillAttackEffectValue), effect(Ref<model::Effect>(effectValue)), percent(percentValue),
		  count(countValue), type(typeValue), removeEffect(removeEffectValue) {}
	~OneTimeBoostSkillAttackEffect_AttackCalcObserver() override = default;

private:
	/** Java `boostCount++` (int, wraps): the old value; java-race: a plain read-modify-write, like Java's */
	int32_t postIncrementBoostCount() {
		const int32_t old = boostCount.get();
		boostCount.set(static_cast<int32_t>(static_cast<uint32_t>(old) + 1u));
		return old;
	}
};

// Anonymous class com.aionemu.gameserver.skillengine.effect.OneTimeBoostSkillAttackEffect$1: the callback struct above
void OneTimeBoostSkillAttackEffect::startEffect(model::Effect& effect) const {
	BufEffect::startEffect(effect);
	const float percent = 1.0f + static_cast<float>(value) / 100.0f;
	// Java `switch (type)` on the unboxed enum: a template without type= is a NullPointerException here (none in the data: 10 of 10 carry one)
	if (!type.has_value())
		throw runtime::NullPointerException("OneTimeBoostSkillAttackEffect.type is null");
	switch (*type) {
		case model::SkillType::PHYSICAL:
		case model::SkillType::MAGICAL:
		case model::SkillType::ALL:
			effect.addObserver(*effect.getEffected(), *OneTimeBoostSkillAttackEffect_AttackCalcObserver::create(*this, effect, percent, count, *type,
				&OneTimeBoostSkillAttackEffect::removeEffect));
			break;
		default:
			break;
	}
}

void OneTimeBoostSkillAttackEffect::removeEffect(model::Effect& effect) const {
	// Java: lambda capturing effect (fieldmap callback OneTimeBoostSkillAttackEffect@L61:44), pinned to the effect until it has run
	utils::ThreadPoolManager::getInstance().schedule({&effect}, [&effect] {
		effect.getEffected()->getEffectController()->removeEffect(effect.getSkillId());
	}, 100);
}

} // namespace aion::gameserver::skillengine::effect
