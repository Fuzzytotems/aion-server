#include "aion/gameserver/controllers/effect/EffectController.h"

#include "aion/gameserver/controllers/ControllerSupport.h"
#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/network/aion/serverpackets/SM_ABNORMAL_EFFECT.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/runtime/base/Finally.h"
#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/skillengine/effect/AbnormalState.h"
#include "aion/gameserver/skillengine/model/Effect.h"
#include "aion/gameserver/skillengine/model/SkillTargetSlotInfo.h"
#include "aion/gameserver/utils/PacketSendUtility.h"

namespace aion::gameserver::controllers::effect {

namespace {

using skillengine::effect::AbnormalState;
using skillengine::model::Effect;
using skillengine::model::SkillTargetSlot;

/** Java: effect.getTargetSlot() (a NullPointerException where Java dereferences a null slot) */
SkillTargetSlot targetSlotOf(Effect& effect) {
	std::optional<SkillTargetSlot> slot = effect.getTargetSlot();
	if (!slot)
		throw runtime::NullPointerException("effect target slot is null");
	return *slot;
}

/** Java: AbnormalState.getId() - skillengine/effect/AbnormalStateInfo.h (P5-03) does not exist yet: the controllers' stand-in */
constexpr int32_t abnormalStateId(AbnormalState state) noexcept {
	return detail::getAbnormalStateId(state);
}

} // namespace

// passiveEffectMap starts null: Java's Collections.emptyMap() (EffectController.h class comment)
EffectController::EffectController(model::gameobjects::Creature& ownerValue) : OwnedPart(ownerValue), owner(ownerValue) {
}

EffectController::~EffectController() = default;

void EffectController::addEffect(skillengine::model::Effect& nextEffect) {
	AION_UNPORTED();
}

void EffectController::put(skillengine::model::Effect& nextEffect) {
	AION_UNPORTED();
}

void EffectController::put(runtime::LinkedHashMap<std::string, runtime::Ref<skillengine::model::Effect>>& mapToUpdate,
	skillengine::model::Effect& nextEffect) {
	AION_UNPORTED();
}

void EffectController::endConflictedEffect(runtime::LinkedHashMap<std::string, runtime::Ref<skillengine::model::Effect>>& effectMap,
	skillengine::model::Effect& newEffect) {
	AION_UNPORTED();
}

bool EffectController::searchConflict(runtime::LinkedHashMap<std::string, runtime::Ref<skillengine::model::Effect>>& mapToUpdate,
	skillengine::model::Effect& nextEffect) {
	AION_UNPORTED();
}

bool EffectController::isConflicting(skillengine::model::Effect& newEffect) {
	AION_UNPORTED();
}

bool EffectController::canConflict(skillengine::model::Effect& e1, skillengine::model::Effect& e2) {
	AION_UNPORTED();
}

bool EffectController::checkExtraEffect(runtime::LinkedHashMap<std::string, runtime::Ref<skillengine::model::Effect>>& effectMap,
	skillengine::model::Effect& nextEffect) {
	AION_UNPORTED();
}

std::vector<runtime::Ptr<skillengine::model::Effect>> EffectController::getAuraEffects() {
	AION_UNPORTED();
}

std::vector<runtime::Ptr<skillengine::model::Effect>> EffectController::getNoShowToggleEffectsExceptAuras() {
	AION_UNPORTED();
}

void EffectController::checkEffectCooldownId(skillengine::model::Effect& effect) {
	AION_UNPORTED();
}

runtime::LinkedHashMap<std::string, runtime::Ref<skillengine::model::Effect>>& EffectController::getMapForEffect(skillengine::model::Effect& effect) {
	AION_UNPORTED();
}

runtime::LinkedHashMap<std::string,
	runtime::Ref<skillengine::model::Effect>>* EffectController::getMapForEffect(const skillengine::model::SkillTemplate* template_,
		bool initialize) {
	AION_UNPORTED();
}

runtime::Ptr<runtime::RcLinkedHashMap<std::string, runtime::Ref<skillengine::model::Effect>>> EffectController::getPassiveEffectMap(bool initialize) {
	if (initialize && !passiveEffectMap.get()) {
		SYNCHRONIZED(*this) {
			if (!passiveEffectMap.get())
				passiveEffectMap = runtime::RcLinkedHashMap<std::string, runtime::Ref<Effect>>::create(AION_LOCK_CLASS(EffectController::passiveEffectMap));
		}
	}
	return passiveEffectMap.get();
}

runtime::Ptr<skillengine::model::Effect> EffectController::getAbnormalEffect(std::string_view stack) {
	AION_UNPORTED();
}

bool EffectController::hasAbnormalEffect(const std::function<bool(skillengine::model::Effect&)>& predicate) {
	AION_UNPORTED();
}

bool EffectController::hasAbnormalEffect(int32_t skillId) {
	AION_UNPORTED();
}

bool EffectController::isUnderNormalShield() {
	AION_UNPORTED();
}

void EffectController::broadCastEffects(runtime::Ptr<skillengine::model::Effect> effect) {
	int32_t slot = effect ? getId(targetSlotOf(*effect)) : skillengine::model::SKILL_TARGET_SLOT_FULLSLOTS;
	std::vector<runtime::Ptr<Effect>> effects = getAbnormalEffects();
	utils::PacketSendUtility::broadcastPacket(getOwner(), network::aion::serverpackets::SM_ABNORMAL_EFFECT(getOwner(), abnormals.get(), effects, slot));
}

void EffectController::clearEffect(skillengine::model::Effect& effect, bool value) {
	AION_UNPORTED();
}

runtime::Ptr<skillengine::model::Effect> EffectController::findBySkillId(int32_t skillId) {
	AION_UNPORTED();
}

void EffectController::removeEffect(int32_t skillId) {
	AION_UNPORTED();
}

void EffectController::removeHideEffects() {
	AION_UNPORTED();
}

void EffectController::removePetOrderUnSummonEffects() {
	AION_UNPORTED();
}

void EffectController::removeParalyzeEffects() {
	AION_UNPORTED();
}

void EffectController::removeStunEffects() {
	AION_UNPORTED();
}

void EffectController::removeTransformEffects() {
	AION_UNPORTED();
}

void EffectController::removeInstanceEffects() {
	AION_UNPORTED();
}

void EffectController::removeEffects(runtime::LinkedHashMap<std::string, runtime::Ref<skillengine::model::Effect>>& mapForEffect,
	const std::function<bool(skillengine::model::Effect&)>& predicate) {
	AION_UNPORTED();
}

runtime::Ptr<skillengine::model::Effect> EffectController::findFirstEffect(
	runtime::LinkedHashMap<std::string, runtime::Ref<skillengine::model::Effect>>& effectMap,
	const std::function<bool(skillengine::model::Effect&)>& filter) {
	AION_UNPORTED();
}

std::vector<runtime::Ptr<skillengine::model::Effect>> EffectController::getAllEffects() {
	int64_t stamp = lock.readLock();
	auto unlock = runtime::finally([this, stamp]() { lock.unlockRead(stamp); });
	std::vector<runtime::Ptr<Effect>> effects = abnormalEffectMap.values();
	if (runtime::Ptr<runtime::RcLinkedHashMap<std::string, runtime::Ref<Effect>>> passiveEffects = passiveEffectMap.get()) {
		for (runtime::Ptr<Effect> effect : passiveEffects->values())
			effects.push_back(effect);
	}
	return effects;
}

std::vector<runtime::Ptr<skillengine::model::Effect>> EffectController::filterEffects(
	runtime::LinkedHashMap<std::string, runtime::Ref<skillengine::model::Effect>>& effectMap,
	const std::function<bool(skillengine::model::Effect&)>& filter) {
	std::vector<runtime::Ptr<Effect>> effects;
	int64_t stamp = lock.readLock();
	auto unlock = runtime::finally([this, stamp]() { lock.unlockRead(stamp); });
	for (runtime::Ptr<Effect> effect : effectMap.values()) {
		if (filter(*effect))
			effects.push_back(effect);
	}
	return effects;
}

void EffectController::removeByDispelSlotType(skillengine::model::DispelSlotType dispelSlotType) {
	AION_UNPORTED();
}

bool EffectController::removeByEffectId(int32_t effectId, int32_t dispelLevel, int32_t power) {
	AION_UNPORTED();
}

bool EffectController::removeByEffectId(runtime::LinkedHashMap<std::string, runtime::Ref<skillengine::model::Effect>>& effectMap, int32_t effectId,
	int32_t dispelLevel, int32_t power) {
	AION_UNPORTED();
}

void EffectController::removeByDispelEffect(std::optional<skillengine::effect::EffectType> effectType,
	std::optional<skillengine::model::DispelSlotType> dispelSlotType, int32_t count, int32_t dispelLevel, int32_t power) {
	AION_UNPORTED();
}

int32_t EffectController::removeByDispelEffect(runtime::LinkedHashMap<std::string, runtime::Ref<skillengine::model::Effect>>& effectMap,
	std::optional<skillengine::effect::EffectType> effectType, std::optional<skillengine::model::DispelSlotType> dispelSlotType, int32_t count,
	int32_t dispelLevel, int32_t power) {
	AION_UNPORTED();
}

int32_t EffectController::calculateBuffsOrEffectorDebuffsToRemove(skillengine::model::Effect& effect, int32_t count, int32_t dispelLevel,
	int32_t power) {
	AION_UNPORTED();
}

void EffectController::removeEffectByDispelCat(skillengine::model::DispelCategoryType dispelCat, skillengine::model::SkillTargetSlot targetSlot,
	int32_t count, int32_t dispelLevel, int32_t power) {
	AION_UNPORTED();
}

bool EffectController::isNoShowToggle(skillengine::model::Effect& effect) {
	return effect.getTargetSlot() == SkillTargetSlot::NOSHOW && effect.isToggle();
}

bool EffectController::isDispellable(skillengine::model::Effect& effect) {
	AION_UNPORTED();
}

void EffectController::dispelBuffCounterAtkEffect(skillengine::model::Effect& effect) {
	AION_UNPORTED();
}

bool EffectController::isRemovableEffect(skillengine::model::Effect& effect) {
	AION_UNPORTED();
}

bool EffectController::removePower(skillengine::model::Effect& effect, int32_t power) {
	AION_UNPORTED();
}

void EffectController::removeAllEffects() {
	removeAllEffects(false);
}

void EffectController::removeAllEffects(bool logout) {
	std::vector<runtime::Ptr<Effect>> effects;
	if (logout) { // remove all effects on logout
		effects = getAllEffects();
	} else {
		effects = filterEffects(abnormalEffectMap, [this](Effect& effect) { return canRemoveOnDie(effect); });
	}
	for (const runtime::Ptr<Effect>& effect : effects) // end outside lock so broadcasting effects can't cause deadlocks
		effect->endEffect(false);
	if (!logout)
		broadCastEffects(nullptr);
}

bool EffectController::canRemoveOnDie(skillengine::model::Effect& effect) {
	return effect.canRemoveOnDie();
}

bool EffectController::isUnderFear() {
	return isAbnormalSet(AbnormalState::FEAR);
}

bool EffectController::isConfused() {
	return isAbnormalSet(AbnormalState::CONFUSE);
}

std::vector<runtime::Ptr<skillengine::model::Effect>> EffectController::getAbnormalEffects() {
	return filterEffects(abnormalEffectMap, [this](Effect& e) { return !isNoShowToggle(e); });
}

std::vector<runtime::Ptr<skillengine::model::Effect>> EffectController::getAbnormalEffectsToTargetSlot(int32_t slot) {
	AION_UNPORTED();
}

std::vector<runtime::Ptr<skillengine::model::Effect>> EffectController::getAbnormalEffectsToShow() {
	return filterEffects(abnormalEffectMap, [](Effect& e) { return e.getTargetSlot() != SkillTargetSlot::NOSHOW; });
}

void EffectController::setAbnormal(skillengine::effect::AbnormalState state) {
	AION_UNPORTED();
}

void EffectController::unsetAbnormal(skillengine::effect::AbnormalState state) {
	AION_UNPORTED();
}

bool EffectController::isAbnormalSet(skillengine::effect::AbnormalState state) {
	if (state == AbnormalState::NONE)
		return abnormals.get() == 0;
	return (abnormals.get() & abnormalStateId(state)) == abnormalStateId(state);
}

bool EffectController::isInAnyAbnormalState(skillengine::effect::AbnormalState state) {
	if (state == AbnormalState::NONE)
		return abnormals.get() == 0;
	return (abnormals.get() & abnormalStateId(state)) != 0;
}

bool EffectController::isEmpty() {
	return abnormalEffectMap.isEmpty();
}

void EffectController::resetDesignatedDispelEffect(skillengine::model::Effect& effect) {
	AION_UNPORTED();
}

// lint: L7 C++-only breaker (no Java body): it takes the write lock like Java's writers of the effect maps
void EffectController::clearEffectMapsWithoutNotify() {
	int64_t stamp = lock.writeLock();
	auto unlock = runtime::finally([this, stamp]() { lock.unlockWrite(stamp); });
	abnormalEffectMap.clear();
	if (runtime::Ptr<runtime::RcLinkedHashMap<std::string, runtime::Ref<Effect>>> passiveEffects = passiveEffectMap.get())
		passiveEffects->clear();
}

} // namespace aion::gameserver::controllers::effect
