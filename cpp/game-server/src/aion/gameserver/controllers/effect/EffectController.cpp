#include "aion/gameserver/controllers/effect/EffectController.h"

#include <algorithm>
#include <cstddef>

#include "aion/gameserver/controllers/ControllerSupport.h"
#include "aion/gameserver/controllers/ObserveController.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/SkillData.h"
#include "aion/gameserver/model/EmotionType.h"
#include "aion/gameserver/model/PlayerClass.h"
#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/gameobjects/state/CreatureState.h"
#include "aion/gameserver/network/aion/serverpackets/SM_ABNORMAL_EFFECT.h"
#include "aion/gameserver/network/aion/serverpackets/SM_EMOTION.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/runtime/base/Finally.h"
#include "aion/gameserver/skillengine/effect/AbnormalState.h"
#include "aion/gameserver/skillengine/effect/EffectTemplate.h"
#include "aion/gameserver/skillengine/effect/EffectType.h"
#include "aion/gameserver/skillengine/effect/HideEffect.h"
#include "aion/gameserver/skillengine/effect/SilenceEffect.h"
#include "aion/gameserver/skillengine/effect/TransformEffect.h"
#include "aion/gameserver/skillengine/model/DispelCategoryType.h"
#include "aion/gameserver/skillengine/model/DispelSlotType.h"
#include "aion/gameserver/skillengine/model/Effect.h"
#include "aion/gameserver/skillengine/model/EffectResult.h"
#include "aion/gameserver/skillengine/model/ShieldType.h"
#include "aion/gameserver/skillengine/model/SkillSubType.h"
#include "aion/gameserver/skillengine/model/SkillTargetSlotInfo.h"
#include "aion/gameserver/skillengine/model/SkillTemplate.h"
#include "aion/gameserver/skillengine/model/TransformType.h"
#include "aion/gameserver/utils/PacketSendUtility.h"

namespace aion::gameserver::controllers::effect {

namespace {

using skillengine::effect::AbnormalState;
using skillengine::effect::EffectTemplate;
using skillengine::model::DispelCategoryType;
using skillengine::model::Effect;
using skillengine::model::SkillSubType;
using skillengine::model::SkillTargetSlot;

using EffectMap = runtime::LinkedHashMap<std::string, runtime::Ref<Effect>>;

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

/** Java: `a instanceof SilenceEffect && b instanceof SilenceEffect` (EffectController.java:141, :179) */
bool bothSilence(const EffectTemplate* a, const EffectTemplate* b) {
	return dynamic_cast<const skillengine::effect::SilenceEffect*>(a) != nullptr && dynamic_cast<const skillengine::effect::SilenceEffect*>(b) != nullptr;
}

} // namespace

// passiveEffectMap starts null: Java's Collections.emptyMap() (EffectController.h class comment)
EffectController::EffectController(model::gameobjects::Creature& ownerValue) : OwnedPart(ownerValue), owner(ownerValue) {
}

EffectController::~EffectController() = default;

void EffectController::addEffect(skillengine::model::Effect& nextEffect) {
	EffectMap& mapToUpdate = getMapForEffect(nextEffect);

	bool useEffectId = true;
	if (nextEffect.isPassive()) {
		runtime::Ptr<Effect> existingEffect;
		{
			int64_t stamp = lock.readLock();
			auto unlock = runtime::finally([this, stamp]() { lock.unlockRead(stamp); });
			existingEffect = mapToUpdate.get(nextEffect.getStack());
		}
		if (existingEffect && existingEffect->isPassive()) {
			// check stack level
			if (existingEffect->getSkillStackLvl() > nextEffect.getSkillStackLvl())
				return;

			// check skill level (when stack level same)
			if (existingEffect->getSkillStackLvl() == nextEffect.getSkillStackLvl() && existingEffect->getSkillLevel() > nextEffect.getSkillLevel())
				return;

			existingEffect->endEffect();
			useEffectId = false;
		}
	}

	if (useEffectId) {
		// idea here is that effects with same effectId shouldn't stack, effect with higher basic lvl takes priority
		if (searchConflict(mapToUpdate, nextEffect)) {
			if (!nextEffect.isPassive() && nextEffect.getTargetSlot() != SkillTargetSlot::DEBUFF)
				nextEffect.setEffectResult(skillengine::model::EffectResult::CONFLICT);
			return;
		}
	}
	endConflictedEffect(mapToUpdate, nextEffect);
	checkEffectCooldownId(nextEffect);

	// max 3 aura effects or 1 toggle skill in noshoweffects
	if (isNoShowToggle(nextEffect)) {
		size_t mts = nextEffect.getSkillSubType() == SkillSubType::CHANT ? 3 : 1;
		// Rangers & Riders are allowed to use 2 Toggle skills. There might be a pattern.
		if (runtime::Ptr<model::gameobjects::player::Player> player = runtime::as<model::gameobjects::player::Player>(nextEffect.getEffector());
			player && (player->getPlayerClass() == model::PlayerClass::RANGER || player->getPlayerClass() == model::PlayerClass::RIDER)) {
			mts = 2;
		}
		std::vector<runtime::Ptr<Effect>> filteredMap =
			nextEffect.getSkillSubType() == SkillSubType::CHANT ? getAuraEffects() : getNoShowToggleEffectsExceptAuras();
		if (filteredMap.size() >= mts)
			detail::listGetFirst(filteredMap)->endEffect();
	}

	// max 4 chants
	if (nextEffect.isChant()) {
		std::vector<runtime::Ptr<Effect>> chants = filterEffects(abnormalEffectMap, [this](Effect& e) { return !isNoShowToggle(e) && e.isChant(); });
		if (chants.size() >= 4)
			detail::listGetFirst(chants)->endEffect();
	}
	put(mapToUpdate, nextEffect);

	nextEffect.startEffect();

	if (!nextEffect.isPassive())
		broadCastEffects(runtime::Ptr<Effect>(nextEffect));
}

void EffectController::put(skillengine::model::Effect& nextEffect) {
	put(getMapForEffect(nextEffect), nextEffect);
}

void EffectController::put(runtime::LinkedHashMap<std::string, runtime::Ref<skillengine::model::Effect>>& mapToUpdate,
	skillengine::model::Effect& nextEffect) {
	int64_t stamp = lock.writeLock();
	auto unlock = runtime::finally([this, stamp]() { lock.unlockWrite(stamp); });
	mapToUpdate.put(nextEffect.getStack(), runtime::Ref<Effect>(nextEffect));
}

void EffectController::endConflictedEffect(runtime::LinkedHashMap<std::string, runtime::Ref<skillengine::model::Effect>>& effectMap,
	skillengine::model::Effect& newEffect) {
	int32_t conflictId = newEffect.getSkillTemplate()->getConflictId();
	if (conflictId == 0)
		return;
	runtime::Ptr<Effect> effectToEnd =
		findFirstEffect(effectMap, [conflictId](Effect& effect) { return effect.getSkillTemplate()->getConflictId() == conflictId; });
	if (effectToEnd)
		effectToEnd->endEffect();
}

bool EffectController::searchConflict(runtime::LinkedHashMap<std::string, runtime::Ref<skillengine::model::Effect>>& mapToUpdate,
	skillengine::model::Effect& nextEffect) {
	if (checkExtraEffect(mapToUpdate, nextEffect))
		return false;
	runtime::Ptr<Effect> effectToEnd;
	{
		int64_t stamp = lock.readLock();
		auto unlock = runtime::finally([this, stamp]() { lock.unlockRead(stamp); });
		// Java: a labelled `break mainLoop` out of three loops and a `return true` from the innermost one; the lambda answers which one happened
		enum class Outcome { NONE, KEEP_EXISTING, END_EXISTING };
		auto searchMainLoop = [&]() -> Outcome {
			for (runtime::Ptr<Effect> effect : mapToUpdate.values()) {
				if (!canConflict(*effect, nextEffect))
					continue;
				for (const EffectTemplate* et : effect->getEffectTemplates()) {
					if (et->getEffectId() == 0)
						continue;
					for (const EffectTemplate* et2 : nextEffect.getEffectTemplates()) {
						if (et2->getEffectId() == 0)
							continue;
						if ((et->getEffectId() == et2->getEffectId()) || bothSilence(et, et2)) {
							if (et->getBasicLvl() > et2->getBasicLvl()) {
								return Outcome::KEEP_EXISTING;
							} else {
								effectToEnd = effect;
								return Outcome::END_EXISTING;
							}
						}
					}
				}
			}
			return Outcome::NONE;
		};
		if (searchMainLoop() == Outcome::KEEP_EXISTING)
			return true;
	}
	if (effectToEnd)
		effectToEnd->endEffect(effectToEnd->getTargetSlot() != nextEffect.getTargetSlot());
	return false;
}

bool EffectController::isConflicting(skillengine::model::Effect& newEffect) {
	if (newEffect.isPassive() || newEffect.getTargetSlot() == SkillTargetSlot::DEBUFF)
		return false;
	EffectMap* mapForEffect = getMapForEffect(newEffect.getSkillTemplate(), false);
	if (mapForEffect == nullptr) // Java: the empty passive map (not reachable: a passive effect returned above)
		return false;
	int64_t stamp = lock.readLock();
	auto unlock = runtime::finally([this, stamp]() { lock.unlockRead(stamp); });
	for (runtime::Ptr<Effect> currentEffect : mapForEffect->values()) {
		if (!canConflict(*currentEffect, newEffect))
			continue;
		for (const EffectTemplate* newEffectTemplate : newEffect.getEffectTemplates()) {
			if (newEffectTemplate->getEffectId() == 0)
				continue;
			for (const EffectTemplate* currentEffectTemplate : currentEffect->getEffectTemplates()) {
				if (currentEffectTemplate->getEffectId() == 0)
					continue;
				if ((currentEffectTemplate->getEffectId() == newEffectTemplate->getEffectId()) || bothSilence(currentEffectTemplate, newEffectTemplate)) {
					if (currentEffectTemplate->getBasicLvl() > newEffectTemplate->getBasicLvl()
						&& dynamic_cast<const skillengine::effect::HideEffect*>(currentEffectTemplate) == nullptr) {
						return true;
					}
				}
			}
		}
	}
	return false;
}

bool EffectController::canConflict(skillengine::model::Effect& e1, skillengine::model::Effect& e2) {
	if (e1.getTargetSlot() == e2.getTargetSlot())
		return true;
	std::unordered_set<skillengine::effect::EffectType> types1 = e1.getPossibleConflictEffectTypes();
	std::unordered_set<skillengine::effect::EffectType> types2 = e2.getPossibleConflictEffectTypes();
	// Java: !Collections.disjoint(...)
	if (std::ranges::any_of(types1, [&types2](skillengine::effect::EffectType type) { return types2.contains(type); }))
		return true; // a few effect types can even conflict on differing target slots (example: Barricade of Steel and Holy Shield)
	return false;
}

bool EffectController::checkExtraEffect(runtime::LinkedHashMap<std::string, runtime::Ref<skillengine::model::Effect>>& effectMap,
	skillengine::model::Effect& nextEffect) {
	if (nextEffect.isPassive() || nextEffect.getDispelCategory() != DispelCategoryType::EXTRA)
		return false;
	runtime::Ptr<Effect> extraEffect = findFirstEffect(effectMap, [](Effect& effect) {
		return effect.getDispelCategory() == DispelCategoryType::EXTRA && !effect.getSkillTemplate()->getStack().starts_with("IDSEAL_BOSS_VRITRA_BUFF");
	});
	if (extraEffect) {
		extraEffect->endEffect();
		return true;
	}
	return false;
}

std::vector<runtime::Ptr<skillengine::model::Effect>> EffectController::getAuraEffects() {
	return filterEffects(abnormalEffectMap, [this](Effect& effect) { return isNoShowToggle(effect) && effect.getSkillSubType() == SkillSubType::CHANT; });
}

std::vector<runtime::Ptr<skillengine::model::Effect>> EffectController::getNoShowToggleEffectsExceptAuras() {
	return filterEffects(abnormalEffectMap, [this](Effect& effect) { return isNoShowToggle(effect) && effect.getSkillSubType() != SkillSubType::CHANT; });
}

void EffectController::checkEffectCooldownId(skillengine::model::Effect& effect) {
	if (effect.isPassive() || effect.getTargetSlot() == SkillTargetSlot::NOSHOW)
		return;
	int32_t cdId = effect.getSkillTemplate()->getCooldownId();
	if (cdId == 1)
		return;
	size_t size = 1;
	switch (cdId) {
		case 273: // Erosion & Flamecage
		case 353: // Lockdown & Dazing Severe Blow
			size = 2;
			break;
		case 6:
			size = 10;
			break;
	}
	std::vector<runtime::Ptr<Effect>> effects = filterEffects(abnormalEffectMap, [this, &effect, cdId](Effect& e) {
		return !isNoShowToggle(e) && e.getTargetSlot() == effect.getTargetSlot() && e.getSkillTemplate()->getCooldownId() == cdId;
	});
	if (effects.size() >= size)
		detail::listGetFirst(effects)->endEffect();
	// archer buffs
	if (cdId >= 2020 && cdId <= 2030) {
		int32_t count = 0;
		runtime::Ptr<Effect> toRemove;
		for (const runtime::Ptr<Effect>& eff : getAbnormalEffectsToShow()) {
			switch (eff->getSkillTemplate()->getCooldownId()) {
				case 2020: // Dodging
				case 2022: // Focused Shots
				case 2024: // Aiming
				case 2026: // Bestial Fury
				case 2028: // Hunter's Eye
				case 2030: // Strong Shots
					if (!toRemove)
						toRemove = eff;
					if (++count >= 2) {
						toRemove->endEffect();
						return;
					}
					break;
			}
		}
	}
}

runtime::LinkedHashMap<std::string, runtime::Ref<skillengine::model::Effect>>& EffectController::getMapForEffect(skillengine::model::Effect& effect) {
	// initialize = true never answers null (getPassiveEffectMap creates the map)
	return *getMapForEffect(effect.getSkillTemplate(), true);
}

runtime::LinkedHashMap<std::string,
	runtime::Ref<skillengine::model::Effect>>* EffectController::getMapForEffect(const skillengine::model::SkillTemplate* template_,
		bool initialize) {
	if (template_->isPassive()) {
		runtime::Ptr<runtime::RcLinkedHashMap<std::string, runtime::Ref<Effect>>> passiveEffects = getPassiveEffectMap(initialize);
		// the map object is never replaced once created (EffectController.h), so the reference outlives the borrow
		return passiveEffects ? static_cast<EffectMap*>(passiveEffects.operator->()) : nullptr;
	}
	return &abnormalEffectMap;
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
	int64_t stamp = lock.readLock();
	auto unlock = runtime::finally([this, stamp]() { lock.unlockRead(stamp); });
	return abnormalEffectMap.get(std::string(stack));
}

bool EffectController::hasAbnormalEffect(const std::function<bool(skillengine::model::Effect&)>& predicate) {
	return static_cast<bool>(findFirstEffect(abnormalEffectMap, predicate));
}

bool EffectController::hasAbnormalEffect(int32_t skillId) {
	return hasAbnormalEffect([skillId](Effect& effect) { return effect.getSkillId() == skillId; });
}

bool EffectController::isUnderNormalShield() {
	const int32_t normal = detail::shieldTypeId(skillengine::model::ShieldType::NORMAL);
	return hasAbnormalEffect([normal](Effect& e) { return (e.getShieldDefense() & normal) == normal; });
}

void EffectController::broadCastEffects(runtime::Ptr<skillengine::model::Effect> effect) {
	int32_t slot = effect ? getId(targetSlotOf(*effect)) : skillengine::model::SKILL_TARGET_SLOT_FULLSLOTS;
	std::vector<runtime::Ptr<Effect>> effects = getAbnormalEffects();
	utils::PacketSendUtility::broadcastPacket(getOwner(), network::aion::serverpackets::SM_ABNORMAL_EFFECT(getOwner(), abnormals.get(), effects, slot));
}

void EffectController::clearEffect(skillengine::model::Effect& effect, bool value) {
	EffectMap* effectMap = getMapForEffect(effect.getSkillTemplate(), false);
	if (effectMap != nullptr) { // Java: the empty passive map answers null for every get, so nothing is removed
		int64_t stamp = lock.writeLock();
		auto unlock = runtime::finally([this, stamp]() { lock.unlockWrite(stamp); });
		runtime::Ptr<Effect> oldEffect = effectMap->get(effect.getStack());
		if (oldEffect) {
			if (oldEffect.get() != &effect) // Java: !oldEffect.equals(effect) - Effect has identity equality
				return; // effect in map was already replaced by a newer one (e.g. when toggling many auras), so there's no need to re-broadcast
			effectMap->remove(effect.getStack());
		}
	}
	if (value)
		broadCastEffects(runtime::Ptr<Effect>(effect));
}

runtime::Ptr<skillengine::model::Effect> EffectController::findBySkillId(int32_t skillId) {
	const skillengine::model::SkillTemplate* skillTemplate = dataholders::DataManager::SKILL_DATA->getSkillTemplate(skillId);
	if (skillTemplate == nullptr)
		throw runtime::NullPointerException("Skill with ID " + std::to_string(skillId) + " does not exist");
	EffectMap* effectMap = getMapForEffect(skillTemplate, false);
	if (effectMap == nullptr) // Java: findFirstEffect over the empty passive map
		return nullptr;
	return findFirstEffect(*effectMap, [skillId](Effect& effect) { return effect.getSkillId() == skillId; });
}

void EffectController::removeEffect(int32_t skillId) {
	runtime::Ptr<Effect> effect = findBySkillId(skillId);
	if (effect)
		effect->endEffect();
}

void EffectController::removeHideEffects() {
	removeEffects(abnormalEffectMap, [](Effect& effect) { return effect.isHideEffect(); });
}

void EffectController::removePetOrderUnSummonEffects() {
	removeEffects(abnormalEffectMap, [](Effect& effect) { return effect.isPetOrderUnSummonEffect(); });
}

void EffectController::removeParalyzeEffects() {
	removeEffects(abnormalEffectMap, [](Effect& effect) { return effect.isParalyzeEffect(); });
}

void EffectController::removeStunEffects() {
	removeEffects(abnormalEffectMap, [](Effect& effect) { return effect.isStunEffect(); });
}

void EffectController::removeTransformEffects() {
	removeEffects(abnormalEffectMap, [](Effect& effect) {
		for (const EffectTemplate* et : effect.getEffectTemplates()) {
			const auto* transform = dynamic_cast<const skillengine::effect::TransformEffect*>(et);
			if (transform != nullptr && transform->getTransformType() != skillengine::model::TransformType::AVATAR)
				return true;
		}
		return false;
	});
}

void EffectController::removeInstanceEffects() {
	removeEffects(abnormalEffectMap, [](Effect& effect) {
		if (effect.getDispelCategory() == DispelCategoryType::NPC_BUFF)
			return true;
		for (const EffectTemplate* et : effect.getEffectTemplates()) {
			const auto* te = dynamic_cast<const skillengine::effect::TransformEffect*>(et);
			if (te != nullptr && te->getTransformType() == skillengine::model::TransformType::FORM1)
				return true;
		}
		return false;
	});
}

void EffectController::removeEffects(runtime::LinkedHashMap<std::string, runtime::Ref<skillengine::model::Effect>>& mapForEffect,
	const std::function<bool(skillengine::model::Effect&)>& predicate) {
	for (const runtime::Ptr<Effect>& effect : filterEffects(mapForEffect, predicate))
		effect->endEffect();
}

runtime::Ptr<skillengine::model::Effect> EffectController::findFirstEffect(
	runtime::LinkedHashMap<std::string, runtime::Ref<skillengine::model::Effect>>& effectMap,
	const std::function<bool(skillengine::model::Effect&)>& filter) {
	int64_t stamp = lock.readLock();
	auto unlock = runtime::finally([this, stamp]() { lock.unlockRead(stamp); });
	for (runtime::Ptr<Effect> effect : effectMap.values()) {
		if (filter(*effect))
			return effect;
	}
	return nullptr;
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
	removeByDispelEffect(std::nullopt, dispelSlotType, 255, 100, 100);
}

bool EffectController::removeByEffectId(int32_t effectId, int32_t dispelLevel, int32_t power) {
	return removeByEffectId(abnormalEffectMap, effectId, dispelLevel, power);
}

bool EffectController::removeByEffectId(runtime::LinkedHashMap<std::string, runtime::Ref<skillengine::model::Effect>>& effectMap, int32_t effectId,
	int32_t dispelLevel, int32_t power) {
	runtime::Ptr<Effect> effectToEnd = findFirstEffect(effectMap, [this, effectId, dispelLevel, power](Effect& effect) {
		return effect.getReqDispelLevel() <= dispelLevel && effect.containsEffectId(effectId) && removePower(effect, power);
	});
	if (effectToEnd) {
		effectToEnd->endEffect();
		return true;
	} else {
		return false;
	}
}

void EffectController::removeByDispelEffect(std::optional<skillengine::effect::EffectType> effectType,
	std::optional<skillengine::model::DispelSlotType> dispelSlotType, int32_t count, int32_t dispelLevel, int32_t power) {
	removeByDispelEffect(abnormalEffectMap, effectType, dispelSlotType, count, dispelLevel, power);
}

int32_t EffectController::removeByDispelEffect(runtime::LinkedHashMap<std::string, runtime::Ref<skillengine::model::Effect>>& effectMap,
	std::optional<skillengine::effect::EffectType> effectType, std::optional<skillengine::model::DispelSlotType> dispelSlotType, int32_t count,
	int32_t dispelLevel, int32_t power) {
	std::vector<runtime::Ptr<Effect>> effectsToEnd;
	{
		int64_t stamp = lock.readLock();
		auto unlock = runtime::finally([this, stamp]() { lock.unlockRead(stamp); });
		for (runtime::Ptr<Effect> effect : effectMap.values()) {
			// check count
			if (count == 0)
				break;
			if (effectType) {
				if (!effect->getSkillTemplate()->hasAnyEffect({*effectType}))
					continue;
				if (*effectType == skillengine::effect::EffectType::STUN && effect->getSkillId() == 11904) { // avatar skill irremovable by Remove Shock TODO: logic
					continue;
				}
			}
			if (dispelSlotType) {
				if (effect->getTargetSlot() != skillengine::model::of(*dispelSlotType))
					continue;
			}
			// check dispel level
			if (effect->getReqDispelLevel() > dispelLevel)
				continue;

			if (removePower(*effect, power))
				effectsToEnd.push_back(effect);
			// decrease count
			count = detail::sub(count, 1);
		}
	}
	for (const runtime::Ptr<Effect>& effect : effectsToEnd)
		effect->endEffect();
	return count;
}

int32_t EffectController::calculateBuffsOrEffectorDebuffsToRemove(skillengine::model::Effect& effect, int32_t count, int32_t dispelLevel,
	int32_t power) {
	int32_t dispelledEffectCount = 0;
	int64_t stamp = lock.readLock();
	auto unlock = runtime::finally([this, stamp]() { lock.unlockRead(stamp); });
	for (runtime::Ptr<Effect> ef : abnormalEffectMap.values()) {
		if (count == 0)
			break;
		if (!isDispellable(*ef))
			continue;
		DispelCategoryType dispelCat = ef->getDispelCategory();
		std::optional<SkillTargetSlot> targetSlot = ef->getSkillTemplate()->getTargetSlot();

		if (targetSlot != SkillTargetSlot::BUFF && targetSlot != SkillTargetSlot::DEBUFF && dispelCat != DispelCategoryType::ALL)
			continue;

		// remove only debuffs of the effector
		if (targetSlot == SkillTargetSlot::DEBUFF && !effect.getEffector()->equals(*ef->getEffector()))
			continue;

		switch (dispelCat) {
			case DispelCategoryType::ALL:
			case DispelCategoryType::BUFF: // DispelBuffCounterAtkEffect
				if (ef->getReqDispelLevel() <= dispelLevel && removePower(*ef, power)) {
					ef->setDesignatedDispelEffect(effect);
					dispelledEffectCount++;
					count = detail::sub(count, 1);
				}
				break;
			default:
				break;
		}
	}
	return dispelledEffectCount;
}

void EffectController::removeEffectByDispelCat(skillengine::model::DispelCategoryType dispelCat, skillengine::model::SkillTargetSlot targetSlot,
	int32_t count, int32_t dispelLevel, int32_t power) {
	std::vector<runtime::Ptr<Effect>> effectsToEnd;
	bool insufficientDispelPower = false;
	bool insufficientDispelLevel = false;
	{
		int64_t stamp = lock.readLock();
		auto unlock = runtime::finally([this, stamp]() { lock.unlockRead(stamp); });
		for (runtime::Ptr<Effect> effect : abnormalEffectMap.values()) {
			if (count == 0)
				break;
			if (!isDispellable(*effect))
				continue;
			if (effect->getTargetSlot() != targetSlot)
				continue;

			bool remove = false;
			switch (dispelCat) {
				case DispelCategoryType::ALL: // DispelDebuffEffect
					if ((effect->getDispelCategory() == DispelCategoryType::ALL || effect->getDispelCategory() == DispelCategoryType::DEBUFF_MENTAL
							|| effect->getDispelCategory() == DispelCategoryType::DEBUFF_PHYSICAL)
						&& effect->getReqDispelLevel() <= dispelLevel)
						remove = true;
					break;
				case DispelCategoryType::DEBUFF_MENTAL: // DispelDebuffMentalEffect
					if ((effect->getDispelCategory() == DispelCategoryType::ALL || effect->getDispelCategory() == DispelCategoryType::DEBUFF_MENTAL)
						&& effect->getReqDispelLevel() <= dispelLevel)
						remove = true;
					break;
				case DispelCategoryType::DEBUFF_PHYSICAL: // DispelDebuffPhysicalEffect
					if ((effect->getDispelCategory() == DispelCategoryType::ALL || effect->getDispelCategory() == DispelCategoryType::DEBUFF_PHYSICAL)
						&& effect->getReqDispelLevel() <= dispelLevel)
						remove = true;
					break;
				case DispelCategoryType::BUFF: // DispelBuffEffect or DispelBuffCounterAtkEffect
					if (effect->getDispelCategory() == DispelCategoryType::BUFF && effect->getReqDispelLevel() <= dispelLevel)
						remove = true;
					break;
				case DispelCategoryType::STUN:
					if (effect->getDispelCategory() == DispelCategoryType::STUN)
						remove = true;
					break;
				case DispelCategoryType::NPC_BUFF: // DispelNpcBuff
					if (effect->getDispelCategory() == DispelCategoryType::NPC_BUFF)
						remove = true;
					break;
				case DispelCategoryType::NPC_DEBUFF_PHYSICAL: // DispelNpcDebuff
					if (effect->getDispelCategory() == DispelCategoryType::NPC_DEBUFF_PHYSICAL)
						remove = true;
					break;
				default:
					break;
			}

			if (remove) {
				if (removePower(*effect, power)) {
					effectsToEnd.push_back(effect);
					count = detail::sub(count, 1);
				} else if (runtime::as<model::gameobjects::player::Player>(getOwner())) {
					insufficientDispelPower = true;
				}
			} else
				insufficientDispelLevel = true;
		}
	}
	for (const runtime::Ptr<Effect>& effect : effectsToEnd) // end outside lock so broadcasting effects can't cause deadlocks
		effect->endEffect();
	if (runtime::Ptr<model::gameobjects::player::Player> player = runtime::as<model::gameobjects::player::Player>(getOwner())) {
		if (insufficientDispelPower)
			utils::PacketSendUtility::sendPacket(*player, network::aion::serverpackets::SM_SYSTEM_MESSAGE::STR_MSG_NOT_ENOUGH_DISPELCOUNT());
		if (insufficientDispelLevel)
			utils::PacketSendUtility::sendPacket(*player, network::aion::serverpackets::SM_SYSTEM_MESSAGE::STR_MSG_NOT_ENOUGH_DISPELLEVEL());
	}
}

bool EffectController::isNoShowToggle(skillengine::model::Effect& effect) {
	return effect.getTargetSlot() == SkillTargetSlot::NOSHOW && effect.isToggle();
}

bool EffectController::isDispellable(skillengine::model::Effect& effect) {
	if (isNoShowToggle(effect))
		return false;
	if (effect.isSanctuaryEffect())
		return false;
	// skip effects that are about to be removed by another dispel effect
	if (effect.getDesignatedDispelEffect())
		return false;
	// effects with duration 86400000 cant be dispelled
	// TODO recheck
	if (effect.getDuration() >= 86400000 && !isRemovableEffect(effect))
		return false;
	return true;
}

void EffectController::dispelBuffCounterAtkEffect(skillengine::model::Effect& effect) {
	// Java: effect.equals(e.getDesignatedDispelEffect()) - identity, false for null
	std::vector<runtime::Ptr<Effect>> effectsToEnd =
		filterEffects(abnormalEffectMap, [&effect](Effect& e) { return e.getDesignatedDispelEffect().get() == &effect; });
	for (const runtime::Ptr<Effect>& ef : effectsToEnd) {
		ef->endEffect();
	}
}

bool EffectController::isRemovableEffect(skillengine::model::Effect& effect) {
	int32_t skillId = effect.getSkillId();
	switch (skillId) {
		case 20941:
		case 20942:
		case 19370:
		case 19371:
		case 19372:
		case 20530:
		case 20531:
		case 19345:
		case 19346:
		case 21438:
		case 21121:
		case 21124:
			// TODO
			return true;
		default:
			return false;
	}
}

bool EffectController::removePower(skillengine::model::Effect& effect, int32_t power) {
	return effect.removePower(power) <= 0;
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
	return filterEffects(abnormalEffectMap, [this, slot](Effect& e) { return !isNoShowToggle(e) && getId(targetSlotOf(e)) == slot; });
}

std::vector<runtime::Ptr<skillengine::model::Effect>> EffectController::getAbnormalEffectsToShow() {
	return filterEffects(abnormalEffectMap, [](Effect& e) { return e.getTargetSlot() != SkillTargetSlot::NOSHOW; });
}

void EffectController::setAbnormal(skillengine::effect::AbnormalState state) {
	getOwner().getObserveController()->notifyAbnormalSettedObservers(state);
	abnormals.set(abnormals.get() | abnormalStateId(state)); // java-race: `abnormals |= state.getId()` on a plain int

	// TODO move to observer?
	// if player is sitting when setting certain abnormal states, he is forcefully made to stand up
	if (runtime::Ptr<model::gameobjects::player::Player> player = runtime::as<model::gameobjects::player::Player>(getOwner());
		player && getOwner().isInState(model::gameobjects::state::CreatureState::RESTING)) {
		if (isInAnyAbnormalState(AbnormalState::AUTOMATICALLY_STANDUP)) {
			getOwner().unsetState(model::gameobjects::state::CreatureState::RESTING);
			utils::PacketSendUtility::broadcastPacket(*player, network::aion::serverpackets::SM_EMOTION(getOwner(), model::EmotionType::STAND), true);
		}
	}
}

void EffectController::unsetAbnormal(skillengine::effect::AbnormalState state) {
	int32_t count = 0;
	{
		int64_t stamp = lock.readLock();
		auto unlock = runtime::finally([this, stamp]() { lock.unlockRead(stamp); });
		for (runtime::Ptr<Effect> effect : abnormalEffectMap.values()) {
			if ((effect->getAbnormals() & abnormalStateId(state)) == abnormalStateId(state)) {
				if (++count == 2)
					return;
			}
		}
	}
	abnormals.set(abnormals.get() & ~abnormalStateId(state)); // java-race: `abnormals &= ~state.getId()` on a plain int
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
	int64_t stamp = lock.readLock();
	auto unlock = runtime::finally([this, stamp]() { lock.unlockRead(stamp); });
	for (runtime::Ptr<Effect> ef : abnormalEffectMap.values()) {
		if (ef->getDesignatedDispelEffect().get() == &effect) { // Java: effect.equals(ef.getDesignatedDispelEffect()), identity
			ef->resetDesignatedDispelEffect();
		}
	}
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
