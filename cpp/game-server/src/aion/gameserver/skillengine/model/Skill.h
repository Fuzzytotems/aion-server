#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "aion/gameserver/runtime/collections/ArrayList.h"
#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/gameserver/controllers/observer/fwd.h"
#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/model/stats/container/fwd.h"
#include "aion/gameserver/model/templates/item/fwd.h"
#include "aion/gameserver/skillengine/model/Skill_SkillMethod.h"
#include "aion/gameserver/skillengine/model/fwd.h"
#include "aion/gameserver/skillengine/properties/Properties_CastState.h"
#include "aion/gameserver/skillengine/properties/fwd.h"

namespace aion::gameserver::skillengine::model {

/**
 * One cast of a skill: validation, cast start, cast end and the creation and application of its effects.
 * <p>
 * Hub header (docs/design/hub-headers.md). RefCounted (fieldmap K4: SM_CASTSPELL_RESULT member, captured by the cast tasks and the first target
 * death observer), created with `Skill::create(...)` (ChargeSkill and PenaltySkill derive and call the protected constructors).
 * - Java's Skill constructor calls the overridable initializeSkillMethod(); a C++ constructor cannot dispatch to the subclass override, so
 *   PenaltySkill's constructor calls its own initializeSkillMethod() again after the base constructor (porting note for P5-02).
 * - `firstTarget` is nullable (SkillEngine passes `null` when the target is no Creature; setFirstTarget stores it), so the constructors take
 *   `Ptr<Creature>`.
 * - `chainCategory` is `Field<std::string>` (fieldmap): the empty string stands for Java null (xmlgen binds a missing ChainCondition category
 *   as "" too).
 * - The effect lists built by endCast hold new Effects that nothing else holds yet (and the hit-time task captures the list), so the private
 *   helpers take `const std::vector<runtime::Ref<Effect>>&`.
 * - The cast tasks (Skill.java:312/314/663) and the first target DeathObserver lambda (:535) are callback structs in Skill.cpp once ported.
 *
 * @author ATracer, Wakizashi, Neon
 */
class Skill : public runtime::RefCounted {
	AION_MAKE_REF_FRIEND
public:
	using SkillMethod = Skill_SkillMethod;

private:
	// Java: private static final Logger log - namespace-scope logger in Skill.cpp (hub-headers.md §11.3)
	runtime::ArrayList<runtime::Ref<gameserver::model::gameobjects::Creature>> effectedList{AION_LOCK_CLASS(Skill::effectedList)};
	runtime::Field<runtime::Ref<gameserver::model::gameobjects::Creature>> firstTarget{};

protected:
	const runtime::Ref<gameserver::model::gameobjects::Creature> effector;

private:
	const int32_t skillLevel;

protected:
	runtime::Field<Skill::SkillMethod> skillMethod{};
	const runtime::Ref<controllers::observer::StartMovingListener> moveListener;

private:
	const SkillTemplate* skillTemplate;
	runtime::Field<bool> firstTargetRangeCheck{true};
	const gameserver::model::templates::item::ItemTemplate* itemTemplate;
	runtime::Field<int32_t> itemObjectId{0};
	runtime::Field<int32_t> targetType{};
	runtime::Field<bool> chainSuccess{true};
	runtime::Field<bool> isCancelled{false};
	runtime::Field<bool> blockedPenaltySkill{false};
	runtime::Field<float> x{};
	runtime::Field<float> y{};
	runtime::Field<float> z{};
	runtime::Field<int8_t> h{};
	runtime::Field<int32_t> boostSkillCost{};
	/** Duration that depends on BOOST_CASTING_TIME */
	runtime::Field<int32_t> baseCastDuration{};
	runtime::Field<int32_t> castDuration{};
	/** from CM_CASTSPELL */
	runtime::Field<int32_t> clientHitTime{};
	/** time when effect is applied */
	runtime::Field<int32_t> hitTime{};
	/** cast speed can boost the animation time of the current skill and the hit time of the following skill */
	runtime::Field<float> castSpeedForAnimationBoostAndChargeSkills{};
	runtime::Field<int64_t> castStartTime{};
	runtime::Field<std::string> chainCategory{};
	runtime::Field<int32_t> chainUsageDuration{0};
	runtime::Field<int32_t> hate{};
	runtime::Field<runtime::Ref<controllers::observer::DeathObserver>> firstTargetDieObserver{};

protected:
	/** Each skill is a separate object upon invocation Skill level will be populated from player SkillList */
	Skill(const SkillTemplate* skillTemplate, gameserver::model::gameobjects::player::Player& effector,
		runtime::Ptr<gameserver::model::gameobjects::Creature> firstTarget);

	Skill(const SkillTemplate* skillTemplate, gameserver::model::gameobjects::player::Player& effector,
		runtime::Ptr<gameserver::model::gameobjects::Creature> firstTarget, int32_t skillLevel);

	Skill(const SkillTemplate* skillTemplate, gameserver::model::gameobjects::Creature& effector, int32_t skillLvl,
		runtime::Ptr<gameserver::model::gameobjects::Creature> firstTarget, const gameserver::model::templates::item::ItemTemplate* itemTemplate);

	~Skill() override;

public:
	/** Java `new Skill(skillTemplate, effector, firstTarget)` */
	static runtime::Ref<Skill> create(const SkillTemplate* skillTemplate, gameserver::model::gameobjects::player::Player& effector,
		runtime::Ptr<gameserver::model::gameobjects::Creature> firstTarget);

	/** Java `new Skill(skillTemplate, effector, firstTarget, skillLevel)` */
	static runtime::Ref<Skill> create(const SkillTemplate* skillTemplate, gameserver::model::gameobjects::player::Player& effector,
		runtime::Ptr<gameserver::model::gameobjects::Creature> firstTarget, int32_t skillLevel);

	/** Java `new Skill(skillTemplate, effector, skillLvl, firstTarget, itemTemplate)` */
	static runtime::Ref<Skill> create(const SkillTemplate* skillTemplate, gameserver::model::gameobjects::Creature& effector, int32_t skillLvl,
		runtime::Ptr<gameserver::model::gameobjects::Creature> firstTarget, const gameserver::model::templates::item::ItemTemplate* itemTemplate);

protected:
	virtual void initializeSkillMethod();

public:
	/**
	 * Check if the skill can be used
	 *
	 * @return True if the skill can be used
	 */
	bool canUseSkill(properties::Properties_CastState castState);

private:
	/**
	 * Checks the costs the cast will have to pay when it ends, without paying them, so that a cast which cannot be afforded never starts (example:
	 * Dimensional Fragments for Summon Group Member, skillId: 3777).
	 *
	 * @return True, if all costs can be paid
	 */
	bool canPayCastCosts();

	bool validateEffectedList();

	bool canUseSkill(gameserver::model::gameobjects::player::Player& player);

	bool isValidTarget(gameserver::model::gameobjects::player::Player& player, gameserver::model::gameobjects::Creature& target);

public:
	/**
	 * Skill entry point
	 *
	 * @return true if usage is successful
	 */
	virtual bool useSkill();

	bool useNoAnimationSkill();

	bool useWithoutPropSkill();

private:
	bool useSkill(bool checkAnimation, bool checkproperties);

	void setCooldowns();

public:
	int32_t getCooldown();

protected:
	void updateCastDurationAndSpeed();

private:
	int32_t calculateChargeCastDuration();

	int32_t calculateCastDuration();

	int32_t calculateMagicalCastDuration();

	/** Java returns null for sub types without a cast boost stat */
	std::optional<gameserver::model::stats::container::StatEnum> getSkillCastBoostStat();

	bool isSummonType(SkillSubType type);

protected:
	void updateHitTime(bool checkAnimation);

private:
	float getDistanceTolerance(gameserver::model::gameobjects::player::Player& player, gameserver::model::gameobjects::Creature& target);

	bool isSuspiciousClientHitTime(int32_t clientHitTime, int32_t serverHitTime, int32_t tolerance, gameserver::model::gameobjects::player::Player& player);

	std::vector<std::string> collectUncertaintyFactorsForHitTime(gameserver::model::gameobjects::player::Player& player, int32_t toleranceMillis);

	void startPenaltySkill();

	/** Start casting of skill */
	void startCast();

public:
	void cancelCast();

private:
	void cancelCurrentSkillCast();

protected:
	/** Apply effects and perform actions specified in skill template */
	void endCast();

private:
	/** C++ port: also sets firstTargetDieObserver to null after removing it (C++ breaker, cycles.toml Skill.firstTargetDieObserver) */
	void removeObservers();

	void addResistedEffectHateAndNotifyFriends(const std::vector<runtime::Ref<Effect>>& effects);

	void applyEffect(const std::vector<runtime::Ref<Effect>>& effects);

	/**
	 * Recall skills (Summon Group Member, example skillId: 3777) validate their target when the cast starts and again when it ends. The caster is
	 * told why it failed.
	 *
	 * @return True, if this is a recall skill whose target may not be recalled
	 */
	bool isInvalidRecall();

	/** @return True, if this skill is meant to be used against enemies (which is what puts caster and targets into combat) */
	bool isHostile();

	bool sendCastSpellEnd(int32_t dashStatus, const std::vector<runtime::Ref<Effect>>& effects);

	/**
	 * Consumes everything the cast costs: the used item, the skill conditions and the skill actions (mp, dp, items).
	 *
	 * @return False, if any of them could not be paid, in which case the cast must be cancelled
	 */
	bool payCastCosts();

	/** Check all conditions before starting cast */
	bool preCastCheck();

	/** Check all conditions before using skill */
	bool preUsageCheck();

	/**
	 * Pays the conditions the skill costs when it ends (mp, hp, item and stone charges).
	 *
	 * @return False, if one of them could not be paid
	 */
	bool endCondCheck();

public:
	/** @param value is the changeMpConsumptionValue to set */
	void setBoostSkillCost(int32_t value) { boostSkillCost.set(value); }

	/** @return the changeMpConsumptionValue */
	int32_t getBoostSkillCost() const { return boostSkillCost.get(); }

	/** @return the effectedList */
	runtime::ArrayList<runtime::Ref<gameserver::model::gameobjects::Creature>>& getEffectedList() { return effectedList; }

	/** @return the effector */
	runtime::Ptr<gameserver::model::gameobjects::Creature> getEffector() const { return effector; }

	/** @return the skillLevel */
	int32_t getSkillLevel() const { return skillLevel; }

	/** @return the skillId */
	int32_t getSkillId();

	/** @return the conditionChangeListener */
	runtime::Ptr<controllers::observer::StartMovingListener> getMoveListener() const { return moveListener; }

	/** @return the skillTemplate */
	const SkillTemplate* getSkillTemplate() const { return skillTemplate; }

	/** @return the firstTarget */
	runtime::Ptr<gameserver::model::gameobjects::Creature> getFirstTarget() const { return firstTarget.get(); }

	/** @param firstTarget the firstTarget to set (nullable) */
	void setFirstTarget(runtime::Ptr<gameserver::model::gameobjects::Creature> firstTarget);

	/** @return true or false */
	bool isPassive();

	/** @return the firstTargetRangeCheck */
	bool isFirstTargetRangeCheck() const { return firstTargetRangeCheck.get(); }

	/** Java returns null for a template without properties */
	std::optional<properties::FirstTargetAttribute> getFirstTargetAttribute();

	/** Java returns null for a template without properties */
	std::optional<properties::TargetRangeAttribute> getTargetRangeAttribute();

	/** @return true if the present skill is a non-targeted, non-point AOE skill */
	bool isNonTargetAOE();

private:
	/** @return true if the present skill is a targeted AOE skill */
	bool isTargetAOE();

public:
	/** @return true if the present skill is a self buff includes items (such as scroll buffs) */
	bool isSelfBuff();

	/** @return true if the present skill has self as first target */
	bool isFirstTargetSelf();

	/** @return true if the present skill is a Point skill */
	bool isPointSkill();

	/** @param firstTargetRangeCheck the firstTargetRangeCheck to set */
	void setFirstTargetRangeCheck(bool value) { firstTargetRangeCheck.set(value); }

	const gameserver::model::templates::item::ItemTemplate* getItemTemplate() const { return itemTemplate; }

	void setItemObjectId(int32_t id) { itemObjectId.set(id); }

	int32_t getItemObjectId() const { return itemObjectId.get(); }

	void setTargetType(int32_t targetTypeValue, float xValue, float yValue, float zValue) {
		targetType.set(targetTypeValue);
		x.set(xValue);
		y.set(yValue);
		z.set(zValue);
	}

	/** Calculated position after skill */
	void setTargetPosition(float xValue, float yValue, float zValue, int8_t hValue) {
		x.set(xValue);
		y.set(yValue);
		z.set(zValue);
		h.set(hValue);
	}

	float getX() const { return x.get(); }

	float getY() const { return y.get(); }

	float getZ() const { return z.get(); }

	int8_t getH() const { return h.get(); }

protected:
	void setCastStartTime(int64_t value) { castStartTime.set(value); }

public:
	void setClientHitTime(int32_t time) { clientHitTime.set(time); }

	int32_t getHitTime() const { return hitTime.get(); }

protected:
	void setCastSpeedForAnimationBoostAndChargeSkills(float value) { castSpeedForAnimationBoostAndChargeSkills.set(value); }

public:
	float getCastSpeedForAnimationBoostAndChargeSkills() const { return castSpeedForAnimationBoostAndChargeSkills.get(); }

	/**
	 * The game client allows to boost the animation time of a skill via cast speed:<br>
	 * - only half of the castSpeedForAnimationBoostAndChargeSkills cast speed boost is considered and only if this boost exceeds the attack speed boost
	 * - animation time considers the current cast speed boost<br>
	 * - hit time considers cast speed boost of the previously cast skill
	 * - hit time will only be boosted if the current skill if cast before the animation of the previous skill finishes<br>
	 */
	bool allowAnimationBoostByCastSpeed();

private:
	bool isCastDurationAffectedByCastSpeed();

public:
	/** @param chainCategory empty for Java null */
	void setChainCategory(std::string_view value) { chainCategory.set(std::string(value)); }

	void setChainUsageDuration(int32_t duration) { chainUsageDuration.set(duration); }

	SkillMethod getSkillMethod() const { return skillMethod.get(); }

private:
	bool isPointPointSkill();

public:
	int32_t getMultiCastCount();

	int64_t getCastStartTime() const { return castStartTime.get(); }

	bool isInstantSkill();

	int32_t getHate() const { return hate.get(); }

	void setHate(int32_t value) { hate.set(value); }
};

} // namespace aion::gameserver::skillengine::model
