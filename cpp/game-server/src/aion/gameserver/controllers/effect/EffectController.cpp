#include "aion/gameserver/controllers/effect/EffectController.h"

#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/skillengine/model/Effect.h"

namespace aion::gameserver::controllers::effect {

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
	AION_UNPORTED();
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
	AION_UNPORTED();
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
	AION_UNPORTED();
}

std::vector<runtime::Ptr<skillengine::model::Effect>> EffectController::filterEffects(
	runtime::LinkedHashMap<std::string, runtime::Ref<skillengine::model::Effect>>& effectMap,
	const std::function<bool(skillengine::model::Effect&)>& filter) {
	AION_UNPORTED();
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
	AION_UNPORTED();
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
	AION_UNPORTED();
}

void EffectController::removeAllEffects(bool logout) {
	AION_UNPORTED();
}

bool EffectController::canRemoveOnDie(skillengine::model::Effect& effect) {
	AION_UNPORTED();
}

bool EffectController::isUnderFear() {
	AION_UNPORTED();
}

bool EffectController::isConfused() {
	AION_UNPORTED();
}

std::vector<runtime::Ptr<skillengine::model::Effect>> EffectController::getAbnormalEffects() {
	AION_UNPORTED();
}

std::vector<runtime::Ptr<skillengine::model::Effect>> EffectController::getAbnormalEffectsToTargetSlot(int32_t slot) {
	AION_UNPORTED();
}

std::vector<runtime::Ptr<skillengine::model::Effect>> EffectController::getAbnormalEffectsToShow() {
	AION_UNPORTED();
}

void EffectController::setAbnormal(skillengine::effect::AbnormalState state) {
	AION_UNPORTED();
}

void EffectController::unsetAbnormal(skillengine::effect::AbnormalState state) {
	AION_UNPORTED();
}

bool EffectController::isAbnormalSet(skillengine::effect::AbnormalState state) {
	AION_UNPORTED();
}

bool EffectController::isInAnyAbnormalState(skillengine::effect::AbnormalState state) {
	AION_UNPORTED();
}

bool EffectController::isEmpty() {
	AION_UNPORTED();
}

void EffectController::resetDesignatedDispelEffect(skillengine::model::Effect& effect) {
	AION_UNPORTED();
}

void EffectController::clearEffectMapsWithoutNotify() {
	AION_UNPORTED();
}

} // namespace aion::gameserver::controllers::effect
