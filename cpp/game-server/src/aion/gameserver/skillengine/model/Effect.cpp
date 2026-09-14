#include "aion/gameserver/skillengine/model/Effect.h"

#include <utility>

#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/runtime/base/TaskInfo.h"
#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/runtime/lifetime/TaskScope.h"
#include "aion/gameserver/skillengine/model/ActivationAttribute.h"
#include "aion/gameserver/skillengine/model/EffectReserved.h"
#include "aion/gameserver/skillengine/model/Skill.h"
#include "aion/gameserver/skillengine/model/SkillTemplate.h"

namespace aion::gameserver::skillengine::model {

// ------------------------------------------------------------------------------------------------------------------------------ ForceType

/**
 * Java class initialization: `DEFAULT = getInstance("")`, `MATERIAL_SKILL = getInstance("MATERIAL_SKILL")`. forceTypes is an inline static
 * member defined (and therefore initialized) before these definitions in this translation unit. getInstance runs in a STARTUP TaskScope because
 * ConcurrentHashMap operations need one (C2); nothing is borrowed, so the scope pins no reclamation. The objects are immortal (never deleted).
 */
const Effect_ForceType* const Effect_ForceType::DEFAULT = [] {
	runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::STARTUP));
	return getInstance("");
}();

const Effect_ForceType* const Effect_ForceType::MATERIAL_SKILL = [] {
	runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::STARTUP));
	return getInstance("MATERIAL_SKILL");
}();

Effect_ForceType::Effect_ForceType(std::string_view value) : name(value) {
}

const Effect_ForceType* Effect_ForceType::getInstance(std::string_view value) {
	// Java: forceTypes.computeIfAbsent(name, key -> new ForceType(name)) - interned, never deleted
	return forceTypes.computeIfAbsent(std::string(value), [value]() -> const Effect_ForceType* { return new Effect_ForceType(value); });
}

// ------------------------------------------------------------------------------------------------------------------------------ Effect

Effect::Effect(Skill& skillValue, runtime::Ptr<gameserver::model::gameobjects::Creature> effectedValue)
	: effector(skillValue.getEffector()), effected(effectedValue), skillTemplate(skillValue.getSkillTemplate()), skill(skillValue),
	  skillLevel(skillValue.getSkillLevel()), magicalCriticals(runtime::Array<bool>::make(4)), effectHate(skillValue.getHate()),
	  power(skillTemplate->getReqDispelCount()), isSubEffect_(false) {
	// Java: this(skill.getEffector(), effected, skill.getSkillTemplate(), skill.getSkillLevel(), null, null) - C++ cannot delegate because `skill`
	// is a const member; the delegated body has nothing to do for null magicalCriticalPositions.
	ActivationAttribute activationAttribute = skillTemplate->getActivationAttribute();
	if (activationAttribute == ActivationAttribute::ACTIVE || activationAttribute == ActivationAttribute::CHARGE)
		allowGodstoneActivation.set(true);
}

Effect::Effect(gameserver::model::gameobjects::Creature& effectorValue, runtime::Ptr<gameserver::model::gameobjects::Creature> effectedValue,
	const SkillTemplate* skillTemplateValue, int32_t skillLevelValue)
	: Effect(effectorValue, effectedValue, skillTemplateValue, skillLevelValue, std::nullopt, nullptr) {
}

Effect::Effect(gameserver::model::gameobjects::Creature& effectorValue, runtime::Ptr<gameserver::model::gameobjects::Creature> effectedValue,
	const SkillTemplate* skillTemplateValue, int32_t skillLevelValue, std::optional<int32_t> durationValue, const ForceType* forceTypeValue)
	: Effect(effectorValue, effectedValue, skillTemplateValue, skillLevelValue, durationValue, forceTypeValue, false, nullptr) {
}

Effect::Effect(gameserver::model::gameobjects::Creature& effectorValue, runtime::Ptr<gameserver::model::gameobjects::Creature> effectedValue,
	const SkillTemplate* skillTemplateValue, int32_t skillLevelValue, std::optional<int32_t> durationValue, const ForceType* forceTypeValue,
	bool isSubEffectValue, const std::unordered_set<int32_t>* magicalCriticalPositions)
	: effector(effectorValue), effected(effectedValue), skillTemplate(skillTemplateValue), skill(nullptr), skillLevel(skillLevelValue),
	  duration(durationValue), magicalCriticals(runtime::Array<bool>::make(4)), forceType(forceTypeValue),
	  power(skillTemplateValue->getReqDispelCount()), isSubEffect_(isSubEffectValue) {
	if (magicalCriticalPositions != nullptr)
		setMagicalCriticals(*magicalCriticalPositions);
}

Effect::~Effect() = default;

runtime::Ref<Effect> Effect::create(Skill& skillValue, runtime::Ptr<gameserver::model::gameobjects::Creature> effectedValue) {
	return runtime::makeRef<Effect>(skillValue, effectedValue);
}

runtime::Ref<Effect> Effect::create(gameserver::model::gameobjects::Creature& effectorValue, runtime::Ptr<gameserver::model::gameobjects::Creature> effectedValue,
	const SkillTemplate* skillTemplateValue, int32_t skillLevelValue) {
	return runtime::makeRef<Effect>(effectorValue, effectedValue, skillTemplateValue, skillLevelValue);
}

runtime::Ref<Effect> Effect::create(gameserver::model::gameobjects::Creature& effectorValue, runtime::Ptr<gameserver::model::gameobjects::Creature> effectedValue,
	const SkillTemplate* skillTemplateValue, int32_t skillLevelValue, std::optional<int32_t> durationValue, const ForceType* forceTypeValue) {
	return runtime::makeRef<Effect>(effectorValue, effectedValue, skillTemplateValue, skillLevelValue, durationValue, forceTypeValue);
}

runtime::Ref<Effect> Effect::create(gameserver::model::gameobjects::Creature& effectorValue, runtime::Ptr<gameserver::model::gameobjects::Creature> effectedValue,
	const SkillTemplate* skillTemplateValue, int32_t skillLevelValue, std::optional<int32_t> durationValue, const ForceType* forceTypeValue,
	bool isSubEffectValue, const std::unordered_set<int32_t>* magicalCriticalPositions) {
	return runtime::makeRef<Effect>(effectorValue, effectedValue, skillTemplateValue, skillLevelValue, durationValue, forceTypeValue, isSubEffectValue,
		magicalCriticalPositions);
}

int32_t Effect::getEffectorId() {
	AION_UNPORTED();
}

void Effect::setAbnormal(effect::AbnormalState state) {
	AION_UNPORTED();
}

int32_t Effect::getSkillId() {
	AION_UNPORTED();
}

std::string Effect::getSkillName() {
	AION_UNPORTED();
}

SkillSubType Effect::getSkillSubType() {
	AION_UNPORTED();
}

std::string Effect::getStack() {
	AION_UNPORTED();
}

int32_t Effect::getSkillStackLvl() {
	AION_UNPORTED();
}

SkillType Effect::getSkillType() {
	AION_UNPORTED();
}

int32_t Effect::getDuration() {
	AION_UNPORTED();
}

runtime::Ptr<gameserver::model::gameobjects::Creature> Effect::getEffected() {
	AION_UNPORTED();
}

bool Effect::isPassive() {
	AION_UNPORTED();
}

bool Effect::isPeriodic() {
	AION_UNPORTED();
}

void Effect::setPeriodicTask(runtime::FutureRef periodicTask, int32_t position) {
	AION_UNPORTED();
}

bool Effect::isMagicalCritical(int32_t position) {
	AION_UNPORTED();
}

void Effect::rollMagicalCritical(int32_t position, int32_t criticalProb) {
	AION_UNPORTED();
}

void Effect::reuseMagicalCritical(int32_t position) {
	AION_UNPORTED();
}

void Effect::resetMagicalCritical() {
	AION_UNPORTED();
}

void Effect::setMagicalCriticals(const std::unordered_set<int32_t>& positions) {
	AION_UNPORTED();
}

std::vector<const effect::EffectTemplate*> Effect::getEffectTemplates() {
	AION_UNPORTED();
}

bool Effect::isToggle() {
	AION_UNPORTED();
}

bool Effect::isChant() {
	AION_UNPORTED();
}

std::optional<SkillTargetSlot> Effect::getTargetSlot() {
	AION_UNPORTED();
}

int32_t Effect::getTargetSlotLevel() {
	AION_UNPORTED();
}

DispelCategoryType Effect::getDispelCategory() {
	AION_UNPORTED();
}

int32_t Effect::getReqDispelLevel() {
	AION_UNPORTED();
}

runtime::Ref<EffectReserved> Effect::getReserveds(int32_t position) {
	AION_UNPORTED();
}

void Effect::setReserveds(EffectReserved& er, bool overTimeEffect) {
	AION_UNPORTED();
}

std::vector<runtime::Ref<EffectReserved>> Effect::getReservedEffectsToSend() {
	AION_UNPORTED();
}

void Effect::setShieldDefense(int32_t value) {
	AION_UNPORTED();
}

bool Effect::isReflected() {
	AION_UNPORTED();
}

void Effect::setSubEffect(runtime::Ptr<Effect> value) {
	subEffect.set(value);
}

bool Effect::containsEffectId(int32_t effectId) {
	AION_UNPORTED();
}

bool Effect::isForcedEffect() {
	AION_UNPORTED();
}

bool Effect::isPhysicalEffect() {
	AION_UNPORTED();
}

void Effect::initialize() {
	AION_UNPORTED();
}

int32_t Effect::calculateHateForSuccessEffects() {
	AION_UNPORTED();
}

void Effect::applyEffect() {
	AION_UNPORTED();
}

void Effect::broadcastHate() {
	AION_UNPORTED();
}

bool Effect::shouldApplyFurtherEffects(runtime::Ptr<gameserver::model::gameobjects::Creature> value) {
	AION_UNPORTED();
}

// Stored lambda com.aionemu.gameserver.skillengine.model.Effect@L682:55 (end task in endTask, pin {this}): a TaskStruct here once ported
void Effect::startEffect() {
	AION_UNPORTED();
}

void Effect::activateToggleSkill() {
	AION_UNPORTED();
}

void Effect::deactivateToggleSkill() {
	AION_UNPORTED();
}

void Effect::endEffect() {
	AION_UNPORTED();
}

void Effect::endEffect(bool broadcast) {
	AION_UNPORTED();
}

void Effect::stopTasks() {
	AION_UNPORTED();
}

int64_t Effect::getRemainingTimeMillis() {
	AION_UNPORTED();
}

int32_t Effect::getRemainingTimeToDisplay() {
	AION_UNPORTED();
}

bool Effect::canSaveOnLogout() {
	AION_UNPORTED();
}

int32_t Effect::getPvpDamage() {
	AION_UNPORTED();
}

const gameserver::model::templates::item::ItemTemplate* Effect::getItemTemplate() {
	AION_UNPORTED();
}

void Effect::addToEffectedController() {
	AION_UNPORTED();
}

// Stored lambda com.aionemu.gameserver.skillengine.model.Effect@L828:27 (observer removal task, captures target and observer as Refs)
void Effect::addObserver(gameserver::model::gameobjects::Creature& target, controllers::observer::ActionObserver& observer) {
	AION_UNPORTED();
}

// Stored lambda com.aionemu.gameserver.skillengine.model.Effect@L833:27 (observer removal task, captures target and observer as Refs)
void Effect::addObserver(gameserver::model::gameobjects::Creature& target, controllers::observer::AttackCalcObserver& observer) {
	AION_UNPORTED();
}

void Effect::removeObservers() {
	AION_UNPORTED();
}

void Effect::addSuccessEffect(const effect::EffectTemplate* effect) {
	AION_UNPORTED();
}

bool Effect::isInSuccessEffects(int32_t position) {
	AION_UNPORTED();
}

const effect::EffectTemplate* Effect::effectInPos(int32_t pos) {
	AION_UNPORTED();
}

void Effect::addAllEffectToSucess() {
	AION_UNPORTED();
}

// Stored lambda com.aionemu.gameserver.skillengine.model.Effect@L871:77 (periodic actions task in periodicActionsTask, pin {this}, captures
// periodicActions as const PeriodicActions*): a TaskStruct here once ported
void Effect::schedulePeriodicActions() {
	AION_UNPORTED();
}

void Effect::stopPeriodicActions() {
	AION_UNPORTED();
}

int32_t Effect::calculateEffectsDuration() {
	AION_UNPORTED();
}

int64_t Effect::calculateTemplateDuration() {
	AION_UNPORTED();
}

int64_t Effect::applyCumulativeResistDurationMultiplier(int64_t value, gameserver::model::gameobjects::player::Player& effectedPlayer) {
	AION_UNPORTED();
}

bool Effect::isDeityAvatar() {
	AION_UNPORTED();
}

int8_t Effect::getSuccessfulEffectsAsByte() {
	AION_UNPORTED();
}

// Anonymous classes com.aionemu.gameserver.skillengine.model.Effect$1 and Effect$2 (ActionObservers ATTACKED / DOT_ATTACKED capturing this
// Effect as const Ref<Effect>, stored in the effected creature's ObserveController): the callback structs `Effect_ActionObserver` and
// `Effect_ActionObserver_2` printed by `fieldmap.py --class com.aionemu.gameserver.skillengine.model.Effect` are defined here once ported.
void Effect::addCancelOnDmgObserver() {
	AION_UNPORTED();
}

void Effect::endEffects() {
	AION_UNPORTED();
}

int32_t Effect::removePower(int32_t value) {
	AION_UNPORTED();
}

bool Effect::isHideEffect() {
	AION_UNPORTED();
}

bool Effect::isParalyzeEffect() {
	AION_UNPORTED();
}

bool Effect::isStunEffect() {
	AION_UNPORTED();
}

bool Effect::isSanctuaryEffect() {
	AION_UNPORTED();
}

bool Effect::isDamageEffect() {
	AION_UNPORTED();
}

bool Effect::isNoDeathPenalty() {
	AION_UNPORTED();
}

bool Effect::isNoResurrectPenalty() {
	AION_UNPORTED();
}

bool Effect::isHiPass() {
	AION_UNPORTED();
}

bool Effect::isDelayedDamage() {
	AION_UNPORTED();
}

bool Effect::isSummoning() {
	AION_UNPORTED();
}

bool Effect::isPetOrderUnSummonEffect() {
	AION_UNPORTED();
}

bool Effect::canRemoveOnDie() {
	AION_UNPORTED();
}

std::unordered_set<effect::EffectType> Effect::getPossibleConflictEffectTypes() {
	AION_UNPORTED();
}

bool Effect::setDesignatedDispelEffect(Effect& effect) {
	AION_UNPORTED();
}

void Effect::resetDesignatedDispelEffect() {
	AION_UNPORTED();
}

bool Effect::tryActivateGodstone() {
	AION_UNPORTED();
}

} // namespace aion::gameserver::skillengine::model
