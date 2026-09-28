#include "aion/gameserver/skillengine/model/Effect.h"

#include <algorithm>
#include <cstdint>
#include <exception>
#include <limits>
#include <string>
#include <utility>
#include <vector>

#include "aion/commons/utils/Exception.h"
#include "aion/commons/utils/Rnd.h"
#include "aion/gameserver/ai/AbstractAI.h"
#include "aion/gameserver/configs/main/CustomConfig.h"
#include "aion/gameserver/controllers/ControllerSupport.h"
#include "aion/gameserver/controllers/ObserveController.h"
#include "aion/gameserver/controllers/PlayerController.h"
#include "aion/gameserver/controllers/attack/AggroList.h"
#include "aion/gameserver/controllers/attack/AttackStatusInfo.h"
#include "aion/gameserver/controllers/effect/CumulativeResistType.h"
#include "aion/gameserver/controllers/effect/EffectController.h"
#include "aion/gameserver/controllers/effect/PlayerEffectController.h"
#include "aion/gameserver/controllers/observer/ActionObserver.h"
#include "aion/gameserver/controllers/observer/AttackCalcObserver.h"
#include "aion/gameserver/controllers/observer/ObserverType.h"
#include "aion/gameserver/instance/handlers/InstanceHandler.h"
#include "aion/gameserver/model/SkillElement.h"
#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/model/gameobjects/Npc.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/stats/container/CreatureGameStats.h"
#include "aion/gameserver/model/stats/container/CreatureLifeStats.h"
#include "aion/gameserver/model/templates/detail/JavaCasts.h"
#include "aion/gameserver/network/aion/serverpackets/SM_ATTACK_STATUS.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SKILL_ACTIVATION.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/runtime/base/TaskInfo.h"
#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/runtime/lifetime/TaskScope.h"
#include "aion/gameserver/runtime/sched/Clock.h"
#include "aion/gameserver/services/event/Event.h"
#include "aion/gameserver/skillengine/SkillEngine.h"
#include "aion/gameserver/skillengine/effect/AbstractAbsoluteStatEffect.h"
#include "aion/gameserver/skillengine/effect/AbnormalState.h"
#include "aion/gameserver/skillengine/effect/EffectTemplate.h"
#include "aion/gameserver/skillengine/effect/EffectType.h"
#include "aion/gameserver/skillengine/effect/Effects.h"
#include "aion/gameserver/skillengine/effect/FearEffect.h"
#include "aion/gameserver/skillengine/effect/ParalyzeEffect.h"
#include "aion/gameserver/skillengine/effect/SleepEffect.h"
#include "aion/gameserver/skillengine/model/ActivationAttribute.h"
#include "aion/gameserver/skillengine/model/EffectReserved.h"
#include "aion/gameserver/skillengine/model/HostileType.h"
#include "aion/gameserver/skillengine/model/ShieldType.h"
#include "aion/gameserver/skillengine/model/Skill.h"
#include "aion/gameserver/skillengine/model/SkillSubType.h"
#include "aion/gameserver/skillengine/model/SkillTargetSlot.h"
#include "aion/gameserver/skillengine/model/SkillTemplate.h"
#include "aion/gameserver/skillengine/periodicaction/PeriodicAction.h"
#include "aion/gameserver/skillengine/periodicaction/PeriodicActions.h"
#include "aion/gameserver/utils/PacketSendUtility.h"
#include "aion/gameserver/utils/ThreadPoolManager.h"
#include "aion/gameserver/utils/stats/StatFunctions.h"
#include "aion/gameserver/world/WorldMapInstance.h"
#include "aion/gameserver/world/WorldPosition.h"
#include "aion/gameserver/world/knownlist/KnownList.h"

namespace aion::gameserver::skillengine::model {

using controllers::attack::AttackStatus;
using effect::EffectTemplate;
using effect::EffectType;
using gameserver::model::gameobjects::Creature;
using gameserver::model::gameobjects::Npc;
using gameserver::model::gameobjects::VisibleObject;
using gameserver::model::gameobjects::player::Player;
using runtime::Ptr;
using runtime::Ref;

namespace {

/** Java int a + b (wraps on overflow) */
constexpr int32_t addInt(int32_t a, int32_t b) noexcept {
	return static_cast<int32_t>(static_cast<uint32_t>(a) + static_cast<uint32_t>(b));
}

/** Java int a - b (wraps on overflow) */
constexpr int32_t subInt(int32_t a, int32_t b) noexcept {
	return static_cast<int32_t>(static_cast<uint32_t>(a) - static_cast<uint32_t>(b));
}

/** Java SubEffectType.getId() (the constructor data of SubEffectType.java:8-15; SubEffectType has no C++ companion header, P5-02a) */
constexpr int8_t subEffectTypeId(SubEffectType type) noexcept {
	switch (type) {
		case SubEffectType::NONE:
		case SubEffectType::SPIN:
			return 0;
		case SubEffectType::PULL:
			return 2;
		case SubEffectType::PULL_NPC:
			return 6;
		case SubEffectType::STUMBLE:
		case SubEffectType::STAGGER:
		case SubEffectType::OPENAERIAL:
			return 4;
		case SubEffectType::SIMPLE_MOVE_BACK:
			return 12;
	}
	return 0;
}

/**
 * Java's implicit null check of `new Effect(...)`: the constructor dereferences the template (skillTemplate.getReqDispelCount(), Effect.java:154),
 * which is a NullPointerException in Java and would be undefined behaviour on the C++ raw pointer.
 */
const SkillTemplate& requireTemplate(const SkillTemplate* skillTemplate) {
	if (skillTemplate == nullptr)
		throw runtime::NullPointerException("Cannot invoke \"SkillTemplate.getReqDispelCount()\" because \"skillTemplate\" is null");
	return *skillTemplate;
}

/**
 * Java System.currentTimeMillis() for the effect's end time. C++: the clock of the installed scheduler backend, which is the system clock in the
 * server (SystemClock::currentTimeMillis) and the ManualClock under a DeterministicExecutor, so endTime and the end task scheduled `duration`
 * ms later are measured on one time line (docs/deviations/P5-02b.md).
 */
int64_t currentTimeMillis() {
	return utils::ThreadPoolManager::clock().currentTimeMillis();
}

/**
 * The values of `successEffects` in Java's iteration order. Java's ConcurrentHashMap<Integer, EffectTemplate> (default capacity: one table of
 * 16 bins) places key k in bin `spread(k) & 15`, which is k itself for 0 <= k < 16, and iterates the bins in order - so its values() come in
 * ascending position order (effect positions are 1..4, Effect.java:58 sizes magicalCriticals for exactly those). The C++ shim stripes by a
 * murmur hash and would iterate in another order; every loop of this file that Java writes over successEffects.values() iterates this snapshot
 * instead. A snapshot is equivalent here: no body called from these loops adds or removes success effects of the same Effect (only the
 * calculate step does, and initialize iterates the template list for it).
 */
std::vector<const EffectTemplate*> javaOrderedValues(const runtime::ConcurrentHashMap<int32_t, const EffectTemplate*>& successEffects) {
	auto entries = successEffects.snapshot();
	std::sort(entries.begin(), entries.end(), [](const auto& a, const auto& b) { return a.key < b.key; });
	std::vector<const EffectTemplate*> values;
	values.reserve(entries.size());
	for (const auto& entry : entries)
		values.push_back(entry.value);
	return values;
}

/**
 * Java: Effect.getPossibleConflictEffectTypes returns the Set of Effects.getPossibleConflictEffectTypes itself. Effects.h stores a std::set and
 * Effect.h declares a std::unordered_set, so the elements are copied; only EffectController.canConflict reads them (Collections.disjoint)
 */
std::unordered_set<EffectType> toUnorderedSet(const std::set<EffectType>& types) {
	return std::unordered_set<EffectType>(types.begin(), types.end());
}

} // namespace

/**
 * Java: the anonymous ActionObserver(ObserverType.ATTACKED) of Effect.addCancelOnDmgObserver (Effect.java:1012-1018, fieldmap key Effect$1).
 * Stored in the effected creature's ObserveController and, through the removal task of addObserver, in observerRemoveTasks; removeObservers
 * (from endEffect) removes it from both (cycles.toml "Effect$1#this": java-hook).
 */
struct Effect_ActionObserver final : controllers::observer::ActionObserver {
	AION_MAKE_REF_FRIEND

	const Ref<Effect> effect; // captured this Effect this (line 1016)

	static Ref<Effect_ActionObserver> create(Effect& effect) { return runtime::makeRef<Effect_ActionObserver>(effect); }

	void attacked(Creature& /*creature*/, int32_t /*skillId*/) override { effect->endEffect(); }

protected:
	explicit Effect_ActionObserver(Effect& effectValue)
		: ActionObserver(controllers::observer::ObserverType::ATTACKED), effect(Ref<Effect>(effectValue)) {}
	~Effect_ActionObserver() override = default;
};

/** Java: the anonymous ActionObserver(ObserverType.DOT_ATTACKED) of Effect.addCancelOnDmgObserver (Effect.java:1019-1025, key Effect$2). */
struct Effect_ActionObserver_2 final : controllers::observer::ActionObserver {
	AION_MAKE_REF_FRIEND

	const Ref<Effect> effect; // captured this Effect this (line 1023)

	static Ref<Effect_ActionObserver_2> create(Effect& effect) { return runtime::makeRef<Effect_ActionObserver_2>(effect); }

	void dotattacked(Creature& /*creature*/, Effect& /*dotEffect*/) override { effect->endEffect(); }

protected:
	explicit Effect_ActionObserver_2(Effect& effectValue)
		: ActionObserver(controllers::observer::ObserverType::DOT_ATTACKED), effect(Ref<Effect>(effectValue)) {}
	~Effect_ActionObserver_2() override = default;
};

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
	  power(requireTemplate(skillTemplate).getReqDispelCount()), isSubEffect_(false) {
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
	  power(requireTemplate(skillTemplateValue).getReqDispelCount()), isSubEffect_(isSubEffectValue) {
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
	return effector->getObjectId();
}

void Effect::setAbnormal(effect::AbnormalState state) {
	abnormals.set(abnormals.get() | controllers::detail::getAbnormalStateId(state)); // java-race: `abnormals |= state.getId()` on a plain int
}

int32_t Effect::getSkillId() {
	return skillTemplate->getSkillId();
}

std::string Effect::getSkillName() {
	return skillTemplate->getName();
}

SkillSubType Effect::getSkillSubType() {
	return skillTemplate->getSubType();
}

std::string Effect::getStack() {
	return skillTemplate->getStack();
}

int32_t Effect::getSkillStackLvl() {
	return skillTemplate->getLvl();
}

SkillType Effect::getSkillType() {
	return skillTemplate->getType();
}

int32_t Effect::getDuration() {
	std::optional<int32_t> value = duration.get();
	return !value ? 0 : *value;
}

runtime::Ptr<gameserver::model::gameobjects::Creature> Effect::getEffected() {
	return isReflected() ? Ptr<Creature>(effector) : Ptr<Creature>(effected);
}

bool Effect::isPassive() {
	return skillTemplate->isPassive();
}

bool Effect::isPeriodic() {
	return static_cast<bool>(periodicTasks.get());
}

void Effect::setPeriodicTask(runtime::FutureRef periodicTask, int32_t position) {
	if (!periodicTasks.get()) {
		periodicTasks.set(runtime::Array<runtime::FutureRef>::make(4));
	} else if ((*periodicTasks)[position - 1].get() && periodicTask) {
		periodicTask->cancel(false);
		// Java: getClass().getSimpleName(); Effect has no subclass
		throw runtime::IllegalStateException("Effect already has a periodic task at position " + std::to_string(position));
	}
	(*periodicTasks)[position - 1] = std::move(periodicTask);
}

bool Effect::isMagicalCritical(int32_t position) {
	return magicalCriticals->get(position - 1);
}

void Effect::rollMagicalCritical(int32_t position, int32_t criticalProb) {
	if (!magicalCriticalRolled.get()) {
		// Java passes the effected field; calculateMagicalCriticalRate dereferences it (StatFunctions.java:424), so null is a NullPointerException
		magicalCritical.set(utils::stats::StatFunctions::calculateMagicalCriticalRate(*effector, *effected, criticalProb));
		magicalCriticalRolled.set(true);
	}
	reuseMagicalCritical(position);
}

void Effect::reuseMagicalCritical(int32_t position) {
	(*magicalCriticals)[position - 1] = magicalCritical.get();
}

void Effect::resetMagicalCritical() {
	magicalCritical.set(false);
	magicalCriticalRolled.set(false);
}

void Effect::setMagicalCriticals(const std::unordered_set<int32_t>& positions) {
	magicalCritical.set(false);
	magicalCriticalRolled.set(true);
	for (int32_t i = 0; i < magicalCriticals->length(); i++) {
		// Java: positions.contains(i) - index i, not i + 1, exactly as written (Effect.java:298)
		(*magicalCriticals)[i] = positions.contains(i);
		magicalCritical.set(magicalCritical.get() | magicalCriticals->get(i));
	}
}

std::vector<const effect::EffectTemplate*> Effect::getEffectTemplates() {
	const effect::Effects* effects = skillTemplate->getEffects();
	if (effects == nullptr) // Java: skillTemplate.getEffects().getEffects() on a skill without <effects>
		throw runtime::NullPointerException("skill " + std::to_string(skillTemplate->getSkillId()) + " has no effects");
	std::vector<const EffectTemplate*> templates;
	templates.reserve(effects->getEffects().size());
	for (const std::unique_ptr<EffectTemplate>& effectTemplate : effects->getEffects())
		templates.push_back(effectTemplate.get());
	return templates;
}

bool Effect::isToggle() {
	return skillTemplate->getActivationAttribute() == ActivationAttribute::TOGGLE;
}

bool Effect::isChant() {
	return skillTemplate->getTargetSlot() == SkillTargetSlot::CHANT;
}

std::optional<SkillTargetSlot> Effect::getTargetSlot() {
	return skillTemplate->getTargetSlot();
}

int32_t Effect::getTargetSlotLevel() {
	return skillTemplate->getTargetSlotLevel();
}

DispelCategoryType Effect::getDispelCategory() {
	return skillTemplate->getDispelCategory();
}

int32_t Effect::getReqDispelLevel() {
	return skillTemplate->getReqDispelLevel();
}

runtime::Ref<EffectReserved> Effect::getReserveds(int32_t position) {
	SYNCHRONIZED(reservedEffects) {
		for (Ptr<EffectReserved> er : reservedEffects) {
			if (er->getPosition() == position)
				return Ref<EffectReserved>(er);
		}
	}
	return EffectReserved::create(0, 0, EffectReserved::ResourceType::HP, true, false);
}

void Effect::setReserveds(EffectReserved& er, bool overTimeEffect) {
	// set effected hp
	// TODO RI_ChargeAttack_G, RI_ChargingFlight_G
	bool instantSkill = false;
	if (getSkill() && getSkill()->isInstantSkill())
		instantSkill = true;
	if (er.getType() == EffectReserved::ResourceType::HP && er.getValue() != 0 && !overTimeEffect && !instantSkill && !getEffected()->isInvulnerable()) {
		Ptr<Creature> effectedValue = getEffected();
		int32_t value = (er.isDamage() ? subInt(0, er.getValue()) : er.getValue());
		value = addInt(value, effectedValue->getLifeStats()->getCurrentHp());
		if (value <= 0) {
			value = 0;
			// effected is about to die
			if (!effectedValue->isDead())
				effectedValue->getLifeStats()->setKillingBlow(er.getValue());
			effectedHp.set(0);
		} else {
			// Java: (int) (100f * value / maxHp) - float arithmetic, then the saturating (int) cast
			effectedHp.set(std::max(1, gameserver::model::templates::detail::floatToInt(
										   100.0f * static_cast<float>(value) / static_cast<float>(effectedValue->getLifeStats()->getMaxHp()))));
		}
	}
	SYNCHRONIZED(reservedEffects) {
		reservedEffects.add(Ref<EffectReserved>(er));
	}
}

std::vector<runtime::Ref<EffectReserved>> Effect::getReservedEffectsToSend() {
	std::vector<Ref<EffectReserved>> toSend; // Java: new TreeSet<>() (EffectReserved.compareTo)
	SYNCHRONIZED(reservedEffects) {
		for (Ptr<EffectReserved> er : reservedEffects) {
			if (er->isSend() && er->getValue() != 0)
				toSend.push_back(Ref<EffectReserved>(er));
		}
	}
	// the set holds distinct objects, and compareTo answers 0 only for the same object, so the TreeSet keeps every element in compareTo order
	std::sort(toSend.begin(), toSend.end(), [](const Ref<EffectReserved>& a, const Ref<EffectReserved>& b) { return a->compareTo(*b) < 0; });
	if (toSend.empty()) // effects without a sent value (like damage over time) can still show their attack status
		return {EffectReserved::create(0, 0, EffectReserved::ResourceType::HP, true, true, attackStatus.get())};
	return toSend;
}

void Effect::setShieldDefense(int32_t value) {
	const int32_t skillReflector = controllers::detail::shieldTypeId(ShieldType::SKILL_REFLECTOR);
	if ((value & skillReflector) != 0 && getSkillSubType() != SkillSubType::ATTACK && getSkillSubType() != SkillSubType::DEBUFF)
		value &= ~skillReflector; // disable SKILL_REFLECTOR bit (only attack type effects can reflect whole effects)
	shieldDefense.set(value);
}

bool Effect::isReflected() {
	return (shieldDefense.get() & controllers::detail::shieldTypeId(ShieldType::SKILL_REFLECTOR)) != 0;
}

void Effect::setSubEffect(runtime::Ptr<Effect> value) {
	subEffect.set(value);
}

bool Effect::containsEffectId(int32_t effectId) {
	for (const EffectTemplate* effectTemplate : javaOrderedValues(successEffects)) {
		if (effectTemplate->getEffectId() == effectId)
			return true;
	}
	return false;
}

bool Effect::isForcedEffect() {
	return forceType.get() != nullptr;
}

bool Effect::isPhysicalEffect() {
	const EffectTemplate* mainEffectTemplate = skillTemplate->getEffectTemplate(1);
	return mainEffectTemplate != nullptr && mainEffectTemplate->getElement() == gameserver::model::SkillElement::NONE;
}

void Effect::initialize() {
	if (skillTemplate->getEffects() == nullptr)
		return;

	if (effected && effected->getEffectController()->isConflicting(*this))
		setEffectResult(EffectResult::CONFLICT);
	if (effectResult.get() != EffectResult::CONFLICT) {
		for (const EffectTemplate* effectTemplate : getEffectTemplates()) {
			effectTemplate->calculate(*this);
		}
	}
	if (!isInSuccessEffects(1)) {
		successEffects.clear();
	} else {
		if (effectHate.get() == 0) // can be overridden from constructor with skill (from pet order)
			effectHate.set(calculateHateForSuccessEffects());
		if (isLaunchSubEffect()) {
			for (const EffectTemplate* effectTemplate : javaOrderedValues(successEffects)) {
				effectTemplate->calculateSubEffect(*this);
			}
		}
		// Java evaluates the conjunction left to right: the chance is rolled only for a critical player hit without a sub effect or periodic task
		if (Ptr<Player> p = runtime::as<Player>(effector); p && getAttackStatus() == AttackStatus::CRITICAL && !getSubEffect() && !isPeriodic()
			&& commons::utils::Rnd::chance() < 10) {
			Ref<Effect> criticalProcEffect = SkillEngine::getInstance().createCriticalProcEffect(*p, *getEffected(), skillTemplate->getSkillId());
			if (criticalProcEffect && criticalProcEffect->getEffectResult() != EffectResult::DODGE
				&& criticalProcEffect->getEffectResult() != EffectResult::RESIST) {
				applyCriticalProcEffect.set(true);
				setSpellStatus(criticalProcEffect->getSpellStatus());
				setSubEffect(criticalProcEffect);
				setSubEffectType(criticalProcEffect->getSubEffectType());
				setTargetLoc(criticalProcEffect->getTargetX(), criticalProcEffect->getTargetY(), criticalProcEffect->getTargetZ());
			}
		}
	}

	if (successEffects.isEmpty()) {
		if (effectResult.get() == EffectResult::CONFLICT) {
			setAttackStatus(AttackStatus::DODGE);
		} else if (isPhysicalEffect()) {
			if (getAttackStatus() == AttackStatus::CRITICAL)
				setAttackStatus(AttackStatus::CRITICAL_DODGE);
			else
				setAttackStatus(AttackStatus::DODGE);
			setSpellStatus(SpellStatus::DODGE2);
			effectResult.set(EffectResult::DODGE);
		} else {
			if (getAttackStatus() == AttackStatus::CRITICAL)
				setAttackStatus(AttackStatus::CRITICAL_RESIST); // TODO recheck
			else
				setAttackStatus(AttackStatus::RESIST);
			setSpellStatus(SpellStatus::NONE);
			effectResult.set(EffectResult::RESIST);
		}
	}

	// set spellstatus for sm_castspell_end packet
	switch (controllers::attack::getBaseStatus(getAttackStatus())) {
		case AttackStatus::DODGE:
			if (effectResult.get() != EffectResult::CONFLICT)
				setSpellStatus(SpellStatus::DODGE);
			break;
		case AttackStatus::PARRY:
			if (getSpellStatus() == SpellStatus::NONE)
				setSpellStatus(SpellStatus::PARRY);
			break;
		case AttackStatus::BLOCK:
			if (getSpellStatus() == SpellStatus::NONE)
				setSpellStatus(SpellStatus::BLOCK);
			break;
		case AttackStatus::RESIST:
			setSpellStatus(SpellStatus::RESIST);
			break;
		default:
			break;
	}
}

int32_t Effect::calculateHateForSuccessEffects() {
	int32_t hate = 0;
	for (const EffectTemplate* effectTemplate : javaOrderedValues(successEffects)) {
		hate = addInt(hate, effectTemplate->calculateHate(*this));
	}
	return hate == 0 ? 0 : utils::stats::StatFunctions::calculateHate(*getEffector(), hate);
}

void Effect::applyEffect() {
	if (successEffects.isEmpty()) {
		broadcastHate();
		return;
	}

	Ptr<Creature> effectedValue = getEffected();
	try {
		for (const EffectTemplate* effectTemplate : javaOrderedValues(successEffects)) {
			if (!shouldApplyFurtherEffects(effectedValue))
				break;
			effectTemplate->applyEffect(*this);
			if (!shouldApplyFurtherEffects(effectedValue))
				break;
			effectTemplate->startSubEffect(*this);
		}
		if (applyCriticalProcEffect.get() && subEffect.get())
			subEffect->applyEffect();
		if (effectedValue)
			effectedValue->getAi().onEffectApplied(*this);
	} catch (const runtime::UnportedException&) {
		// C++ only: a body that is not ported yet is not one of Java's exceptions; it propagates unwrapped so its site stays the exception the
		// caller and unported_trace.txt name (docs/deviations/P5-02b.md)
		throw;
	} catch (const std::exception&) {
		throw commons::utils::Exception("Error applying effect of skill " + std::to_string(getSkillId()) + " from " + effector->toString() + " to "
				+ (effectedValue ? effectedValue->toString() : std::string("null")),
			std::current_exception());
	}
}

void Effect::broadcastHate() {
	if (effectHate.get() != 0 && tauntHate.get() >= 0) { // don't add hate if taunt hate is < 0!
		Ptr<Creature> effectedValue = getEffected();
		effectedValue->getAggroList().addHate(*effector, effectHate.get());
		if (skillTemplate->getHostileType() == HostileType::INDIRECT) {
			effectedValue->getKnownList().forEachObject([this, &effectedValue](VisibleObject& visibleObject) {
				if (Ptr<Creature> creature = runtime::as<Creature>(visibleObject)) {
					controllers::attack::AggroList& al = creature->getAggroList();
					if (al.isHating(*effector) || al.isHating(*effectedValue))
						al.addHate(*effector, effectHate.get());
				}
			});
		}
		effectHate.set(0); // set to 0 to avoid external second broadcast
	}
}

bool Effect::shouldApplyFurtherEffects(runtime::Ptr<gameserver::model::gameobjects::Creature> value) {
	if (value) {
		if (!value->isSpawned() && !skillTemplate->isPassive()) // only allow on despawned if it's a passive skill (players get them during enterWorld)
			return false;
		if (value->isDead() && !skillTemplate->hasResurrectEffect())
			return false;
	}
	return true;
}

// Stored lambda com.aionemu.gameserver.skillengine.model.Effect@L682:55 (end task in endTask, pin {this}): cycles.toml "Effect@L682:55#this" is
// a one-shot task that releases its captures when it runs or is cancelled (stopTasks)
void Effect::startEffect() {
	SYNCHRONIZED(*this) {
		if (hasEnded.get()) // multiple concurrent skill casts can end each others effects by conflict
			return;
		if (successEffects.isEmpty())
			return;

		schedulePeriodicActions();

		if (!successEffects.isEmpty()) {
			for (const EffectTemplate* effectTemplate : javaOrderedValues(successEffects))
				effectTemplate->startEffect(*this);
			addCancelOnDmgObserver();
		}

		broadcastHate();

		if (!duration.get()) {
			if (isToggle()) {
				if (runtime::as<Player>(effector))
					activateToggleSkill();
				duration.set(skillTemplate->getToggleTimer());
			} else {
				duration.set(calculateEffectsDuration());
			}
		}
		const int32_t durationValue = *duration.get();
		if (durationValue == 0)
			return;
		endTime.set(currentTimeMillis() + durationValue);

		endTask.set(utils::ThreadPoolManager::getInstance().schedule(runtime::Pin{this}, [this] {
			endedByTime.set(true);
			endEffect(true);
		}, durationValue));
	}
	effected->getPosition()->getWorldMapInstance()->getInstanceHandler()->onStartEffect(Ptr<Effect>(*this));
}

void Effect::activateToggleSkill() {
	utils::PacketSendUtility::sendPacket(*runtime::cast<Player>(effector), network::aion::serverpackets::SM_SKILL_ACTIVATION(getSkillId(), true));
}

void Effect::deactivateToggleSkill() {
	utils::PacketSendUtility::sendPacket(*runtime::cast<Player>(effector), network::aion::serverpackets::SM_SKILL_ACTIVATION(getSkillId(), false));
}

void Effect::endEffect() {
	endEffect(true);
}

void Effect::endEffect(bool broadcast) {
	SYNCHRONIZED(*this) { // wait for startEffect before ending
		if (!hasEnded.compareAndSet(false, true))
			return;
	}
	stopTasks();
	removeObservers();

	endEffects();

	// if effect is a stance, remove stance from player
	if (Ptr<Player> player = runtime::as<Player>(effector)) {
		if (player->getController().getStanceSkillId() == getSkillId())
			player->getController().stopStance();
	}

	Ptr<Creature> effectedValue = getEffected();
	// TODO better way to finish
	if (getSkillTemplate()->getTargetSlot() == SkillTargetSlot::SPEC2) {
		// Java: (int) (maxHp * 0.2f) - an int times a float, then the saturating (int) cast
		effectedValue->getLifeStats()->increaseHp(network::aion::serverpackets::SM_ATTACK_STATUS_TYPE::REGULAR,
			gameserver::model::templates::detail::floatToInt(static_cast<float>(effectedValue->getLifeStats()->getMaxHp()) * 0.2f), *getEffector());
		effectedValue->getLifeStats()->increaseMp(
			gameserver::model::templates::detail::floatToInt(static_cast<float>(effectedValue->getLifeStats()->getMaxMp()) * 0.2f));
	}

	if (isToggle() && runtime::as<Player>(effector)) {
		deactivateToggleSkill();
	}
	effectedValue->getEffectController()->clearEffect(*this, broadcast);
	// C++ breaker (cycles.toml "Effect.designatedDispelEffect", Effect.h): an ended effect is out of every EffectController map, the only readers
	resetDesignatedDispelEffect();

	effectedValue->getAi().onEffectEnd(Ptr<Effect>(*this));
	effectedValue->getPosition()->getWorldMapInstance()->getInstanceHandler()->onEndEffect(*this);
}

// cycles.toml: "AbstractOverTimeEffect@L55:72#effect" and the other periodic task rows (java-hook: Effect.stopTasks cancels the periodic task held
// in Effect.periodicTasks), "Effect@L682:55#this" (the end task) and "Effect@L871:77#this" (stopPeriodicActions). Cancelling a pending task releases
// its Pin and callable at once (Future::cancel), which is what cuts the Effect -> Future -> Effect cycles.
void Effect::stopTasks() {
	if (Ptr<runtime::Future> task = endTask.get()) {
		task->cancel(false);
		endTask.set(nullptr);
	}

	if (Ptr<runtime::Array<runtime::FutureRef>> tasks = periodicTasks.get()) {
		for (Ptr<runtime::Future> periodicTask : *tasks) {
			if (periodicTask)
				periodicTask->cancel(false);
		}
		periodicTasks.set(nullptr);
	}

	stopPeriodicActions();
}

int64_t Effect::getRemainingTimeMillis() {
	return endTime.get() - currentTimeMillis();
}

int32_t Effect::getRemainingTimeToDisplay() {
	if (getDuration() == 0) // permanent effect (or not yet started, should not happen)
		return -1;
	if (*duration.get() >= 86400000 && runtime::as<Npc>(effected)) // >= 24h
		return -1;
	int64_t remainingTimeMillis = getRemainingTimeMillis();
	// Java: (int) remainingTimeMillis - a narrowing that keeps the low 32 bits (C++20 conversion is modular as well)
	return remainingTimeMillis > std::numeric_limits<int32_t>::max() ? -1 : static_cast<int32_t>(remainingTimeMillis);
}

bool Effect::canSaveOnLogout() {
	if (skillTemplate->isNoSaveOnLogout())
		return false;
	if (getDuration() == 0) // permanent effect, such as toggle or passive skill (or not yet started, should not happen)
		return false;
	if (*duration.get() >= 86400000) // effects with duration >= 24h are event or instance related
		return false;
	return true;
}

int32_t Effect::getPvpDamage() {
	return skillTemplate->getPvpDamage();
}

const gameserver::model::templates::item::ItemTemplate* Effect::getItemTemplate() {
	return !skill ? nullptr : skill->getItemTemplate();
}

void Effect::addToEffectedController() {
	if (!addedToController.get()) {
		Ptr<Creature> effectedValue = getEffected();
		if (effectedValue->getLifeStats() && !effectedValue->isDead()) {
			effectedValue->getEffectController()->addEffect(*this);
			addedToController.set(true);
		}
	}
}

// Stored lambda com.aionemu.gameserver.skillengine.model.Effect@L828:27 (observer removal task, pin {&target, &observer}): cycles.toml
// "Effect@L828:27#observer"/"#target" - removeObservers (from endEffect) runs and clears observerRemoveTasks
void Effect::addObserver(gameserver::model::gameobjects::Creature& target, controllers::observer::ActionObserver& observer) {
	target.getObserveController()->addObserver(observer);
	observerRemoveTasks.add(runtime::PinnedCallback<void()>(runtime::Pin{&target, &observer}, [&target, &observer] {
		target.getObserveController()->removeObserver(observer);
	}));
}

// Stored lambda com.aionemu.gameserver.skillengine.model.Effect@L833:27 (observer removal task, pin {&target, &observer}): cycles.toml
// "Effect@L833:27#observer"/"#target" - removeObservers (from endEffect) runs and clears observerRemoveTasks
void Effect::addObserver(gameserver::model::gameobjects::Creature& target, controllers::observer::AttackCalcObserver& observer) {
	target.getObserveController()->addAttackCalcObserver(observer);
	observerRemoveTasks.add(runtime::PinnedCallback<void()>(runtime::Pin{&target, &observer}, [&target, &observer] {
		target.getObserveController()->removeAttackCalcObserver(observer);
	}));
}

void Effect::removeObservers() {
	observerRemoveTasks.forEach([](const runtime::PinnedCallback<void()>& task) { task(); });
	observerRemoveTasks.clear();
}

void Effect::addSuccessEffect(const effect::EffectTemplate* effectTemplate) {
	successEffects.put(effectTemplate->getPosition(), effectTemplate);
}

bool Effect::isInSuccessEffects(int32_t position) {
	return successEffects.get(position) != nullptr;
}

const effect::EffectTemplate* Effect::effectInPos(int32_t pos) {
	return successEffects.get(pos);
}

void Effect::addAllEffectToSucess() {
	successEffects.clear();
	for (const EffectTemplate* effectTemplate : getEffectTemplates()) {
		successEffects.put(effectTemplate->getPosition(), effectTemplate);
	}
}

// Stored lambda com.aionemu.gameserver.skillengine.model.Effect@L871:77 (periodic actions task in periodicActionsTask, pin {this}, captures
// periodicActions as const PeriodicActions*): cycles.toml "Effect@L871:77#this" - stopTasks -> stopPeriodicActions cancels it
void Effect::schedulePeriodicActions() {
	if (skillTemplate->getPeriodicActions() == nullptr)
		return;
	const periodicaction::PeriodicActions* periodicActions = skillTemplate->getPeriodicActions();
	if (periodicActions->getPeriodicActions().empty()) // Java: getPeriodicActions() == null || isEmpty()
		return;
	int32_t checktime = periodicActions->getChecktime();
	// the immortal template is captured by reference and pinned (a template pin retains nothing, lint L5)
	const periodicaction::PeriodicActions& actions = *periodicActions;
	periodicActionsTask.set(utils::ThreadPoolManager::getInstance().scheduleAtFixedRate(runtime::Pin{this, &actions}, [this, &actions] {
		for (const std::unique_ptr<periodicaction::PeriodicAction>& action : actions.getPeriodicActions())
			action->act(*this);
	}, checktime, checktime));
}

void Effect::stopPeriodicActions() {
	if (Ptr<runtime::Future> task = periodicActionsTask.get()) {
		task->cancel(false);
		periodicActionsTask.set(nullptr);
	}
}

int32_t Effect::calculateEffectsDuration() {
	int64_t value = calculateTemplateDuration();

	if (Ptr<Player> effectedPlayer = runtime::as<Player>(getEffected())) {
		bool isEffectorPlayer =
			static_cast<bool>(runtime::as<Player>(configs::main::CustomConfig::COUNT_SUMMON_EFFECTS_FOR_CUMULATIVE_RESIST.load() ? effector->getMaster()
																											   : Ptr<Creature>(effector)));
		if (isEffectorPlayer) {
			value = applyCumulativeResistDurationMultiplier(value, *effectedPlayer);
		}
		// Java: !effector.equals(effected) - the effected field; equals(null) is false
		if (skillTemplate->getPvpDuration() != 0 && !(effected && effector->equals(*effected))) {
			value = value * skillTemplate->getPvpDuration() / 100;
		}
	}
	// Java: (int) Math.min(Integer.MAX_VALUE, duration) - the long narrowing keeps the low 32 bits
	return static_cast<int32_t>(std::min<int64_t>(std::numeric_limits<int32_t>::max(), value));
}

int64_t Effect::calculateTemplateDuration() {
	// retail sets the first effect duration > 0 as the skill duration, ignoring longer durations of other effects (see 620 Armor of Attrition)
	for (const EffectTemplate* et : javaOrderedValues(successEffects)) {
		int64_t effectDuration = et->getDuration2() + static_cast<int64_t>(et->getDuration1()) * getSkillLevel(); // some event skills would produce an int overflow
		if (effectDuration > 0) {
			if (et->getRandomTime() > 0)
				effectDuration -= commons::utils::Rnd::get(0, et->getRandomTime());
			return effectDuration;
		}
	}
	return 0;
}

int64_t Effect::applyCumulativeResistDurationMultiplier(int64_t value, gameserver::model::gameobjects::player::Player& effectedPlayer) {
	for (const EffectTemplate* et : javaOrderedValues(successEffects)) {
		// Java: a type pattern switch, so subclasses match too (BuffSleepEffect extends SleepEffect)
		std::optional<controllers::effect::CumulativeResistType> cumulativeResistType;
		if (dynamic_cast<const effect::FearEffect*>(et))
			cumulativeResistType = controllers::effect::CumulativeResistType::FEAR;
		else if (dynamic_cast<const effect::ParalyzeEffect*>(et))
			cumulativeResistType = controllers::effect::CumulativeResistType::PARALYZE;
		else if (dynamic_cast<const effect::SleepEffect*>(et))
			cumulativeResistType = controllers::effect::CumulativeResistType::SLEEP;
		if (cumulativeResistType && !et->isNoResist())
			return effectedPlayer.getEffectController()->calculateAndApplyCumulativeResistDuration(*cumulativeResistType, value);
	}
	return value;
}

bool Effect::isDeityAvatar() {
	return skillTemplate->isDeityAvatar();
}

int8_t Effect::getSuccessfulEffectsAsByte() {
	if (effectResult.get() == EffectResult::DODGE)
		return 0;
	if (effectResult.get() == EffectResult::RESIST)
		return 1;
	int8_t sucEffects = 0;
	for (const EffectTemplate* effectTemplate : javaOrderedValues(successEffects)) {
		// e1 = 1000, e2 = 11000, e3 = 111000, e4 = 1111000
		// Java: `sucEffects += 1 << (position + 3)` on a byte - the int sum narrowed back to a byte, the shift count masked to 5 bits
		sucEffects = static_cast<int8_t>(sucEffects + static_cast<int32_t>(uint32_t{1} << ((effectTemplate->getPosition() + 3) & 31)));
	}
	sucEffects = static_cast<int8_t>(sucEffects + subEffectTypeId(subEffectType.get()));
	return sucEffects;
}

// Anonymous classes com.aionemu.gameserver.skillengine.model.Effect$1 and Effect$2: the callback structs Effect_ActionObserver and
// Effect_ActionObserver_2 above
void Effect::addCancelOnDmgObserver() {
	if (isCancelOnDmg()) {
		Ptr<Creature> effectedValue = getEffected();
		addObserver(*effectedValue, *Effect_ActionObserver::create(*this));
		addObserver(*effectedValue, *Effect_ActionObserver_2::create(*this));
	}
}

void Effect::endEffects() {
	bool hasStatModifiers = false;
	for (const EffectTemplate* effectTemplate : javaOrderedValues(successEffects)) {
		effectTemplate->endEffect(*this);
		// Java: template.getChange() != null && !template.getChange().isEmpty() - the bound list is empty where Java's is null or empty
		if (!hasStatModifiers
			&& (!effectTemplate->getChange().empty() || dynamic_cast<const effect::AbstractAbsoluteStatEffect*>(effectTemplate) != nullptr))
			hasStatModifiers = true;
	}
	if (hasStatModifiers)
		getEffected()->getGameStats()->endEffect(*this);
}

int32_t Effect::removePower(int32_t value) {
	power.set(subInt(power.get(), value));

	return power.get();
}

bool Effect::isHideEffect() {
	return getSkillTemplate()->hasAnyEffect({EffectType::HIDE});
}

bool Effect::isParalyzeEffect() {
	return getSkillTemplate()->hasAnyEffect({EffectType::PARALYZE});
}

bool Effect::isStunEffect() {
	return getSkillTemplate()->hasAnyEffect({EffectType::STUN});
}

bool Effect::isSanctuaryEffect() {
	return getSkillTemplate()->hasAnyEffect({EffectType::SANCTUARY});
}

bool Effect::isDamageEffect() {
	return getSkillTemplate()->hasAnyEffect({EffectType::BACKDASH, EffectType::CARVESIGNET, EffectType::DASH, EffectType::DEATHBLOW,
		EffectType::DELAYEDSPELLATTACKINSTANT, EffectType::DISPELBUFFCOUNTERATK, EffectType::MOVEBEHIND, EffectType::NOREDUCESPELLATKINSTANT,
		EffectType::PROCATKINSTANT, EffectType::SIGNETBURST, EffectType::SKILLATKDRAININSTANT, EffectType::SKILLATTACKINSTANT,
		EffectType::SPELLATKDRAININSTANT, EffectType::SPELLATTACKINSTANT});
}

bool Effect::isNoDeathPenalty() {
	return getSkillTemplate()->hasAnyEffect({EffectType::NODEATHPENALTY});
}

bool Effect::isNoResurrectPenalty() {
	return getSkillTemplate()->hasAnyEffect({EffectType::NORESURRECTPENALTY});
}

bool Effect::isHiPass() {
	return getSkillTemplate()->hasAnyEffect({EffectType::HIPASS});
}

bool Effect::isDelayedDamage() {
	return getSkillTemplate()->hasAnyEffect({EffectType::DELAYEDSPELLATTACKINSTANT});
}

bool Effect::isSummoning() {
	return getSkillTemplate()->hasAnyEffect({EffectType::SUMMON, EffectType::SUMMONBINDINGGROUPGATE, EffectType::SUMMONFUNCTIONALNPC,
		EffectType::SUMMONGROUPGATE, EffectType::SUMMONHOMING, EffectType::SUMMONHOUSEGATE, EffectType::SUMMONSERVANT, EffectType::SUMMONSKILLAREA,
		EffectType::SUMMONTOTEM, EffectType::SUMMONTRAP});
}

bool Effect::isPetOrderUnSummonEffect() {
	return getSkillTemplate()->hasAnyEffect({EffectType::PETORDERUNSUMMON});
}

bool Effect::canRemoveOnDie() {
	if (getSkillTemplate()->isNoRemoveOnDie())
		return false;
	if (services::event::Event::isEventEffectForceType(forceType.get()))
		return false;
	if (getSkillTemplate()->hasAnyEffect({EffectType::XPBOOST}))
		return false;
	return true;
}

std::unordered_set<effect::EffectType> Effect::getPossibleConflictEffectTypes() {
	return skillTemplate->getEffects() == nullptr ? std::unordered_set<EffectType>()
												  : toUnorderedSet(skillTemplate->getEffects()->getPossibleConflictEffectTypes());
}

bool Effect::setDesignatedDispelEffect(Effect& effect) {
	if (!designatedDispelEffect.get()) { // java-race: a plain check-then-set, as Java writes it
		designatedDispelEffect.set(Ref<Effect>(effect));
		return true;
	}
	return false;
}

void Effect::resetDesignatedDispelEffect() {
	designatedDispelEffect.set(nullptr);
}

bool Effect::tryActivateGodstone() {
	return allowGodstoneActivation.compareAndSet(true, false);
}

} // namespace aion::gameserver::skillengine::model
