#pragma once

#include "aion/gameserver/skillengine/effect/EffectTemplate.xml.h"

#include <cstdint>
#include <optional>

#include "aion/gameserver/model/fwd.h"
#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/model/stats/container/fwd.h"
#include "aion/gameserver/skillengine/effect/modifier/fwd.h"
#include "aion/gameserver/skillengine/model/fwd.h"

namespace aion::gameserver::skillengine::effect {

/**
 * Java com.aionemu.gameserver.skillengine.effect.EffectTemplate: the static data and behaviour root of all skill effects.
 * <p>
 * Hub header (docs/design/hub-headers.md): the xmlgen behaviour shell (static-data.md §2.2), extended in place. The generated member block holds
 * the JAXB fields, the trivial getters, the virtual destructor and `javaClassName()`; the hand-written methods follow in Java order. Templates
 * are immutable after load and referenced as `const EffectTemplate*`, so every method, virtual ones included, is const; the mutable state lives
 * in the `model::Effect&` argument.
 * - Nullable `StatEnum statEnum` and `SpellStatus spellStatus` parameters are `std::optional` (calculate(effect) passes null for both).
 * - The 111 subclasses override calculate(Effect&), applyEffect, startEffect, endEffect and friends; a subclass that overrides one calculate
 *   overload needs `using EffectTemplate::calculate;` to keep the others visible (C++ name hiding).
 * - The generated getters that Java subclasses override are virtual in the member block (xmlgen: getValue, getDuration2 and isNoResist, for
 *   AbstractOverTimeEffect and SkillAttackInstantEffect).
 *
 * @author ATracer
 */
class EffectTemplate : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/skillengine/effect/EffectTemplate.xml.inc"
public:
	/** @return the first modifier whose check passes for the effect, nullptr if none */
	const modifier::ActionModifier* getActionModifiers(model::Effect& effect) const;

protected:
	/** @return the base value (damage, heal, etc.) according to the skill template, it should equal the value stated in the skill description */
	virtual int32_t calculateBaseValue(model::Effect& effect) const;

public:
	int32_t calculateCritAddDmg(model::Effect& effect) const;

	int32_t calculateCritProbMod(model::Effect& effect) const;

	/** Calculate effect result */
	virtual void calculate(model::Effect& effect) const;

	bool calculate(model::Effect& effect, std::optional<gameserver::model::stats::container::StatEnum> statEnum,
		std::optional<model::SpellStatus> spellStatus) const;

	/**
	 * 1) check conditions 2) check preeffect 3) check effectresistrate 4) check noresist 5) decide if its magical or physical effect 6) physical -
	 * check cannotmiss 7) check magic resist / dodge 8) addsuccess exceptions: buffbind buffsilence buffsleep buffstun randommoveloc recallinstant
	 * returneffect returnpoint shieldeffect signeteffect summoneffect xpboosteffect
	 */
	bool calculate(model::Effect& effect, std::optional<gameserver::model::stats::container::StatEnum> statEnum,
		std::optional<model::SpellStatus> spellStatus, gameserver::model::SkillElement element) const;

protected:
	/**
	 * Rolls or takes over the magical critical for this effect position. Called before the resist check, so even resisted effects can decide the
	 * critical of the following positions.
	 */
	virtual void resolveMagicalCritical(model::Effect& effect) const;

private:
	bool validateEffectConditions(model::Effect& effect) const;

	bool validatePreEffects(model::Effect& effect) const;

protected:
	virtual bool isDodgedOrResisted(model::Effect& effect, std::optional<gameserver::model::stats::container::StatEnum> statEnum) const;

private:
	/** @return true = no dodge/resist, false = dodged/resisted */
	bool checkDodgeOrResistRate(model::Effect& effect) const;

	void addSuccessEffect(model::Effect& effect, std::optional<model::SpellStatus> spellStatus) const;

public:
	/** Apply effect to effected (Java abstract: every concrete effect shell overrides it) */
	virtual void applyEffect(model::Effect& effect) const = 0;

	/** Start effect on effected */
	virtual void startEffect(model::Effect& effect) const;

	virtual void calculateDamage(model::Effect& effect) const;

	void calculateSubEffect(model::Effect& effect) const;

private:
	/** Check all sub effect condition statuses for effect */
	bool effectSubConditionsCheck(model::Effect& effect) const;

public:
	int32_t calculateHate(model::Effect& effect) const;

	void startSubEffect(model::Effect& effect) const;

	/** Do periodic effect on effected */
	virtual void onPeriodicAction(model::Effect& effect) const;

	/** End effect on effected */
	virtual void endEffect(model::Effect& effect) const;

	/** @return true = no resist, false = resisted */
	bool checkEffectResistRate(model::Effect& effect, std::optional<gameserver::model::stats::container::StatEnum> statEnum) const;

private:
	bool isImmuneToAbnormal(model::Effect& effect, gameserver::model::stats::container::StatEnum statEnum) const;

	/** @return true = it's an altered state effect, false = it is Poison/Bleed dot (normal Dots have statEnum null here) */
	bool isAlteredState(gameserver::model::stats::container::StatEnum stat) const;

	bool isProtectedByShield(gameserver::model::gameobjects::Creature& effected, gameserver::model::stats::container::StatEnum stat) const;

	/** @return the `<stat>_PENETRATION` constant, nullopt (Java null, logged) if there is none */
	std::optional<gameserver::model::stats::container::StatEnum> getPenetrationStat(gameserver::model::stats::container::StatEnum statEnum) const;
};

} // namespace aion::gameserver::skillengine::effect
