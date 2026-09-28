#include "aion/gameserver/skillengine/effect/EffectTemplate.h"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/commons/utils/Exception.h"
#include "aion/commons/utils/Rnd.h"
#include "aion/gameserver/ai/AbstractAI.h"
#include "aion/gameserver/ai/poll/AIQuestion.h"
#include "aion/gameserver/configs/main/CustomConfig.h"
#include "aion/gameserver/controllers/ObserveController.h"
#include "aion/gameserver/controllers/attack/AttackResult.h"
#include "aion/gameserver/controllers/effect/CumulativeResistType.h"
#include "aion/gameserver/controllers/effect/EffectController.h"
#include "aion/gameserver/controllers/effect/PlayerEffectController.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/SkillData.h"
#include "aion/gameserver/dataholders/loadingutils/EnumTraits.h"
#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/model/gameobjects/Npc.h"
#include "aion/gameserver/model/gameobjects/Summon.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/stats/calc/Stat2.h"
#include "aion/gameserver/model/stats/container/CreatureGameStats.h"
#include "aion/gameserver/model/stats/container/StatEnum.h"
#include "aion/gameserver/model/templates/detail/JavaCasts.h"
#include "aion/gameserver/model/templates/npc/NpcTemplate.h"
#include "aion/gameserver/model/templates/stats/StatsTemplate.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/skillengine/condition/Condition.h"
#include "aion/gameserver/skillengine/condition/Conditions.h"
#include "aion/gameserver/skillengine/effect/DamageEffect.h"
#include "aion/gameserver/skillengine/effect/modifier/ActionModifier.h"
#include "aion/gameserver/skillengine/effect/modifier/ActionModifiers.h"
#include "aion/gameserver/skillengine/model/Effect.h"
#include "aion/gameserver/skillengine/model/EffectReserved.h"
#include "aion/gameserver/skillengine/model/ShieldType.h"
#include "aion/gameserver/skillengine/model/SkillSubType.h"
#include "aion/gameserver/skillengine/model/SkillTemplate.h"
#include "aion/gameserver/skillengine/model/SpellStatus.h"
#include "aion/gameserver/utils/stats/StatFunctions.h"

namespace aion::gameserver::skillengine::effect {

// Java: LoggerFactory.getLogger(EffectTemplate.class) inside getPenetrationStat (hub-headers.md §11.3)
static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.skillengine.effect.EffectTemplate");

using controllers::attack::AttackResult;
using controllers::effect::CumulativeResistType;
using gameserver::model::SkillElement;
using gameserver::model::gameobjects::Creature;
using gameserver::model::stats::container::StatEnum;
using runtime::Ptr;
using runtime::Ref;

namespace {

/** Java's implicit null check of a dereference of a static template pointer (a plain C++ dereference of nullptr is undefined) */
template <class T>
const T& nonNull(const T* value, const char* what) {
	if (value == nullptr)
		throw runtime::NullPointerException(std::string(what) + " is null");
	return *value;
}

/** Java int a * b (wraps on overflow) */
constexpr int32_t mulInt(int32_t a, int32_t b) noexcept {
	return static_cast<int32_t>(static_cast<uint32_t>(a) * static_cast<uint32_t>(b));
}

/** Java int a + b (wraps on overflow) */
constexpr int32_t addInt(int32_t a, int32_t b) noexcept {
	return static_cast<int32_t>(static_cast<uint32_t>(a) + static_cast<uint32_t>(b));
}

/** Java int a - b (wraps on overflow) */
constexpr int32_t subInt(int32_t a, int32_t b) noexcept {
	return static_cast<int32_t>(static_cast<uint32_t>(a) - static_cast<uint32_t>(b));
}

/**
 * Java Conditions.validate(Effect) (Conditions.java:66-73, P5-02a): every condition must hold, and the first that does not ends the loop.
 * condition/Conditions.h declares no validate(Effect&) (header request, docs/deviations/P5-03.md), so its loop is ported here, the way
 * StatFunction.cpp ports Conditions.validate(Stat2, IStatFunction). Replace with `conditions.validate(effect)` once the declaration lands.
 */
bool validateConditions(const condition::Conditions& conditions, model::Effect& effect) {
	for (const std::unique_ptr<condition::Condition>& condition : conditions.getConditions()) {
		if (!condition->validate(effect)) {
			return false;
		}
	}
	return true;
}

/**
 * Java CumulativeResistType.get(StatEnum) (CumulativeResistType.java:11-18, P5-02b): no C++ companion declares it (header request,
 * docs/deviations/P5-03.md). nullopt is Java's null.
 */
std::optional<CumulativeResistType> cumulativeResistTypeOf(StatEnum stat) {
	switch (stat) {
		case StatEnum::FEAR_RESISTANCE:
			return CumulativeResistType::FEAR;
		case StatEnum::PARALYZE_RESISTANCE:
			return CumulativeResistType::PARALYZE;
		case StatEnum::SLEEP_RESISTANCE:
			return CumulativeResistType::SLEEP;
		default:
			return std::nullopt;
	}
}

} // namespace

const modifier::ActionModifier* EffectTemplate::getActionModifiers(model::Effect& effect) const {
	if (modifiers == nullptr)
		return nullptr;

	// Only one of modifiers will be applied now
	for (const std::unique_ptr<modifier::ActionModifier>& modifier : modifiers->getActionModifiers()) {
		if (modifier->check(effect))
			return modifier.get();
	}

	return nullptr;
}

int32_t EffectTemplate::calculateBaseValue(model::Effect& effect) const {
	return addInt(value, mulInt(delta, effect.getSkillLevel()));
}

int32_t EffectTemplate::calculateCritAddDmg(model::Effect& effect) const {
	return addInt(critAddDmg2, mulInt(critAddDmg1, effect.getSkillLevel()));
}

int32_t EffectTemplate::calculateCritProbMod(model::Effect& effect) const {
	return addInt(critProbMod2, mulInt(critProbMod1, effect.getSkillLevel()));
}

void EffectTemplate::calculate(model::Effect& effect) const {
	calculate(effect, std::nullopt, std::nullopt, element);
}

bool EffectTemplate::calculate(model::Effect& effect, std::optional<gameserver::model::stats::container::StatEnum> statEnum,
	std::optional<model::SpellStatus> spellStatus) const {
	return calculate(effect, statEnum, spellStatus, element);
}

bool EffectTemplate::calculate(model::Effect& effect, std::optional<gameserver::model::stats::container::StatEnum> statEnum,
	std::optional<model::SpellStatus> spellStatus, gameserver::model::SkillElement /*elementValue*/) const {
	// Java: the `element` parameter shadows the field and this body never reads it; checkDodgeOrResistRate reads the field (EffectTemplate.java:358)
	if (nonNull(effect.getSkillTemplate(), "skillTemplate").isPassive()) {
		addSuccessEffect(effect, spellStatus);
		return true;
	}

	if (statEnum && isAlteredState(*statEnum) && isImmuneToAbnormal(effect, *statEnum)) {
		return false;
	}

	bool isForcedEffect = effect.isForcedEffect();
	if (!isForcedEffect && (!validateEffectConditions(effect) || !validatePreEffects(effect))) {
		effect.resetMagicalCritical(); // a filtered out effect breaks the chain, so the next position rolls its own critical
		return false;
	}
	resolveMagicalCritical(effect);
	if (!isForcedEffect && isDodgedOrResisted(effect, statEnum)) {
		if (getPosition() != 1 && dynamic_cast<const DamageEffect*>(effect.effectInPos(1)) == nullptr)
			effect.getSuccessEffects().clear();
		return false;
	}
	addSuccessEffect(effect, spellStatus);
	calculateDamage(effect);
	return true;
}

void EffectTemplate::resolveMagicalCritical(model::Effect& /*effect*/) const {
	// Java: empty body
}

bool EffectTemplate::validateEffectConditions(model::Effect& effect) const {
	return effectConditions == nullptr || validateConditions(*effectConditions, effect);
}

bool EffectTemplate::validatePreEffects(model::Effect& effect) const {
	if (getPreEffects()) {
		for (int32_t preEffectPosition : *getPreEffects()) {
			if (!effect.isInSuccessEffects(preEffectPosition))
				return false;
		}
		if (commons::utils::Rnd::chance() >= static_cast<float>(getPreEffectProb()))
			return false;
	}
	return true;
}

bool EffectTemplate::isDodgedOrResisted(model::Effect& effect, std::optional<gameserver::model::stats::container::StatEnum> statEnum) const {
	return !isNoResist() && (!checkEffectResistRate(effect, statEnum) || !checkDodgeOrResistRate(effect));
}

bool EffectTemplate::checkDodgeOrResistRate(model::Effect& effect) const {
	int32_t accuracyModifier = addInt(addInt(accMod2, mulInt(accMod1, effect.getSkillLevel())), effect.getAccModBoost());
	if (nonNull(effect.getSkillTemplate(), "skillTemplate").getSubType() == model::SkillSubType::DEBUFF)
		accuracyModifier = addInt(accuracyModifier, effect.getEffector()->getGameStats()->getStat(StatEnum::BOOST_RESIST_DEBUFF, 0)->getCurrent());
	if (element == SkillElement::NONE)
		return !utils::stats::StatFunctions::checkIsDodgedHit(*effect.getEffector(), *effect.getEffected(), accuracyModifier);
	// Java evaluates the left operand first: the roll happens before the resist rate is calculated (C++ leaves the operand order unsequenced)
	int32_t roll = commons::utils::Rnd::get(1, 1000);
	return roll > utils::stats::StatFunctions::calculateMagicalResistRate(*effect.getEffector(), *effect.getEffected(), accuracyModifier, element);
}

void EffectTemplate::addSuccessEffect(model::Effect& effect, std::optional<model::SpellStatus> spellStatus) const {
	effect.addSuccessEffect(this);
	if (spellStatus)
		effect.setSpellStatus(*spellStatus);
}

void EffectTemplate::startEffect(model::Effect& /*effect*/) const {
	// Java: empty body
}

void EffectTemplate::calculateDamage(model::Effect& effect) const {
	// evaluate skill reflect for non-dmg skills
	Ref<AttackResult> attackResult = AttackResult::create(0, effect.getAttackStatus(), hitType);
	effect.getEffected()->getObserveController()->checkShieldStatus(std::vector<Ptr<AttackResult>>{attackResult}, effect, *effect.getEffector(),
		model::ShieldType::SKILL_REFLECTOR);
	if (attackResult->getShieldType() != 0) {
		effect.setShieldDefense(effect.getShieldDefense() | attackResult->getShieldType());
		effect.setReflectedSkillId(attackResult->getReflectedSkillId());
	}
}

void EffectTemplate::calculateSubEffect(model::Effect& effect) const {
	if (subEffect == nullptr)
		return;
	const modifier::ActionModifiers* mod = getModifiers();
	if (mod != nullptr) {
		const modifier::ActionModifier* modifier = getActionModifiers(effect);
		if (modifier == nullptr) {
			return;
		}
	}
	// Pre-Check for sub effect conditions
	if (!effectSubConditionsCheck(effect)) {
		effect.setSubEffectAborted(true);
		return;
	}

	// chance to trigger subeffect
	if (commons::utils::Rnd::chance() >= static_cast<float>(subEffect->getChance()))
		return;

	// a missing template (null) goes on to the Effect constructor, whose skillTemplate.getReqDispelCount() throws Java's NullPointerException in
	// C++ as well (requireTemplate, Effect.cpp)
	const model::SkillTemplate* skillTemplate = dataholders::DataManager::SKILL_DATA->getSkillTemplate(subEffect->getSkillId());
	int32_t level = 1;
	int32_t accBoost = effect.getAccModBoost();
	if (subEffect->isAddEffect()) { // Only used by signet bursts
		level = effect.getSignetBurstedCount();
		accBoost = std::numeric_limits<int16_t>::max(); // sub effects cannot be resisted by magic resist in case of signet bursts
	}
	Ref<model::Effect> newEffect = model::Effect::create(*effect.getEffector(), effect.getOriginalEffected(), skillTemplate, level, std::nullopt,
		effect.getForceType(), true, nullptr);
	newEffect->setShieldDefense(effect.getShieldDefense());
	newEffect->setAccModBoost(accBoost);
	newEffect->initialize();
	if (newEffect->getSpellStatus() != model::SpellStatus::DODGE && newEffect->getSpellStatus() != model::SpellStatus::RESIST)
		effect.setSpellStatus(newEffect->getSpellStatus());
	effect.setSubEffect(newEffect);
	effect.setSubEffectType(newEffect->getSubEffectType());
	effect.setTargetLoc(newEffect->getTargetX(), newEffect->getTargetY(), newEffect->getTargetZ());
}

bool EffectTemplate::effectSubConditionsCheck(model::Effect& effect) const {
	return effectSubConditions == nullptr || validateConditions(*effectSubConditions, effect);
}

int32_t EffectTemplate::calculateHate(model::Effect& effect) const {
	if (hopType) {
		int32_t hate = 0;
		switch (*hopType) {
			case model::HopType::DAMAGE:
				hate = effect.getReserveds(0)->getValue();
				[[fallthrough]];
			case model::HopType::SKILLLV: {
				int32_t skillLvl = effect.getSkillLevel();
				hate = addInt(hate, addInt(hopB, mulInt(hopA, skillLvl))); // Aggro-value of the effect
				break;
			}
			default:
				throw commons::utils::UnsupportedOperationException(
					"Unhandled effect type " + std::string(xml::enumName(*hopType)) + " for hate calculation");
		}
		return std::max(1, hate);
	}
	return 0;
}

void EffectTemplate::startSubEffect(model::Effect& effect) const {
	if (subEffect == nullptr)
		return;
	// Apply-Check for sub effect conditions
	if (effect.isSubEffectAbortedBySubConditions())
		return;
	if (effect.getSubEffect() != nullptr)
		effect.getSubEffect()->applyEffect();
}

void EffectTemplate::onPeriodicAction(model::Effect& /*effect*/) const {
	// Java: empty body
}

void EffectTemplate::endEffect(model::Effect& /*effect*/) const {
	// Java: empty body
}

bool EffectTemplate::checkEffectResistRate(model::Effect& effect, std::optional<gameserver::model::stats::container::StatEnum> statEnum) const {
	if (!statEnum)
		return true;

	Ptr<Creature> effected = effect.getEffected();
	Ptr<Creature> effector = effect.getEffector();

	if (effected == nullptr || effected->getGameStats() == nullptr || effector == nullptr || effector->getGameStats() == nullptr)
		return false;

	// Stun like effects cannot be applied as long as a shield is active
	if (isProtectedByShield(*effected, *statEnum))
		return false;

	int32_t effectPower = 1000;

	if (isAlteredState(*statEnum))
		effectPower = subInt(effectPower, effected->getGameStats()->getAbnormalResistance()->getCurrent());

	// effect resistance
	effectPower = subInt(effectPower, effected->getGameStats()->getResistance(*statEnum)->getCurrent());

	// calculate cumulative resist chance for fear, sleep and paralyze if effector and effected are players
	bool isEffectorPlayer = runtime::as<gameserver::model::gameobjects::player::Player>(
								configs::main::CustomConfig::COUNT_SUMMON_EFFECTS_FOR_CUMULATIVE_RESIST.load() ? effector->getMaster() : effector)
		!= nullptr;
	if (isEffectorPlayer) {
		if (Ptr<gameserver::model::gameobjects::player::Player> player = runtime::as<gameserver::model::gameobjects::player::Player>(effected)) {
			// Java passes CumulativeResistType.get(statEnum) as it is; for every other stat it is null and getCumulativeResistance(null) answers 0
			// (EnumMap.get(null) is null, PlayerEffectController.java:149-153). The C++ parameter is not nullable (header request,
			// docs/deviations/P5-03.md), so that arm subtracts the 0 without the call.
			std::optional<CumulativeResistType> cumulativeResistType = cumulativeResistTypeOf(*statEnum);
			effectPower = subInt(effectPower, cumulativeResistType ? player->getEffectController()->getCumulativeResistance(*cumulativeResistType) : 0);
		}
	}

	// penetration
	std::optional<StatEnum> penetrationStat = getPenetrationStat(*statEnum);
	if (penetrationStat)
		effectPower = addInt(effectPower, effector->getGameStats()->getStat(*penetrationStat, 0)->getCurrent());

	// resist mod
	if (effectPower > 0 && effector->isPvpTarget(*effected)) { // pvp
		int32_t lvlDiff = effected->getLevel() - effector->getLevel();
		if (lvlDiff > 4) {
			float reductionRate = 0.1f * static_cast<float>(lvlDiff - 4); // see https://forums.aiononline.com/topic/25-arena-of-discipline-entries/?page=2#elComment_2213
			// Java: `effectPower *= Math.max(1 - reductionRate, 0.1f)` - a compound assignment, so the float product is narrowed by (int), which
			// truncates towards zero (JLS 15.26.2)
			effectPower = gameserver::model::templates::detail::floatToInt(static_cast<float>(effectPower) * std::max(1 - reductionRate, 0.1f));
		}
	}
	return commons::utils::Rnd::get(1, 1000) <= effectPower;
}

bool EffectTemplate::isImmuneToAbnormal(model::Effect& effect, gameserver::model::stats::container::StatEnum statEnum) const {
	Ptr<Creature> effected = effect.getEffected();
	if (effected != effect.getEffector()) {
		Ptr<gameserver::model::gameobjects::Npc> npc = runtime::as<gameserver::model::gameobjects::Npc>(effected);
		Ptr<gameserver::model::gameobjects::Summon> summon = runtime::as<gameserver::model::gameobjects::Summon>(effected);
		if (npc || summon) {
			if (effected->getAi().ask(ai::poll::AIQuestion::IS_IMMUNE_TO_ABNORMAL_STATES))
				return true;
			// Java: ((NpcTemplate) effected.getObjectTemplate()) - the narrowing accessor of the concrete class is that cast
			const gameserver::model::templates::npc::NpcTemplate* npcTemplate = npc ? npc->getObjectTemplate() : summon->getObjectTemplate();
			if (nonNull(nonNull(npcTemplate, "objectTemplate").getStatsTemplate(), "statsTemplate").getRunSpeed() == 0)
				return statEnum == StatEnum::PULLED_RESISTANCE || statEnum == StatEnum::STAGGER_RESISTANCE || statEnum == StatEnum::STUMBLE_RESISTANCE;
		}
	}
	return false;
}

bool EffectTemplate::isAlteredState(gameserver::model::stats::container::StatEnum stat) const {
	switch (stat) {
		case StatEnum::BLEED_RESISTANCE:
		case StatEnum::POISON_RESISTANCE:
			return false;
		default:
			return true;
	}
}

bool EffectTemplate::isProtectedByShield(gameserver::model::gameobjects::Creature& effected, gameserver::model::stats::container::StatEnum stat) const {
	switch (stat) {
		case StatEnum::STUMBLE_RESISTANCE:
		case StatEnum::OPENAERIAL_RESISTANCE:
		case StatEnum::SPIN_RESISTANCE:
		case StatEnum::STAGGER_RESISTANCE:
			return effected.getEffectController()->isUnderNormalShield();
		default:
			return false;
	}
}

std::optional<gameserver::model::stats::container::StatEnum> EffectTemplate::getPenetrationStat(gameserver::model::stats::container::StatEnum statEnum) const {
	// Java: StatEnum.valueOf(statEnum.toString() + "_PENETRATION"), whose IllegalArgumentException is caught, logged and answered with null
	std::optional<StatEnum> toReturn = xml::enumFromName<StatEnum>(std::string(xml::enumName(statEnum)) + "_PENETRATION");
	if (!toReturn)
		log.warn("Missing statenum penetration for " + std::string(xml::enumName(statEnum)));
	return toReturn;
}

} // namespace aion::gameserver::skillengine::effect
