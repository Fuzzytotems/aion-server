#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

#include "aion/gameserver/runtime/collections/ArrayList.h"
#include "aion/gameserver/runtime/collections/ConcurrentHashMap.h"
#include "aion/gameserver/runtime/collections/HashSet.h"
#include "aion/gameserver/runtime/fields/Array.h"
#include "aion/gameserver/runtime/fields/Atomic.h"
#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/gameserver/runtime/sched/Future.h"
#include "aion/gameserver/runtime/sched/PinnedCallback.h"
#include "aion/gameserver/controllers/attack/AttackStatus.h"
#include "aion/gameserver/controllers/observer/fwd.h"
#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/model/stats/calc/StatOwner.h"
#include "aion/gameserver/model/templates/item/fwd.h"
#include "aion/gameserver/skillengine/effect/fwd.h"
#include "aion/gameserver/skillengine/model/DashStatus.h"
#include "aion/gameserver/skillengine/model/Effect_ForceType.h"
#include "aion/gameserver/skillengine/model/EffectResult.h"
#include "aion/gameserver/skillengine/model/SpellStatus.h"
#include "aion/gameserver/skillengine/model/SubEffectType.h"
#include "aion/gameserver/skillengine/model/fwd.h"

namespace aion::gameserver::skillengine::model {

/**
 * One application of a skill's effect templates from an effector to an effected creature: initialize, apply, start, end.
 * <p>
 * Hub header (docs/design/hub-headers.md). RefCounted (fieldmap K4: StatOwner stored by stat functions, packet members, EffectController maps),
 * created with `Effect::create(...)`; the constructors only store members and the Java field initializers (ported).
 * - `effector`/`effected` are `const Ref<Creature>`; the cycles through the creature's EffectController maps are resolved by the Java hook
 *   `endEffect` (cycles.toml, runtime-architecture.md §5.1, §14.2(c)). `effected` is nullable (Skill.java:612 `new Effect(this, null)` for
 *   point-point skills), so every constructor takes it as `Ptr<Creature>`.
 * - The stored lambdas and anonymous observers (fieldmap: `Effect$1`, `Effect$2` cancel-on-damage ActionObservers capturing this Effect, the
 *   end task at Effect.java:682, the periodic actions task at :871, the observer removal Runnables at :828/:833 held in `observerRemoveTasks`)
 *   are callback structs in Effect.cpp once their bodies are ported; they need no member beyond `endTask`, `periodicActionsTask` and
 *   `observerRemoveTasks`. removeObservers() (from endEffect) clears the observer captures, which breaks the Effect -> observer -> Effect cycle.
 * - `Integer duration` is `std::optional<int32_t>`; `Set<Integer> magicalCriticalPositions` (nullable) is a `const std::unordered_set<int32_t>*`.
 * - getReserveds/getReservedEffectsToSend may return new EffectReserved objects, so they return owning `Ref`s.
 * - Members named like methods get a trailing underscore (`isCancelOnDmg_`, `isSubEffect_`, CONVENTIONS keyword rule).
 *
 * @author ATracer, Wakizashi, Sippolo, kecimis, Neon
 */
class Effect : public runtime::RefCounted, public gameserver::model::stats::calc::StatOwner {
	AION_MAKE_REF_FRIEND
public:
	/** Java static nested class ForceType: hoisted to Effect_ForceType.h (interned, `const ForceType*`), see there. */
	using ForceType = Effect_ForceType;

private:
	const runtime::Ref<gameserver::model::gameobjects::Creature> effector;
	const runtime::Ref<gameserver::model::gameobjects::Creature> effected;
	const SkillTemplate* skillTemplate;
	const runtime::Ref<Skill> skill;
	const int32_t skillLevel;
	runtime::Field<std::optional<int32_t>> duration{};
	runtime::Field<int64_t> endTime{};
	runtime::Field<SubEffectType> subEffectType{SubEffectType::NONE};
	runtime::Field<runtime::FutureRef> endTask{};
	runtime::Field<runtime::Ref<runtime::Array<runtime::FutureRef>>> periodicTasks{};
	runtime::Field<runtime::FutureRef> periodicActionsTask{};
	runtime::Field<int32_t> effectedHp{-1};
	runtime::HashSet<runtime::Ref<EffectReserved>> reservedEffects{};
	runtime::Field<SpellStatus> spellStatus{SpellStatus::NONE};
	runtime::Field<DashStatus> dashStatus{DashStatus::NONE};
	runtime::Field<controllers::attack::AttackStatus> attackStatus{controllers::attack::AttackStatus::NORMALHIT};
	const runtime::Ref<runtime::Array<bool>> magicalCriticals;
	runtime::Field<bool> magicalCriticalRolled{};
	runtime::Field<bool> magicalCritical{};
	/** shield effects related */
	runtime::Field<int32_t> shieldDefense{};
	runtime::Field<int32_t> reflectedDamage{0};
	runtime::Field<int32_t> reflectedSkillId{0};
	runtime::Field<int32_t> protectedSkillId{0};
	runtime::Field<int32_t> protectedDamage{0};
	runtime::Field<int32_t> protectorId{0};
	runtime::Field<int32_t> mpAbsorbed{0};
	runtime::Field<int32_t> mpShieldSkillId{0};
	runtime::Field<bool> addedToController{};
	runtime::ArrayList<runtime::PinnedCallback<void()>> observerRemoveTasks{};
	runtime::Field<bool> launchSubEffect{true};
	runtime::Field<runtime::Ref<Effect>> subEffect{};
	runtime::AtomicBoolean hasEnded{};
	runtime::Field<bool> isCancelOnDmg_{};
	runtime::Field<bool> subEffectAbortedBySubConditions{};
	/** Hate that will be placed on effected list */
	runtime::Field<int32_t> tauntHate{};
	/** Total hate that will be broadcasted */
	runtime::Field<int32_t> effectHate{};
	runtime::ConcurrentHashMap<int32_t, const effect::EffectTemplate*> successEffects{};
	runtime::Field<int32_t> carvedSignet{0};
	runtime::Field<int32_t> signetBurstedCount{0};
	runtime::Field<int32_t> abnormals{};
	runtime::Field<float> targetX{};
	runtime::Field<float> targetY{};
	runtime::Field<float> targetZ{};
	runtime::Field<float> x{};
	runtime::Field<float> y{};
	runtime::Field<float> z{};
	runtime::Field<int32_t> worldId{};
	runtime::Field<int32_t> instanceId{};
	runtime::Field<const Effect::ForceType*> forceType{};
	/** power of effect ( used for dispels) */
	runtime::Field<int32_t> power{};
	/** accModBoost used for SignetBurstEffect */
	runtime::Field<int32_t> accModBoost{0};
	runtime::Field<EffectResult> effectResult{EffectResult::NORMAL};
	runtime::Field<bool> endedByTime{false};
	runtime::Field<runtime::Ref<Effect>> designatedDispelEffect{};
	/** Whether this effect is a sub effect of another effect */
	const bool isSubEffect_;
	runtime::Field<bool> applyCriticalProcEffect{false};
	runtime::AtomicBoolean allowGodstoneActivation{};

protected:
	Effect(Skill& skill, runtime::Ptr<gameserver::model::gameobjects::Creature> effected);

	Effect(gameserver::model::gameobjects::Creature& effector, runtime::Ptr<gameserver::model::gameobjects::Creature> effected,
		const SkillTemplate* skillTemplate, int32_t skillLevel);

	/** If duration is null, it will be calculated upon execution, else the forced value will be used. */
	Effect(gameserver::model::gameobjects::Creature& effector, runtime::Ptr<gameserver::model::gameobjects::Creature> effected,
		const SkillTemplate* skillTemplate, int32_t skillLevel, std::optional<int32_t> duration, const ForceType* forceType);

	/** @param magicalCriticalPositions nullable (Java null) */
	Effect(gameserver::model::gameobjects::Creature& effector, runtime::Ptr<gameserver::model::gameobjects::Creature> effected,
		const SkillTemplate* skillTemplate, int32_t skillLevel, std::optional<int32_t> duration, const ForceType* forceType, bool isSubEffect,
		const std::unordered_set<int32_t>* magicalCriticalPositions);

	~Effect() override;

public:
	/** Java `new Effect(skill, effected)` */
	static runtime::Ref<Effect> create(Skill& skill, runtime::Ptr<gameserver::model::gameobjects::Creature> effected);

	/** Java `new Effect(effector, effected, skillTemplate, skillLevel)` */
	static runtime::Ref<Effect> create(gameserver::model::gameobjects::Creature& effector, runtime::Ptr<gameserver::model::gameobjects::Creature> effected,
		const SkillTemplate* skillTemplate, int32_t skillLevel);

	/** Java `new Effect(effector, effected, skillTemplate, skillLevel, duration, forceType)` */
	static runtime::Ref<Effect> create(gameserver::model::gameobjects::Creature& effector, runtime::Ptr<gameserver::model::gameobjects::Creature> effected,
		const SkillTemplate* skillTemplate, int32_t skillLevel, std::optional<int32_t> duration, const ForceType* forceType);

	/** Java `new Effect(effector, effected, skillTemplate, skillLevel, duration, forceType, isSubEffect, magicalCriticalPositions)` */
	static runtime::Ref<Effect> create(gameserver::model::gameobjects::Creature& effector, runtime::Ptr<gameserver::model::gameobjects::Creature> effected,
		const SkillTemplate* skillTemplate, int32_t skillLevel, std::optional<int32_t> duration, const ForceType* forceType, bool isSubEffect,
		const std::unordered_set<int32_t>* magicalCriticalPositions);

	/** C++ only: Ref<StatOwner> retains the Effect itself (hub-headers.md §9.2). */
	void retain() const noexcept override { runtime::RefCounted::retain(); }
	void release() const noexcept override { runtime::RefCounted::release(); }

	void setWorldPosition(int32_t worldIdValue, int32_t instanceIdValue, float xValue, float yValue, float zValue) {
		x.set(xValue);
		y.set(yValue);
		z.set(zValue);
		worldId.set(worldIdValue);
		instanceId.set(instanceIdValue);
	}

	int32_t getEffectorId();

	runtime::Ptr<Skill> getSkill() const { return skill; }

	void setAbnormal(effect::AbnormalState state);

	int32_t getAbnormals() const { return abnormals.get(); }

	int32_t getSkillId();

	std::string getSkillName();

	const SkillTemplate* getSkillTemplate() const { return skillTemplate; }

	SkillSubType getSkillSubType();

	std::string getStack();

	int32_t getSkillLevel() const { return skillLevel; }

	int32_t getSkillStackLvl();

	SkillType getSkillType();

	int32_t getDuration();

	/** @return The effected from effect constructor, can differ from getEffected if effect got reflected via shield observer. */
	runtime::Ptr<gameserver::model::gameobjects::Creature> getOriginalEffected() const { return effected; }

	/** @return The creature which receives the damage/skill results. It will be the effector if isReflected() returns true. */
	runtime::Ptr<gameserver::model::gameobjects::Creature> getEffected();

	runtime::Ptr<gameserver::model::gameobjects::Creature> getEffector() const { return effector; }

	bool isPassive();

	bool isPeriodic();

	/** @param periodicTask nullable (Java null clears the position) */
	void setPeriodicTask(runtime::FutureRef periodicTask, int32_t position);

	controllers::attack::AttackStatus getAttackStatus() const { return attackStatus.get(); }

	void setAttackStatus(controllers::attack::AttackStatus value) { attackStatus.set(value); }

	bool isMagicalCritical(int32_t position);

	/**
	 * Rolls the magical critical for the given effect position, or takes over the one an earlier position already rolled, since a cast only rolls once
	 * per target.
	 */
	void rollMagicalCritical(int32_t position, int32_t criticalProb);

	/** Takes over the magical critical of an earlier effect position without rolling one. */
	void reuseMagicalCritical(int32_t position);

	/** An effect which got filtered out breaks the chain, so the next effect position rolls its own magical critical again. */
	void resetMagicalCritical();

private:
	void setMagicalCriticals(const std::unordered_set<int32_t>& positions);

public:
	/** Java returns the template's live list; the templates are immortal static data. */
	std::vector<const effect::EffectTemplate*> getEffectTemplates();

	bool isToggle();

	bool isChant();

	/** Java returns null for a template without `tslot` */
	std::optional<SkillTargetSlot> getTargetSlot();

	int32_t getTargetSlotLevel();

	DispelCategoryType getDispelCategory();

	int32_t getReqDispelLevel();

	int32_t getEffectedHp() const { return effectedHp.get(); }

	/** @return the reserved entry of the position, or a new empty one */
	runtime::Ref<EffectReserved> getReserveds(int32_t position);

	void setReserveds(EffectReserved& er, bool overTimeEffect);

	/** @return Java TreeSet order (EffectReserved::compareTo, no compareTo-duplicates); may hold a new EffectReserved, hence owning Refs */
	std::vector<runtime::Ref<EffectReserved>> getReservedEffectsToSend();

	bool isLaunchSubEffect() const { return launchSubEffect.get(); }

	void setLaunchSubEffect(bool value) { launchSubEffect.set(value); }

	int32_t getShieldDefense() const { return shieldDefense.get(); }

	void setShieldDefense(int32_t shieldDefense);

	/** @return True if the whole effect is reflected (through shield observer) */
	bool isReflected();

	int32_t getReflectedDamage() const { return reflectedDamage.get(); }

	void setReflectedDamage(int32_t value) { reflectedDamage.set(value); }

	int32_t getReflectedSkillId() const { return reflectedSkillId.get(); }

	void setReflectedSkillId(int32_t value) { reflectedSkillId.set(value); }

	int32_t getProtectedSkillId() const { return protectedSkillId.get(); }

	void setProtectedSkillId(int32_t skillId) { protectedSkillId.set(skillId); }

	int32_t getProtectedDamage() const { return protectedDamage.get(); }

	void setProtectedDamage(int32_t value) { protectedDamage.set(value); }

	int32_t getProtectorId() const { return protectorId.get(); }

	void setProtectorId(int32_t value) { protectorId.set(value); }

	SpellStatus getSpellStatus() const { return spellStatus.get(); }

	void setSpellStatus(SpellStatus value) { spellStatus.set(value); }

	DashStatus getDashStatus() const { return dashStatus.get(); }

	void setDashStatus(DashStatus value) { dashStatus.set(value); }

	/** Number of signets carved on target */
	int32_t getCarvedSignet() const { return carvedSignet.get(); }

	void setCarvedSignet(int32_t value) { carvedSignet.set(value); }

	runtime::Ptr<Effect> getSubEffect() const { return subEffect.get(); }

	void setSubEffect(runtime::Ptr<Effect> subEffect);

	bool containsEffectId(int32_t effectId);

	void setForceType(const ForceType* value) { forceType.set(value); }

	const ForceType* getForceType() const { return forceType.get(); }

	bool isForcedEffect();

	bool isPhysicalEffect();

	/**
	 * Do initialization with proper calculations<br>
	 * Correct lifecycle of Effect: INITIALIZE - APPLY - START - END
	 */
	void initialize();

private:
	int32_t calculateHateForSuccessEffects();

public:
	/** Apply all effect templates */
	void applyEffect();

	void broadcastHate();

private:
	/** @param effected nullable (getEffected() of a point-point skill effect) */
	bool shouldApplyFurtherEffects(runtime::Ptr<gameserver::model::gameobjects::Creature> effected);

public:
	/**
	 * Start effect which includes: - start effect defined in template - start subeffect if possible - activate toggle skill if needed - schedule end of
	 * effect
	 */
	void startEffect();

private:
	/** Will visually activate toggle skill */
	void activateToggleSkill();

	/** Will visually deactivate toggle skill */
	void deactivateToggleSkill();

public:
	void endEffect();

	/**
	 * End effect and all effect actions
	 * <p>
	 * C++ port: also resets designatedDispelEffect after clearEffect (C++ breaker, cycles.toml Effect.designatedDispelEffect).
	 */
	void endEffect(bool broadcast);

	/** Stop all scheduled tasks */
	void stopTasks();

	/** @return Time in milliseconds till the effect ends */
	int64_t getRemainingTimeMillis();

	int32_t getRemainingTimeToDisplay();

	bool canSaveOnLogout();

	int64_t getEndTime() const { return endTime.get(); }

	int32_t getPvpDamage();

	const gameserver::model::templates::item::ItemTemplate* getItemTemplate();

	/** Try to add this effect to effected controller */
	void addToEffectedController();

	int32_t getEffectHate() const { return effectHate.get(); }

	int32_t getTauntHate() const { return tauntHate.get(); }

	void setTauntHate(int32_t value) { tauntHate.set(value); }

	/** The removal task stored in observerRemoveTasks captures `target` and `observer` as Refs. */
	void addObserver(gameserver::model::gameobjects::Creature& target, controllers::observer::ActionObserver& observer);

	/** The removal task stored in observerRemoveTasks captures `target` and `observer` as Refs. */
	void addObserver(gameserver::model::gameobjects::Creature& target, controllers::observer::AttackCalcObserver& observer);

private:
	void removeObservers();

public:
	void addSuccessEffect(const effect::EffectTemplate* effect);

	bool isInSuccessEffects(int32_t position);

	const effect::EffectTemplate* effectInPos(int32_t pos);

	/** Java returns the live `successEffects.values()` view: a reference to the map (callers iterate values() or clear() it). */
	runtime::ConcurrentHashMap<int32_t, const effect::EffectTemplate*>& getSuccessEffects() { return successEffects; }

	void addAllEffectToSucess();

private:
	void schedulePeriodicActions();

	void stopPeriodicActions();

	int32_t calculateEffectsDuration();

	int64_t calculateTemplateDuration();

	int64_t applyCumulativeResistDurationMultiplier(int64_t duration, gameserver::model::gameobjects::player::Player& effected);

public:
	bool isDeityAvatar();

	float getX() const { return x.get(); }

	float getY() const { return y.get(); }

	float getZ() const { return z.get(); }

	int32_t getWorldId() const { return worldId.get(); }

	int32_t getInstanceId() const { return instanceId.get(); }

	SubEffectType getSubEffectType() const { return subEffectType.get(); }

	/**
	 * Client expects one byte(8bits) determining which effect positions (and sub effect type) were successful: <br>
	 *     If effect is dodged -> 0000 0000 <br>
	 *     If effect is resisted -> 0000 0001 <br>
	 *     If e1 is successful -> 0001 0000 <br>
	 *         If e1 is successful and has openaerial as a sub effect -> 0001 0100 <br>
	 *     If e1 and e4 are successful -> 1001 0000 <br><br>
	 *     This byte is used to output the corresponding chat messages.
	 * @return an integer containing all successful effect positions and a subeffect (if one exists)
	 */
	int8_t getSuccessfulEffectsAsByte();

	void setSubEffectType(SubEffectType value) { subEffectType.set(value); }

	float getTargetX() const { return targetX.get(); }

	float getTargetY() const { return targetY.get(); }

	float getTargetZ() const { return targetZ.get(); }

	void setTargetLoc(float xValue, float yValue, float zValue) {
		targetX.set(xValue);
		targetY.set(yValue);
		targetZ.set(zValue);
	}

	void setSubEffectAborted(bool value) { subEffectAbortedBySubConditions.set(value); }

	bool isSubEffectAbortedBySubConditions() const { return subEffectAbortedBySubConditions.get(); }

private:
	void addCancelOnDmgObserver();

public:
	void setCancelOnDmg(bool value) { isCancelOnDmg_.set(value); }

	bool isCancelOnDmg() const { return isCancelOnDmg_.get(); }

	void endEffects();

	int32_t getPower() const { return power.get(); }

	void setPower(int32_t value) { power.set(value); }

	int32_t removePower(int32_t power);

	void setAccModBoost(int32_t value) { accModBoost.set(value); }

	int32_t getAccModBoost() const { return accModBoost.get(); }

	bool isHideEffect();

	bool isParalyzeEffect();

	bool isStunEffect();

	bool isSanctuaryEffect();

	bool isDamageEffect();

	bool isNoDeathPenalty();

	bool isNoResurrectPenalty();

	bool isHiPass();

	bool isDelayedDamage();

	bool isSummoning();

	bool isPetOrderUnSummonEffect();

	bool canRemoveOnDie();

	int32_t getSignetBurstedCount() const { return signetBurstedCount.get(); }

	void setSignetBurstedCount(int32_t value) { signetBurstedCount.set(value); }

	EffectResult getEffectResult() const { return effectResult.get(); }

	void setEffectResult(EffectResult value) { effectResult.set(value); }

	bool isEndedByTime() const { return endedByTime.get(); }

	int32_t getMpAbsorbed() const { return mpAbsorbed.get(); }

	void setMpAbsorbed(int32_t value) { mpAbsorbed.set(value); }

	int32_t getMpShieldSkillId() const { return mpShieldSkillId.get(); }

	void setMpShieldSkillId(int32_t value) { mpShieldSkillId.set(value); }

	std::unordered_set<effect::EffectType> getPossibleConflictEffectTypes();

	runtime::Ptr<Effect> getDesignatedDispelEffect() const { return designatedDispelEffect.get(); }

	bool setDesignatedDispelEffect(Effect& effect);

	void resetDesignatedDispelEffect();

	bool isSubEffect() const { return isSubEffect_; }

	bool tryActivateGodstone();
};

} // namespace aion::gameserver::skillengine::model
