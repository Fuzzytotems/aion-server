#include "aion/gameserver/model/stats/container/CreatureGameStats.h"

#include <algorithm>

#include "aion/gameserver/model/EmotionType.h"
#include "aion/gameserver/model/SkillElement.h"
#include "aion/gameserver/model/enchants/EnchantEffect.h"
#include "aion/gameserver/model/gameobjects/AionObject.h"
#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/model/gameobjects/Item.h"
#include "aion/gameserver/model/items/ItemSlot.h"
#include "aion/gameserver/model/items/ManaStone.h"
#include "aion/gameserver/model/items/RandomBonusEffect.h"
#include "aion/gameserver/model/stats/calc/AdditionStat.h"
#include "aion/gameserver/model/stats/calc/ReverseStat.h"
#include "aion/gameserver/model/stats/calc/Stat2.h"
#include "aion/gameserver/model/stats/calc/StatCapUtil.h"
#include "aion/gameserver/model/stats/calc/StatOwner.h"
#include "aion/gameserver/model/stats/calc/functions/IStatFunction.h"
#include "aion/gameserver/model/stats/calc/functions/StatArmorMasteryFunction.h"
#include "aion/gameserver/model/stats/calc/functions/StatFunctionProxy.h"
#include "aion/gameserver/model/stats/container/CreatureLifeStats.h"
#include "aion/gameserver/model/stats/container/StatEnum.h"
#include "aion/gameserver/model/templates/itemset/ItemSetTemplate.h"
#include "aion/gameserver/model/templates/stats/StatsTemplate.h"
#include "aion/gameserver/network/aion/serverpackets/SM_EMOTION.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/skillengine/model/Effect.h"
#include "aion/gameserver/utils/JavaMath.h"
#include "aion/gameserver/utils/PacketSendUtility.h"
#include "aion/gameserver/utils/stats/CalculationType.h"

namespace aion::gameserver::model::stats::container {

using calc::Stat2;
using calc::functions::IStatFunction;
using runtime::Ptr;
using runtime::Ref;
using utils::stats::CalculationType;

namespace {

using StatFunctionList = runtime::RcArrayList<Ref<IStatFunction>>;

/** Java: getStatsTemplate().getX() - a NullPointerException where the creature has no stats template */
const templates::stats::StatsTemplate& nonNull(const templates::stats::StatsTemplate* statsTemplate) {
	if (statsTemplate == nullptr)
		throw runtime::NullPointerException("the creature has no stats template");
	return *statsTemplate;
}

/**
 * Java Objects.equals(a, b) / a.equals(b) of stat owners: identity, except that AionObject (Item) overrides equals with the object id. The other
 * owner types (effects, stones, templates, bonus effects) keep Object.equals.
 */
bool ownersEqual(Ptr<calc::StatOwner> a, Ptr<calc::StatOwner> b) {
	if (a.get() == b.get())
		return true;
	if (!a || !b)
		return false;
	const auto* objectA = dynamic_cast<const gameobjects::AionObject*>(a.get());
	const auto* objectB = dynamic_cast<const gameobjects::AionObject*>(b.get());
	return objectA != nullptr && objectB != nullptr && objectA->equals(*objectB);
}

} // namespace

CreatureGameStats::CreatureGameStats(gameobjects::Creature& value) : runtime::OwnedPart(value), owner(value) {
}

CreatureGameStats::~CreatureGameStats() = default;

void CreatureGameStats::setAttackCounter(int32_t value) {
	if (value <= 0) {
		this->attackCounter = 1;
	} else {
		this->attackCounter = value;
	}
}

void CreatureGameStats::increaseAttackCounter() {
	if (attackCounter.get() == ATTACK_MAX_COUNTER) {
		this->attackCounter = 1;
	} else {
		this->attackCounter = attackCounter.get() + 1;
	}
}

void CreatureGameStats::addEffectOnly(Ptr<calc::StatOwner> statOwner, const std::vector<Ptr<IStatFunction>>& functions) {
	for (const Ptr<IStatFunction>& function : functions) {
		Ref<IStatFunction> functionToAdd =
			ownersEqual(statOwner, function->getOwner()) ? Ref<IStatFunction>(function) : Ref<IStatFunction>(calc::functions::StatFunctionProxy::create(statOwner, *function));
		stats.compute(functionToAdd->getName(), [&functionToAdd](const Ptr<StatFunctionList>& statFunctions) -> Ref<StatFunctionList> {
			if (!statFunctions) {
				Ref<StatFunctionList> newFunctions = StatFunctionList::create(AION_LOCK_CLASS(CreatureGameStats::stats));
				newFunctions->add(functionToAdd);
				return newFunctions;
			}
			SYNCHRONIZED(*statFunctions) {
				statFunctions->add(functionToAdd);
				statFunctions->sort(nullptr);
			}
			return Ref<StatFunctionList>(statFunctions);
		});
	}
}

void CreatureGameStats::addEffect(Ptr<calc::StatOwner> statOwner, const std::vector<Ptr<IStatFunction>>& functions) {
	addEffectOnly(statOwner, functions);
	onStatsChange(runtime::as<skillengine::model::Effect>(statOwner));
}

void CreatureGameStats::endEffect(calc::StatOwner& statOwner) {
	bool statsChanged = false;
	Ptr<calc::StatOwner> endedOwner(statOwner);
	for (const auto& functions : stats.values()) {
		SYNCHRONIZED(*functions) {
			statsChanged |=
				functions->removeIf([&endedOwner](const Ptr<IStatFunction>& statFunction) { return ownersEqual(endedOwner, statFunction->getOwner()); });
		}
	}
	if (statsChanged && !owner.isDead())
		onStatsChange(nullptr);
}

// lint: L7 C++-only breaker (no Java body): it takes the list locks like endEffect
void CreatureGameStats::clearEffectFunctionsWithoutNotify() {
	for (const auto& functions : stats.values()) {
		SYNCHRONIZED(*functions) {
			functions->removeIf([](const Ptr<IStatFunction>& statFunction) {
				return runtime::as<skillengine::model::Effect>(statFunction->getOwner()) != nullptr;
			});
		}
	}
}

float CreatureGameStats::getPositiveStat(StatEnum statEnum, float base) {
	std::unique_ptr<Stat2> stat = getStat(statEnum, base);
	return static_cast<float>(std::max(0, stat->getCurrent()));
}

int32_t CreatureGameStats::getPositiveReverseStat(StatEnum statEnum, int32_t base) {
	std::unique_ptr<Stat2> stat = getReverseStat(statEnum, static_cast<float>(base));
	return std::max(0, stat->getCurrent());
}

std::unique_ptr<Stat2> CreatureGameStats::getStat(StatEnum statEnum, float base) {
	static const std::unordered_set<CalculationType> EMPTY_SET; // Java Collections.emptySet()
	return getStat(statEnum, base, EMPTY_SET);
}

std::unique_ptr<Stat2> CreatureGameStats::getStat(StatEnum statEnum, float base, const std::unordered_set<CalculationType>& calculationTypes) {
	std::unique_ptr<Stat2> stat = std::make_unique<calc::AdditionStat>(statEnum, base, owner);
	applyStatFunctions(statEnum, *stat, calculationTypes);
	return stat;
}

std::unique_ptr<Stat2> CreatureGameStats::getReverseStat(StatEnum statEnum, float base) {
	static const std::unordered_set<CalculationType> EMPTY_SET; // Java Collections.emptySet()
	std::unique_ptr<Stat2> stat = std::make_unique<calc::ReverseStat>(statEnum, base, owner);
	applyStatFunctions(statEnum, *stat, EMPTY_SET);
	return stat;
}

Stat2& CreatureGameStats::applyStatFunctions(StatEnum statEnum, Stat2& stat, const std::unordered_set<CalculationType>& calculationTypes) {
	for (const Ptr<IStatFunction>& func : getStatsSorted(statEnum)) {
		if (func->validate(stat)) {
			Ptr<enchants::EnchantEffect> ef;
			if ((statEnum == StatEnum::PHYSICAL_ATTACK || statEnum == StatEnum::MAGICAL_ATTACK)
				&& (ef = runtime::as<enchants::EnchantEffect>(func->getOwner()))) {
				if ((ef->getItemSlot() == items::ItemSlot::MAIN_HAND && calculationTypes.contains(CalculationType::MAIN_HAND))
					|| (ef->getItemSlot() == items::ItemSlot::SUB_HAND && calculationTypes.contains(CalculationType::OFF_HAND))) {
					func->apply(stat, calculationTypes);
				}
			} else {
				func->apply(stat, calculationTypes);
			}
		}
	}
	calc::StatCapUtil::calculateBaseValue(stat, owner);
	return stat;
}

Stat2& CreatureGameStats::getItemStatBoost(StatEnum statEnum, Stat2& stat) {
	static const std::unordered_set<CalculationType> EMPTY_SET; // Java Collections.emptySet()
	for (const Ptr<IStatFunction>& func : getStatsSorted(statEnum)) {
		if (func->isBonus() && func->validate(stat)) {
			Ptr<calc::StatOwner> funcOwner = func->getOwner();
			if (runtime::as<gameobjects::Item>(funcOwner) || runtime::as<items::ManaStone>(funcOwner) || runtime::as<templates::itemset::ItemSetTemplate>(funcOwner)
				|| runtime::as<items::RandomBonusEffect>(funcOwner)) {
				func->apply(stat, EMPTY_SET);
			}
		}
	}
	return stat;
}

std::unique_ptr<Stat2> CreatureGameStats::getPower() {
	return getStat(StatEnum::POWER, static_cast<float>(nonNull(getStatsTemplate()).getPower()));
}

std::unique_ptr<Stat2> CreatureGameStats::getHealth() {
	return getStat(StatEnum::HEALTH, static_cast<float>(nonNull(getStatsTemplate()).getHealth()));
}

std::unique_ptr<Stat2> CreatureGameStats::getAccuracy() {
	return getStat(StatEnum::ACCURACY, static_cast<float>(nonNull(getStatsTemplate()).getBaseAccuracy()));
}

std::unique_ptr<Stat2> CreatureGameStats::getAgility() {
	return getStat(StatEnum::AGILITY, static_cast<float>(nonNull(getStatsTemplate()).getAgility()));
}

std::unique_ptr<Stat2> CreatureGameStats::getKnowledge() {
	return getStat(StatEnum::KNOWLEDGE, static_cast<float>(nonNull(getStatsTemplate()).getKnowledge()));
}

std::unique_ptr<Stat2> CreatureGameStats::getWill() {
	return getStat(StatEnum::WILL, static_cast<float>(nonNull(getStatsTemplate()).getWill()));
}

std::unique_ptr<Stat2> CreatureGameStats::getMaxHp() {
	return getStat(StatEnum::MAXHP, static_cast<float>(nonNull(getStatsTemplate()).getMaxHp()));
}

std::unique_ptr<Stat2> CreatureGameStats::getMaxMp() {
	return getStat(StatEnum::MAXMP, static_cast<float>(nonNull(getStatsTemplate()).getMaxMp()));
}

std::unique_ptr<Stat2> CreatureGameStats::getPDef() {
	return getStat(StatEnum::PHYSICAL_DEFENSE, static_cast<float>(nonNull(getStatsTemplate()).getPdef()));
}

std::unique_ptr<Stat2> CreatureGameStats::getMDef() {
	return getStat(StatEnum::MAGICAL_DEFEND, static_cast<float>(nonNull(getStatsTemplate()).getMdef()));
}

std::unique_ptr<Stat2> CreatureGameStats::getEvasion() {
	return getStat(StatEnum::EVASION, static_cast<float>(nonNull(getStatsTemplate()).getEvasion()));
}

std::unique_ptr<Stat2> CreatureGameStats::getParry() {
	return getStat(StatEnum::PARRY, static_cast<float>(nonNull(getStatsTemplate()).getParry()));
}

std::unique_ptr<Stat2> CreatureGameStats::getBlock() {
	return getStat(StatEnum::BLOCK, static_cast<float>(nonNull(getStatsTemplate()).getBlock()));
}

std::unique_ptr<Stat2> CreatureGameStats::getMResist() {
	return getStat(StatEnum::MAGICAL_RESIST, static_cast<float>(nonNull(getStatsTemplate()).getMresist()));
}

std::unique_ptr<Stat2> CreatureGameStats::getPCR() {
	return getStat(StatEnum::PHYSICAL_CRITICAL_RESIST, static_cast<float>(nonNull(getStatsTemplate()).getStrikeResist()));
}

std::unique_ptr<Stat2> CreatureGameStats::getMCR() {
	return getStat(StatEnum::MAGICAL_CRITICAL_RESIST, static_cast<float>(nonNull(getStatsTemplate()).getSpellResist()));
}

std::unique_ptr<Stat2> CreatureGameStats::getMainHandPAttack(std::initializer_list<CalculationType> calculationTypes) {
	return getMainHandPAttack(toSet(std::span<const CalculationType>(calculationTypes.begin(), calculationTypes.size())));
}

std::unique_ptr<Stat2> CreatureGameStats::getMainHandPAttack(const std::unordered_set<CalculationType>& calculationTypes) {
	return getStat(StatEnum::PHYSICAL_ATTACK, static_cast<float>(nonNull(getStatsTemplate()).getAttack()), calculationTypes);
}

std::unique_ptr<Stat2> CreatureGameStats::getMainHandPCritical() {
	return getStat(StatEnum::PHYSICAL_CRITICAL, static_cast<float>(nonNull(getStatsTemplate()).getPcrit()));
}

std::unique_ptr<Stat2> CreatureGameStats::getMainHandPAccuracy() {
	return getStat(StatEnum::PHYSICAL_ACCURACY, static_cast<float>(nonNull(getStatsTemplate()).getAccuracy()));
}

std::unique_ptr<Stat2> CreatureGameStats::getMainHandMAttack(std::initializer_list<CalculationType> calculationTypes) {
	return getMainHandMAttack(toSet(std::span<const CalculationType>(calculationTypes.begin(), calculationTypes.size())));
}

std::unique_ptr<Stat2> CreatureGameStats::getMainHandMAttack(const std::unordered_set<CalculationType>& calculationTypes) {
	return getStat(StatEnum::MAGICAL_ATTACK, static_cast<float>(nonNull(getStatsTemplate()).getMagicalAttack()), calculationTypes);
}

std::unique_ptr<Stat2> CreatureGameStats::getMCritical() {
	return getStat(StatEnum::MAGICAL_CRITICAL, static_cast<float>(nonNull(getStatsTemplate()).getMcrit()));
}

std::unique_ptr<Stat2> CreatureGameStats::getMAccuracy() {
	return getStat(StatEnum::MAGICAL_ACCURACY, static_cast<float>(nonNull(getStatsTemplate()).getMacc()));
}

std::unique_ptr<Stat2> CreatureGameStats::getMBoost() {
	return getStat(StatEnum::BOOST_MAGICAL_SKILL, static_cast<float>(nonNull(getStatsTemplate()).getMagicBoost()));
}

std::unique_ptr<Stat2> CreatureGameStats::getMBResist() {
	return getStat(StatEnum::MAGIC_SKILL_BOOST_RESIST, static_cast<float>(nonNull(getStatsTemplate()).getMsup()));
}

std::unique_ptr<Stat2> CreatureGameStats::getAbnormalResistance() {
	return getStat(StatEnum::ABNORMAL_RESISTANCE_ALL, static_cast<float>(nonNull(getStatsTemplate()).getAbnormalResistance()));
}

std::unique_ptr<Stat2> CreatureGameStats::getResistance(StatEnum statEnum) {
	int32_t base;
	switch (statEnum) {
		case StatEnum::OPENAERIAL_RESISTANCE:
		case StatEnum::PARALYZE_RESISTANCE:
		case StatEnum::SPIN_RESISTANCE:
		case StatEnum::STAGGER_RESISTANCE:
		case StatEnum::STUMBLE_RESISTANCE:
		case StatEnum::STUN_RESISTANCE:
			base = nonNull(getStatsTemplate()).getStunLikeResistance();
			break;
		default:
			base = 0;
	}
	return getStat(statEnum, static_cast<float>(base));
}

std::unique_ptr<Stat2> CreatureGameStats::getAttackSpeed() {
	return getStat(StatEnum::ATTACK_SPEED, static_cast<float>(getBaseAttackSpeed()));
}

float CreatureGameStats::getAttackSpeedRate() {
	return static_cast<float>(getAttackSpeed()->getCurrent()) / getBaseAttackSpeed();
}

int32_t CreatureGameStats::getElementalDefenseFor(SkillElement element) {
	switch (element) {
		case SkillElement::EARTH:
			return getStat(StatEnum::EARTH_RESISTANCE, 0)->getCurrent();
		case SkillElement::FIRE:
			return getStat(StatEnum::FIRE_RESISTANCE, 0)->getCurrent();
		case SkillElement::WATER:
			return getStat(StatEnum::WATER_RESISTANCE, 0)->getCurrent();
		case SkillElement::WIND:
			return getStat(StatEnum::WIND_RESISTANCE, 0)->getCurrent();
		case SkillElement::LIGHT:
			return getStat(StatEnum::ELEMENTAL_RESISTANCE_LIGHT, 0)->getCurrent();
		case SkillElement::DARK:
			return getStat(StatEnum::ELEMENTAL_RESISTANCE_DARK, 0)->getCurrent();
		default:
			return 0;
	}
}

float CreatureGameStats::getMovementSpeedFloat() {
	return getMovementSpeed()->getCurrent() / 1000.0f;
}

void CreatureGameStats::updateArmorMasteryStats(const std::vector<Ptr<gameobjects::Item>>& equipment) {
	for (const auto& statFunctions : stats.values()) {
		for (Ptr<IStatFunction> statFunction : *statFunctions) {
			if (Ptr<calc::functions::StatFunctionProxy> proxy = runtime::as<calc::functions::StatFunctionProxy>(statFunction))
				statFunction = proxy->getProxiedFunction();
			if (Ptr<calc::functions::StatArmorMasteryFunction> armorMasteryFunction = runtime::as<calc::functions::StatArmorMasteryFunction>(statFunction))
				armorMasteryFunction->updateEquipmentFactor(equipment);
		}
	}
}

void CreatureGameStats::updateStatInfo() {
}

void CreatureGameStats::updateSpeedInfo() {
	utils::PacketSendUtility::broadcastPacket(owner, network::aion::serverpackets::SM_EMOTION(owner, EmotionType::CHANGE_SPEED));
}

bool CreatureGameStats::checkSpeedStats() {
	int32_t currentSpeed = getMovementSpeed()->getCurrent();
	if (currentSpeed != cachedSpeed.get()) {
		updateSpeedInfo();
		cachedSpeed = currentSpeed;
		return true;
	}
	return false;
}

std::vector<Ptr<IStatFunction>> CreatureGameStats::getStatsSorted(StatEnum stat) {
	Ptr<StatFunctionList> statFunctions = stats.get(stat);
	if (!statFunctions)
		return {};
	SYNCHRONIZED(*statFunctions) {
		return statFunctions->snapshot();
	}
}

void CreatureGameStats::onStatsChange(Ptr<skillengine::model::Effect> effect) {
	checkMaxHPChanged(effect);
	checkMaxMPChanged(effect);
}

void CreatureGameStats::checkMaxHPChanged(Ptr<skillengine::model::Effect> effect) {
	SYNCHRONIZED(*this) {
		int32_t oldMaxHp = cachedMaxHp.get() != 0 ? cachedMaxHp.get() : nonNull(getStatsTemplate()).getMaxHp();
		int32_t currentMaxHp = getMaxHp()->getCurrent();
		cachedMaxHp = currentMaxHp;
		if (oldMaxHp != currentMaxHp) {
			float percent = 1.0f * currentMaxHp / oldMaxHp;
			int32_t newHp = std::min(utils::JavaMath::round(owner.getLifeStats()->getCurrentHp() * percent), currentMaxHp);
			gameobjects::Creature& effector = effect ? *effect->getEffector() : owner;
			owner.getLifeStats()->setCurrentHp(newHp, effector);
		}
	}
}

void CreatureGameStats::checkMaxMPChanged(Ptr<skillengine::model::Effect> effect) {
	SYNCHRONIZED(*this) {
		int32_t oldMaxMp = cachedMaxMp.get() != 0 ? cachedMaxMp.get() : nonNull(getStatsTemplate()).getMaxMp();
		int32_t currentMaxMp = getMaxMp()->getCurrent();
		cachedMaxMp = currentMaxMp;
		if (oldMaxMp != currentMaxMp) {
			float percent = 1.0f * currentMaxMp / oldMaxMp;
			owner.getLifeStats()->setCurrentMp(std::min(utils::JavaMath::round(owner.getLifeStats()->getCurrentMp() * percent), currentMaxMp));
		}
	}
}

std::unordered_set<CalculationType> CreatureGameStats::toSet(std::span<const CalculationType> calculationTypes) {
	return std::unordered_set<CalculationType>(calculationTypes.begin(), calculationTypes.end());
}

std::unordered_set<CalculationType> CreatureGameStats::copyWith(const std::unordered_set<CalculationType>& types, CalculationType type) {
	std::unordered_set<CalculationType> calculationTypes(types);
	calculationTypes.insert(type);
	return calculationTypes;
}

} // namespace aion::gameserver::model::stats::container
