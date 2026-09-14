#pragma once

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "aion/gameserver/runtime/collections/HashMap.h"
#include "aion/gameserver/runtime/collections/Rc.h"
#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/lifetime/Parts.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/sync/StampedLock.h"
#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/skillengine/effect/fwd.h"
#include "aion/gameserver/skillengine/model/fwd.h"

namespace aion::gameserver::controllers::effect {

/**
 * Hub header (docs/design/hub-headers.md). A part of Creature (`PartSlot<EffectController>`, Java `setEffectController(new
 * EffectController(this))`), bound to its owner in the constructor.
 * <p>
 * C++ notes: both effect maps are LinkedHashMap shims, so the private helpers take the live map as `LinkedHashMap&` like Java's
 * `Map<String, Effect>` parameters.
 * - passiveEffectMap is null until the first passive effect: null stands for Java's initial `Collections.emptyMap()` (an immutable, lock-free
 *   empty map compared by identity in getPassiveEffectMap). Readers treat null as empty (getMapForEffect(template, false) returns nullptr,
 *   the callers skip the loop or lookup); getPassiveEffectMap(true) creates the owned map once (double-checked under SYNCHRONIZED(*this)) and
 *   never replaces it afterwards, so a borrowed map reference stays valid for the controller's lifetime. A shared sentinel map is not used: it
 *   would serialize every creature on one Monitor and turn Java's UnsupportedOperationException on a wrong put into a write visible to all.
 * - EffectType and DispelSlotType arguments that Java passes as null (removeByDispelSlotType) are `std::optional`.
 *
 * @author ATracer, Wakizashi, Sippolo, Cheatkiller, Neon
 */
class EffectController : public runtime::OwnedPart {
private:
	runtime::OwnerRef<model::gameobjects::Creature> owner;
	runtime::StampedLock lock{};
	/** null: Java's initial Collections.emptyMap() (see the class comment) */
	runtime::Field<runtime::Ref<runtime::RcLinkedHashMap<std::string, runtime::Ref<skillengine::model::Effect>>>> passiveEffectMap{};
	runtime::LinkedHashMap<std::string, runtime::Ref<skillengine::model::Effect>> abnormalEffectMap{};
	runtime::Field<int32_t> abnormals{};

public:
	explicit EffectController(model::gameobjects::Creature& owner);
	~EffectController() override;

	/** Java: Creature getOwner(); PlayerEffectController narrows it to Player& (cast-only override, §8.2). */
	model::gameobjects::Creature& getOwner() const { return owner; }

	virtual void addEffect(skillengine::model::Effect& nextEffect);

protected:
	void put(skillengine::model::Effect& nextEffect);

private:
	void put(runtime::LinkedHashMap<std::string, runtime::Ref<skillengine::model::Effect>>& mapToUpdate, skillengine::model::Effect& nextEffect);

	void endConflictedEffect(runtime::LinkedHashMap<std::string, runtime::Ref<skillengine::model::Effect>>& effectMap,
		skillengine::model::Effect& newEffect);

	bool searchConflict(runtime::LinkedHashMap<std::string, runtime::Ref<skillengine::model::Effect>>& mapToUpdate,
		skillengine::model::Effect& nextEffect);

public:
	/**
	 * @return True if {@code newEffectTemplate} is in conflict with another existing effect.
	 */
	bool isConflicting(skillengine::model::Effect& newEffect);

private:
	static bool canConflict(skillengine::model::Effect& e1, skillengine::model::Effect& e2);

	bool checkExtraEffect(runtime::LinkedHashMap<std::string, runtime::Ref<skillengine::model::Effect>>& effectMap,
		skillengine::model::Effect& nextEffect);

	std::vector<runtime::Ptr<skillengine::model::Effect>> getAuraEffects();

	std::vector<runtime::Ptr<skillengine::model::Effect>> getNoShowToggleEffectsExceptAuras();

	void checkEffectCooldownId(skillengine::model::Effect& effect);

	/** @return the live map (passiveEffectMap, created if needed, or abnormalEffectMap) */
	runtime::LinkedHashMap<std::string, runtime::Ref<skillengine::model::Effect>>& getMapForEffect(skillengine::model::Effect& effect);

	/** @return the live map (passiveEffectMap or abnormalEffectMap); nullptr for a passive template while no passive map exists and !initialize
	 *          (Java: the empty map) */
	runtime::LinkedHashMap<std::string, runtime::Ref<skillengine::model::Effect>>* getMapForEffect(const skillengine::model::SkillTemplate* template_,
		bool initialize);

	/** @return the passive effect map, null (Java: Collections.emptyMap()) until the first call with initialize creates it */
	runtime::Ptr<runtime::RcLinkedHashMap<std::string, runtime::Ref<skillengine::model::Effect>>> getPassiveEffectMap(bool initialize);

public:
	runtime::Ptr<skillengine::model::Effect> getAbnormalEffect(std::string_view stack);

	bool hasAbnormalEffect(const std::function<bool(skillengine::model::Effect&)>& predicate);

	bool hasAbnormalEffect(int32_t skillId);

	bool isUnderNormalShield();

	/** effect: null broadcasts all slots (removeAllEffects, DAOs) */
	void broadCastEffects(runtime::Ptr<skillengine::model::Effect> effect);

	virtual void clearEffect(skillengine::model::Effect& effect, bool broadCastEffects);

	/** @throws NullPointerException if the skill does not exist; @return null if no such effect is active */
	runtime::Ptr<skillengine::model::Effect> findBySkillId(int32_t skillId);

	void removeEffect(int32_t skillId);

	void removeHideEffects();

	void removePetOrderUnSummonEffects();

	void removeParalyzeEffects();

	void removeStunEffects();

	/**
	 * Removes Transform effects from owner. For more info see {@link TransformEffect} it doesn't remove Avatar transforms (TODO find out on retail)
	 */
	void removeTransformEffects();

	void removeInstanceEffects();

private:
	void removeEffects(runtime::LinkedHashMap<std::string, runtime::Ref<skillengine::model::Effect>>& mapForEffect,
		const std::function<bool(skillengine::model::Effect&)>& predicate);

	runtime::Ptr<skillengine::model::Effect> findFirstEffect(runtime::LinkedHashMap<std::string, runtime::Ref<skillengine::model::Effect>>& effectMap,
		const std::function<bool(skillengine::model::Effect&)>& filter);

public:
	std::vector<runtime::Ptr<skillengine::model::Effect>> getAllEffects();

private:
	std::vector<runtime::Ptr<skillengine::model::Effect>> filterEffects(
		runtime::LinkedHashMap<std::string, runtime::Ref<skillengine::model::Effect>>& effectMap,
		const std::function<bool(skillengine::model::Effect&)>& filter);

public:
	/**
	 * Removes all effects by given dispel slot type
	 */
	void removeByDispelSlotType(skillengine::model::DispelSlotType dispelSlotType);

	bool removeByEffectId(int32_t effectId, int32_t dispelLevel, int32_t power);

private:
	bool removeByEffectId(runtime::LinkedHashMap<std::string, runtime::Ref<skillengine::model::Effect>>& effectMap, int32_t effectId,
		int32_t dispelLevel, int32_t power);

public:
	/** effectType: null matches every effect type (removeByDispelSlotType); dispelSlotType: null matches every slot */
	void removeByDispelEffect(std::optional<skillengine::effect::EffectType> effectType,
		std::optional<skillengine::model::DispelSlotType> dispelSlotType, int32_t count, int32_t dispelLevel, int32_t power);

private:
	int32_t removeByDispelEffect(runtime::LinkedHashMap<std::string, runtime::Ref<skillengine::model::Effect>>& effectMap,
		std::optional<skillengine::effect::EffectType> effectType, std::optional<skillengine::model::DispelSlotType> dispelSlotType, int32_t count,
		int32_t dispelLevel, int32_t power);

public:
	/**
	 * Calculates effects that will be removed and reduces their power. Used only in DispelBuffCounterAtkEffect
	 *
	 * @return number of effects that will be removed
	 */
	int32_t calculateBuffsOrEffectorDebuffsToRemove(skillengine::model::Effect& effect, int32_t count, int32_t dispelLevel, int32_t power);

	void removeEffectByDispelCat(skillengine::model::DispelCategoryType dispelCat, skillengine::model::SkillTargetSlot targetSlot, int32_t count,
		int32_t dispelLevel, int32_t power);

private:
	bool isNoShowToggle(skillengine::model::Effect& effect);

	bool isDispellable(skillengine::model::Effect& effect);

public:
	void dispelBuffCounterAtkEffect(skillengine::model::Effect& effect);

private:
	bool isRemovableEffect(skillengine::model::Effect& effect);

	bool removePower(skillengine::model::Effect& effect, int32_t power);

public:
	/**
	 * Removes all effects from controllers and ends them appropriately Passive effect will not be removed
	 */
	virtual void removeAllEffects();

	virtual void removeAllEffects(bool logout);

protected:
	virtual bool canRemoveOnDie(skillengine::model::Effect& effect);

public:
	bool isUnderFear();

	bool isConfused();

	/**
	 * @return copy of abnormals list
	 */
	std::vector<runtime::Ptr<skillengine::model::Effect>> getAbnormalEffects();

	/**
	 * @return list of effects to display as top icons
	 */
	std::vector<runtime::Ptr<skillengine::model::Effect>> getAbnormalEffectsToTargetSlot(int32_t slot);

	/**
	 * @return list of effects to display as top icons
	 */
	std::vector<runtime::Ptr<skillengine::model::Effect>> getAbnormalEffectsToShow();

	void setAbnormal(skillengine::effect::AbnormalState state);

	void unsetAbnormal(skillengine::effect::AbnormalState state);

	/**
	 * Used for checking unique abnormal states
	 */
	bool isAbnormalSet(skillengine::effect::AbnormalState state);

	/**
	 * Used for compound abnormal state checks
	 */
	bool isInAnyAbnormalState(skillengine::effect::AbnormalState state);

	int32_t getAbnormals() const { return abnormals.get(); }

	bool isEmpty();

	void resetDesignatedDispelEffect(skillengine::model::Effect& effect);

	/**
	 * C++ only (LogoutBreakers D3, cycles.toml EffectController.abnormalEffectMap/passiveEffectMap): empties both effect maps under the write
	 * lock without ending the effects, broadcasting or notifying anyone; a timed effect still ends through its end task. The passive map object
	 * stays (see the class comment). Idempotent. Not noexcept: the lock may throw (lock order); LogoutBreakers logs a throwing step.
	 */
	void clearEffectMapsWithoutNotify();
};

} // namespace aion::gameserver::controllers::effect
