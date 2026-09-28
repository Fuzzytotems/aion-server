#include "aion/gameserver/utils/stats/StatFunctions.h"

#include <algorithm>
#include <cmath>
#include <memory>
#include <string>

#include "aion/commons/utils/Rnd.h"
#include "aion/gameserver/configs/main/FallDamageConfig.h"
#include "aion/gameserver/configs/main/RatesConfig.h"
#include "aion/gameserver/controllers/ObserveController.h"
#include "aion/gameserver/controllers/attack/AttackResult.h"
#include "aion/gameserver/controllers/attack/AttackStatusInfo.h"
#include "aion/gameserver/controllers/movement/PlayableMoveController.h"
#include "aion/gameserver/controllers/movement/PlayerMoveController.h"
#include "aion/gameserver/controllers/observer/AttackerCriticalStatus.h"
#include "aion/gameserver/instance/handlers/InstanceHandler.h"
#include "aion/gameserver/model/SkillElementInfo.h"
#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/model/gameobjects/Homing.h"
#include "aion/gameserver/model/gameobjects/Item.h"
#include "aion/gameserver/model/gameobjects/Npc.h"
#include "aion/gameserver/model/gameobjects/Servant.h"
#include "aion/gameserver/model/gameobjects/Summon.h"
#include "aion/gameserver/model/gameobjects/SummonedObject.h"
#include "aion/gameserver/model/gameobjects/player/Equipment.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/gameobjects/player/PlayerCommonData.h"
#include "aion/gameserver/model/gameobjects/player/RatesInfo.h"
#include "aion/gameserver/model/siege/Influence.h"
#include "aion/gameserver/model/stats/calc/Stat2.h"
#include "aion/gameserver/model/stats/calc/StatCapUtil.h"
#include "aion/gameserver/model/stats/container/CombatMode.h"
#include "aion/gameserver/model/stats/container/CreatureGameStats.h"
#include "aion/gameserver/model/stats/container/NpcGameStats.h"
#include "aion/gameserver/model/stats/container/PlayerGameStats.h"
#include "aion/gameserver/model/stats/container/PlayerLifeStats.h"
#include "aion/gameserver/model/stats/container/RatioType.h"
#include "aion/gameserver/model/stats/container/StatEnum.h"
#include "aion/gameserver/model/templates/detail/JavaCasts.h"
#include "aion/gameserver/model/templates/item/ItemTemplate.h"
#include "aion/gameserver/model/templates/item/WeaponStats.h"
#include "aion/gameserver/model/templates/item/enums/ItemSubType.h"
#include "aion/gameserver/model/templates/npc/NpcTemplate.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/skillengine/effect/EffectTemplate.h"
#include "aion/gameserver/skillengine/effect/NoReduceSpellATKInstantEffect.h"
#include "aion/gameserver/skillengine/model/HitType.h"
#include "aion/gameserver/utils/JavaMath.h"
#include "aion/gameserver/utils/stats/CalculationType.h"
#include "aion/gameserver/utils/stats/XPRewardEnumInfo.h"
#include "aion/gameserver/world/WorldMap.h"
#include "aion/gameserver/world/WorldMapInstance.h"
#include "aion/gameserver/world/WorldPosition.h"

namespace aion::gameserver::utils::stats {

using model::stats::container::StatEnum;
using model::templates::npc::NpcRating;
using runtime::Ptr;
using runtime::Ref;

namespace {

/** Java's implicit null check of a dereference: the object, NullPointerException for null, where the plain C++ dereference is undefined */
template <class T>
T& nonNull(Ptr<T> value, const char* what) {
	if (!value)
		throw runtime::NullPointerException(std::string(what) + " is null");
	return *value;
}

/** Java's implicit null check of a dereference of a static template pointer */
template <class T>
const T& nonNull(const T* value, const char* what) {
	if (value == nullptr)
		throw runtime::NullPointerException(std::string(what) + " is null");
	return *value;
}

/** Java unboxing of a nullable enum (`switch (boxed)`, `StatCapUtil.clampStatValue(element.getStatForElement(), ...)`) */
template <class T>
T unbox(const std::optional<T>& value, const char* what) {
	if (!value)
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

/** Java `RatesConfig.XP_SOLO_RATES[0]` on a snapshot of the reloadable config value (ArrayIndexOutOfBoundsException on an empty array) */
float firstXpSoloRate() {
	std::shared_ptr<const std::vector<float>> rates = configs::main::RatesConfig::XP_SOLO_RATES.get();
	if (!rates || rates->empty())
		throw runtime::IndexOutOfBoundsException("Index 0 out of bounds for length 0");
	return (*rates)[0];
}

} // namespace

int64_t StatFunctions::calculateExperienceReward(int32_t maxLevelInRange, model::gameobjects::Npc& target) {
	world::WorldMapInstance& instance = nonNull(nonNull(target.getPosition(), "position").getWorldMapInstance(), "worldMapInstance");
	int32_t baseXP = calculateBaseExp(target);
	float mapMulti = nonNull(instance.getInstanceHandler(), "instanceHandler").getExpMultiplier(); // map modifier to approach retail exp values
	if (nonNull(instance.getParent(), "parent").isInstanceType() && instance.getMaxPlayers() >= 2 && instance.getMaxPlayers() <= 6) {
		mapMulti *= static_cast<float>(instance.getMaxPlayers()); // on retail you get mob EP * max instance member count (only for group instances)
		mapMulti /= firstXpSoloRate(); // custom: divide by regular xp rates, so they will not affect the rewarded XP
	}
	int32_t xpPercentage = xpRewardFrom(target.getLevel() - maxLevelInRange);
	// Java: `long rewardXP = Math.round(baseXP * mapMulti * (xpPercentage / 100f))` - the whole expression is a float, so this is the
	// Math.round(float) overload, whose int result is then widened to long (StatFunctions.java:58)
	int64_t rewardXP = utils::JavaMath::round(static_cast<float>(baseXP) * mapMulti * (static_cast<float>(xpPercentage) / 100.0f));
	return rewardXP;
}

int32_t StatFunctions::calculateBaseExp(model::gameobjects::Npc& npc) {
	int32_t maxHp = npc.getGameStats()->getMaxHp()->getCurrent();
	if (maxHp <= 0)
		return 0;
	float multiplier;
	switch (npc.getRating()) {
		case NpcRating::JUNK:
			multiplier = 2.0f;
			break;
		case NpcRating::NORMAL:
			multiplier = 2.2f;
			break;
		case NpcRating::ELITE:
			multiplier = 4.0f;
			break;
		case NpcRating::HERO:
			multiplier = 5.4f;
			break;
		case NpcRating::LEGENDARY:
			multiplier = 6.4f;
			break;
		default:
			// Java: "Could not calculate experience reward for " + npc + " due to unknown rating." - Npc.toString() is not ported (hub-headers.md §2),
			// so the message names the npc by name and object id instead
			throw runtime::IllegalArgumentException(
				"Could not calculate experience reward for " + npc.getName() + " [" + std::to_string(npc.getObjectId()) + "] due to unknown rating.");
	}
	multiplier += static_cast<float>(static_cast<int32_t>(npc.getRank())) * 0.2f; // Java: getRank().ordinal() * 0.2f
	return utils::JavaMath::round(static_cast<float>(maxHp) * multiplier);
}

int32_t StatFunctions::calculateDPReward(model::gameobjects::player::Player& player, model::gameobjects::Creature& target) {
	int32_t playerLevel = player.getCommonData()->getLevel();
	int32_t targetLevel = target.getLevel();
	// Java: ((Npc) target).getObjectTemplate().getRating() - a ClassCastException for a non-npc target, a NullPointerException without a template.
	// The rating itself is a nullable XML attribute; Java carries the null into calculateRatingMultiplier, whose switch then throws the NPE, so
	// the unboxing here throws in exactly the cases Java does.
	NpcRating npcRating = unbox(nonNull(runtime::cast<model::gameobjects::Npc>(target)->getObjectTemplate(), "objectTemplate").getRating(), "rating");

	// TODO: fix to see monster Rating level, NORMAL lvl 1, 2 | ELITE lvl 1, 2 etc..
	// look at:
	// http://www.aionsource.com/forum/mechanic-analysis/42597-character-stats-xp-dp-origin-gerbator-team-july-2009-a.html
	int32_t baseDP = mulInt(targetLevel, calculateRatingMultiplier(npcRating));
	int32_t xpPercentage = xpRewardFrom(targetLevel - playerLevel);
	// Java: (int) Math.floor(baseDP * xpPercentage / 100f) - int multiplication, float division, then the saturating (int) cast of the double floor
	return calcResult(model::gameobjects::player::Rates::DP_PVE, player,
		model::templates::detail::floatToInt(std::floor(static_cast<float>(mulInt(baseDP, xpPercentage)) / 100.0f)));
}

int32_t StatFunctions::calculatePvEApGained(model::gameobjects::player::Player& player, model::gameobjects::Creature& target) {
	if (player.getCommonData()->getLevel() - target.getLevel() > 10)
		return 1;

	// Java: getApNpcRating(((Npc) target).getObjectTemplate().getRating()) - the nullable rating throws inside getApNpcRating's switch (see above)
	float apNpcRate = static_cast<float>(getApNpcRating(
		unbox(nonNull(runtime::cast<model::gameobjects::Npc>(target)->getObjectTemplate(), "objectTemplate").getRating(), "rating")));

	// TODO: find out why they give 1/4 AP base(normal NpcRate) (5 AP retail)
	if (target.getName() == "flame hoverstone")
		apNpcRate = 0.5f;

	return calcResult(model::gameobjects::player::Rates::AP_PVE, player, model::templates::detail::floatToInt(std::floor(15 * apNpcRate)));
}

int32_t StatFunctions::calculatePvPApLost(model::gameobjects::player::Player& defeated, model::gameobjects::player::Player& winner) {
	AION_UNPORTED();
}

int32_t StatFunctions::calculatePvpApGained(model::gameobjects::player::Player& defeated, int32_t winnerAbyssRank, int32_t maxLevel) {
	AION_UNPORTED();
}

int32_t StatFunctions::calculatePvpXpGained(model::gameobjects::player::Player& defeated, int32_t winnerAbyssRank, int32_t maxLevel) {
	AION_UNPORTED();
}

int32_t StatFunctions::calculatePvpDpGained(model::gameobjects::player::Player& defeated, int32_t maxRank, int32_t maxLevel) {
	AION_UNPORTED();
}

int32_t StatFunctions::adjustPvpDpGained(int32_t points, int32_t defeatedLvl, int32_t killerLvl) {
	AION_UNPORTED();
}

int32_t StatFunctions::calculateHate(model::gameobjects::Creature& creature, int32_t value) {
	int32_t boostHate = creature.getGameStats()->getStat(StatEnum::BOOST_HATE, 100)->getCurrent();
	// Java: (int) ((long) value * (1000 + boostHate) / 1000) - long arithmetic (1000 + boostHate wraps as an int), then a truncating (int) cast
	int64_t hate = static_cast<int64_t>(value) * addInt(1000, boostHate) / 1000;
	return static_cast<int32_t>(static_cast<uint32_t>(static_cast<uint64_t>(hate)));
}

std::vector<runtime::Ref<controllers::attack::AttackResult>> StatFunctions::calculateAttackDamage(model::gameobjects::Creature& attacker,
	model::SkillElement element, controllers::attack::AttackStatus status, const std::unordered_set<CalculationType>& calculationTypes) {
	using controllers::attack::AttackResult;
	using controllers::attack::AttackStatus;
	using model::gameobjects::player::Player;

	std::vector<Ref<AttackResult>> attackResultList;
	if (getBaseStatus(status) == AttackStatus::DODGE || getBaseStatus(status) == AttackStatus::RESIST) {
		attackResultList.push_back(AttackResult::create(0, getBaseStatus(status)));
		return attackResultList;
	}
	std::unique_ptr<model::stats::calc::Stat2> mainHandAttack;
	std::unique_ptr<model::stats::calc::Stat2> offHandAttack;
	skillengine::model::HitType hitType = skillengine::model::HitType::PHHIT;
	Ptr<Player> attackingPlayer = runtime::as<Player>(attacker);
	if (element == model::SkillElement::NONE) {
		mainHandAttack = attacker.getGameStats()->getMainHandPAttack(calculationTypes);
		if (attackingPlayer)
			offHandAttack = attackingPlayer->getGameStats()->getOffHandPAttack(calculationTypes);
	} else {
		hitType = skillengine::model::HitType::MAHIT;
		mainHandAttack = attacker.getGameStats()->getMainHandMAttack(calculationTypes);
		if (attackingPlayer)
			offHandAttack = attackingPlayer->getGameStats()->getOffHandMAttack(calculationTypes);
	}

	if (attackingPlayer) {
		Player& p = *attackingPlayer;
		model::gameobjects::player::Equipment& equipment = p.getEquipment();
		Ptr<model::gameobjects::Item> mainHandWeapon = equipment.getMainHandWeapon();
		if (mainHandWeapon) {
			Ptr<model::gameobjects::Item> offHandWeapon = equipment.getOffHandWeapon();
			const model::templates::item::WeaponStats* mainWeaponStats =
				nonNull(mainHandWeapon->getItemTemplate(), "itemTemplate").getWeaponStats();
			const model::templates::item::WeaponStats* offWeaponStats =
				(!offHandWeapon || nonNull(offHandWeapon->getItemTemplate(), "itemTemplate").getItemSubType() == model::templates::item::enums::ItemSubType::SHIELD)
					? nullptr
					: nonNull(offHandWeapon->getItemTemplate(), "itemTemplate").getWeaponStats();
			if (mainWeaponStats != nullptr) {
				float mainHandDamage = mainHandAttack->getExactCurrent();
				// Java: offHandAttack.getExactCurrent() - a Player always has an off hand stat, so this never dereferences null
				float offHandDamage = offHandAttack->getExactCurrent();
				if (calculationTypes.contains(CalculationType::SKILL)) { // 80% of damage is added on retail
					if (offWeaponStats != nullptr) {
						float totalBaseDamage = (offHandAttack->getExactBaseWithoutBaseRate() * p.getGameStats()->getSkillEfficiency()
													+ mainHandAttack->getExactBaseWithoutBaseRate())
							* 0.8f;
						mainHandDamage = (mainHandAttack->getExactCurrentWithoutFixedBonus() + totalBaseDamage * offHandAttack->getFixedBonusRate()) * 0.8f;
						offHandDamage = (offHandAttack->getExactCurrentWithoutFixedBonus() + totalBaseDamage * mainHandAttack->getFixedBonusRate()) * 0.8f
							* p.getGameStats()->getSkillEfficiency();
					}
				} else {
					if (commons::utils::Rnd::nextInt(1000) >= p.getGameStats()->getMaxDamageChance()) {
						offHandDamage *= p.getGameStats()->getMinDamageRatio();
						if (offHandDamage <= 0 && offWeaponStats != nullptr) {
							offHandDamage = 1;
						}
					}
				}
				attackResultList.push_back(AttackResult::create(mainHandDamage, status, hitType));
				if (offWeaponStats != nullptr)
					attackResultList.push_back(AttackResult::create(offHandDamage, getOffHandStats(status), hitType));
			}
		} else { // Attack without weapon
			// "no weapon" damage has a power of 70, whereas weapons have their own power
			// TODO: parse values, but for now we can ignore it since most player weapons have a power of 100
			float damage = static_cast<float>(commons::utils::Rnd::get(16, 20))
					* (1 + ((p.getGameStats()->getPower()->getCurrent() - 100) / 100.0f * 70.0f) / 100.0f)
				+ static_cast<float>(mainHandAttack->getBonus());
			attackResultList.push_back(AttackResult::create(damage, status, hitType));
		}
	} else {
		int32_t val = runtime::as<model::gameobjects::Homing>(attacker) ? 100 : commons::utils::Rnd::get(80, 120);
		// Java: mainHandAttack.getCurrent() * val / 100f - the int product wraps, then the float division
		attackResultList.push_back(
			AttackResult::create(static_cast<float>(mulInt(mainHandAttack->getCurrent(), val)) / 100.0f, status, hitType));
	}
	return attackResultList;
}

float StatFunctions::reduceDamageByElementalDefense(model::gameobjects::Creature& effector, model::gameobjects::Creature& attacked,
	model::SkillElement element, float damage) {
	float elementalDenominator = static_cast<float>(getElementalDefenseDenominator(effector, attacked));
	int32_t rawDefense = attacked.getGameStats()->getElementalDefenseFor(element);
	int32_t adjustedDefense =
		model::templates::detail::floatToInt(adjustStatByMovementModifier(attacked, getStatForElement(element), static_cast<float>(rawDefense)));
	// Java: StatCapUtil.clampStatValue(element.getStatForElement(), ...) - a NullPointerException for SkillElement.NONE, whose stat is null
	adjustedDefense = model::stats::calc::StatCapUtil::clampStatValue(
		unbox(getStatForElement(element), "statForElement"), attacked, adjustedDefense);
	return damage * (1.0f - static_cast<float>(adjustedDefense) / elementalDenominator);
}

int32_t StatFunctions::getElementalDefenseDenominator(model::gameobjects::Creature& effector, model::gameobjects::Creature& attacked) {
	if (runtime::as<model::gameobjects::player::Player>(attacked)) {
		int32_t level = std::min<int32_t>(effector.getLevel(), attacked.getLevel());
		return addInt(model::stats::calc::StatCapUtil::getElementalDefenseBaseValue(), mulInt(std::max(0, level - 50), 10));
	}
	return model::stats::calc::StatCapUtil::getElementalDefenseBaseValue();
}

float StatFunctions::calculateMagicalSkillDamage(model::gameobjects::Creature& effector, model::gameobjects::Creature& target, float baseDamage,
	int32_t bonus, const skillengine::effect::EffectTemplate* template_, bool useMagicBoost, bool useKnowledge, bool useBoostSpellAttack) {
	using model::templates::detail::floatToInt;

	float damage = baseDamage;
	Ptr<model::stats::container::CreatureGameStats> sgs = effector.getGameStats();
	Ptr<model::stats::container::CreatureGameStats> tgs = target.getGameStats();
	int32_t magicBoost = 0;

	if (useMagicBoost) {
		magicBoost = sgs->getMBoost()->getCurrent();
		magicBoost = subInt(magicBoost, tgs->getMBResist()->getCurrent());
		// Java: (int) Math.max(0, limit(StatEnum.BOOST_MAGICAL_SKILL, magicBoost)) - the int is widened for limit, Math.max(float, float) keeps
		// the float, and the (int) cast saturates. Neither operand can be NaN or -0.0f here, so std::max answers what Math.max answers.
		magicBoost = floatToInt(std::max(0.0f, limit(StatEnum::BOOST_MAGICAL_SKILL, static_cast<float>(magicBoost))));
	}
	int32_t knowledge = useKnowledge ? sgs->getKnowledge()->getCurrent() : 100;
	damage *= (static_cast<float>(magicBoost) / 1000.0f) + (static_cast<float>(knowledge) / 100.0f);

	// Java: getStat(StatEnum.BOOST_SPELL_ATTACK, (int) damage).getCurrent() - the damage is truncated to an int base (widened back to float for
	// getStat(StatEnum, float)), and the int the stat answers replaces the damage
	if (useBoostSpellAttack)
		damage = static_cast<float>(sgs->getStat(StatEnum::BOOST_SPELL_ATTACK, static_cast<float>(floatToInt(damage)))->getCurrent());

	// add bonus damage
	damage += static_cast<float>(bonus);
	// Java: template.getElement() is the first dereference of the template, a NullPointerException for null
	const skillengine::effect::EffectTemplate& effectTemplate = nonNull(template_, "template");
	if (effectTemplate.getElement() != model::SkillElement::NONE
		&& !dynamic_cast<const skillengine::effect::NoReduceSpellATKInstantEffect*>(template_)) {
		damage = reduceDamageByElementalDefense(effector, target, effectTemplate.getElement(), damage);
		// damage is reduced by 100 per 1000 mdef
		damage -=
			adjustStatByMovementModifier(target, StatEnum::MAGICAL_DEFEND, static_cast<float>(target.getGameStats()->getMDef()->getCurrent())) / 10.0f;
	}

	if (damage < 0) {
		damage = 0;
	} else if (runtime::as<model::gameobjects::Npc>(effector) && !runtime::as<model::gameobjects::SummonedObject>(effector)) {
		// Java: (int) (damage * 0.08f) - the saturating cast; damage is not negative here (or NaN, which casts to 0), so -rnd cannot overflow
		int32_t rnd = floatToInt(damage * 0.08f);
		damage += static_cast<float>(commons::utils::Rnd::get(-rnd, rnd));
	}

	return damage;
}

bool StatFunctions::calculateMagicalCriticalRate(model::gameobjects::Creature& attacker, model::gameobjects::Creature& attacked, int32_t criticalProb) {
	if (runtime::as<model::gameobjects::Servant>(attacker) || runtime::as<model::gameobjects::Homing>(attacker))
		return false;

	// Java: `float critical = getMCritical().getCurrent() - getMCR().getCurrent()` - both accessors return int, so the subtraction is int
	// arithmetic (it wraps) and only the result is widened to float
	float critical = static_cast<float>(subInt(attacker.getGameStats()->getMCritical()->getCurrent(), attacked.getGameStats()->getMCR()->getCurrent()));
	// add critical Prob
	if (criticalProb != 100) {
		// note the difference from checkIsPhysicalCriticalHit, which multiplies only `criticalRate > 0`: the magical body raises a rate of 0 or
		// below to 1 first, so a skill's criticalProb can never scale it away to nothing (StatFunctions.java:426-429)
		if (critical <= 0)
			critical = 1;
		critical *= static_cast<float>(criticalProb) / 100.0f;
	}

	return static_cast<float>(commons::utils::Rnd::nextInt(1000)) < limit(StatEnum::MAGICAL_CRITICAL, critical);
}

int32_t StatFunctions::calculateRatingMultiplier(model::templates::npc::NpcRating npcRating) {
	// FIXME: to correct formula, have any reference?
	switch (npcRating) {
		case NpcRating::JUNK:
		case NpcRating::NORMAL:
			return 2;
		case NpcRating::ELITE:
			return 3;
		case NpcRating::HERO:
			return 4;
		case NpcRating::LEGENDARY:
			return 5;
		default:
			return 1;
	}
}

int32_t StatFunctions::getApNpcRating(model::templates::npc::NpcRating npcRating) {
	switch (npcRating) {
		case NpcRating::JUNK:
			return 1;
		case NpcRating::NORMAL:
			return 2;
		case NpcRating::ELITE:
			return 4;
		case NpcRating::HERO:
			return 35; // need check
		case NpcRating::LEGENDARY:
			return 2500; // need check
		default:
			return 1;
	}
}

float StatFunctions::adjustDamageByPvpOrPveModifiers(model::gameobjects::Creature& attacker, model::gameobjects::Creature& target, float baseDamage,
	int32_t pvpDamage, bool useTemplateDmg, model::SkillElement element) {
	using model::stats::container::CombatMode;
	using model::stats::container::RatioType;

	int32_t attackBonus = 0;
	int32_t defenseBonus = 0;
	float damage = baseDamage;
	bool pvpTarget = attacker.isPvpTarget(target);
	if (pvpTarget) {
		if (pvpDamage > 0)
			damage *= static_cast<float>(pvpDamage) * 0.01f;
		damage *= 0.42f; // PVP modifier 42%, last checked on NA (4.9) 19.03.2016
		if (!useTemplateDmg) {
			attackBonus = attacker.getGameStats()->getStat(StatEnum::PVP_ATTACK_RATIO, 0)->getCurrent();
			defenseBonus = target.getGameStats()->getStat(StatEnum::PVP_DEFEND_RATIO, 0)->getCurrent();
			if (attacker.getRace() != target.getRace() && !attacker.isInInstance())
				attackBonus = addInt(attackBonus, model::siege::Influence::getInstance().getPvpRaceBonusRatio(attacker.getRace()));
			switch (element) {
				case model::SkillElement::NONE:
					attackBonus = addInt(attackBonus, attacker.getGameStats()->getStat(StatEnum::PVP_ATTACK_RATIO_PHYSICAL, 0)->getCurrent());
					defenseBonus = addInt(defenseBonus, target.getGameStats()->getStat(StatEnum::PVP_DEFEND_RATIO_PHYSICAL, 0)->getCurrent());
					break;
				default:
					attackBonus = addInt(attackBonus, attacker.getGameStats()->getStat(StatEnum::PVP_ATTACK_RATIO_MAGICAL, 0)->getCurrent());
					defenseBonus = addInt(defenseBonus, target.getGameStats()->getStat(StatEnum::PVP_DEFEND_RATIO_MAGICAL, 0)->getCurrent());
			}
		}
	} else if (!useTemplateDmg) {
		if (runtime::as<model::gameobjects::player::Player>(attacker)) { // npcs dmg is not reduced because of the level difference GF (4.9) 23.04.2016
			damage *= 1.0f - getNpcLevelDiffMod(target, attacker);
		}
		attackBonus = attacker.getGameStats()->getStat(StatEnum::PVE_ATTACK_RATIO, 0)->getCurrent();
		defenseBonus = target.getGameStats()->getStat(StatEnum::PVE_DEFEND_RATIO, 0)->getCurrent();
		switch (element) {
			case model::SkillElement::NONE:
				attackBonus = addInt(attackBonus, attacker.getGameStats()->getStat(StatEnum::PVE_ATTACK_RATIO_PHYSICAL, 0)->getCurrent());
				defenseBonus = addInt(defenseBonus, target.getGameStats()->getStat(StatEnum::PVE_DEFEND_RATIO_PHYSICAL, 0)->getCurrent());
				break;
			default:
				attackBonus = addInt(attackBonus, attacker.getGameStats()->getStat(StatEnum::PVE_ATTACK_RATIO_MAGICAL, 0)->getCurrent());
				defenseBonus = addInt(defenseBonus, target.getGameStats()->getStat(StatEnum::PVE_DEFEND_RATIO_MAGICAL, 0)->getCurrent());
		}
	}
	CombatMode mode = pvpTarget ? CombatMode::PVP : CombatMode::PVE;
	// PvP/PvE ratio caps are applied after aggregation (retail behavior)
	attackBonus = model::stats::calc::StatCapUtil::limitValueForPvpOrPveStat(mode, RatioType::ATTACK, attackBonus);
	defenseBonus = model::stats::calc::StatCapUtil::limitValueForPvpOrPveStat(mode, RatioType::DEFENSE, defenseBonus);
	float multiplier = 1.0f + static_cast<float>(subInt(attackBonus, defenseBonus)) / 1000.0f;
	// Retail behavior: damage multiplier has a minimum cap of 10% (0.1f)
	multiplier = std::max(multiplier, 0.1f);

	return damage * multiplier;
}

bool StatFunctions::checkIsDodgedHit(model::gameobjects::Creature& attacker, model::gameobjects::Creature& attacked, int32_t accMod) {
	using controllers::attack::AttackStatus;
	// check if attacker is blinded
	if (attacker.getObserveController()->checkAttackerStatus(AttackStatus::DODGE))
		return true;
	// check always dodge
	if (attacked.getObserveController()->checkAttackStatus(AttackStatus::DODGE))
		return true;

	float accuracy = static_cast<float>(attacker.getGameStats()->getMainHandPAccuracy()->getCurrent()) + static_cast<float>(accMod);
	float dodge = static_cast<float>(attacked.getGameStats()->getEvasion()->getBonus())
		+ adjustStatByMovementModifier(attacked, StatEnum::EVASION, static_cast<float>(attacked.getGameStats()->getEvasion()->getBase()));
	float dodgeRate = dodge - accuracy;
	if (Ptr<model::gameobjects::Npc> npc = runtime::as<model::gameobjects::Npc>(attacked)) {
		// static npcs never dodge
		if (npc->hasStatic())
			return false;
		dodgeRate *= 1 + getNpcLevelDiffMod(attacked, attacker);
	}
	return commons::utils::Rnd::nextInt(1000) < limit(StatEnum::EVASION, dodgeRate);
}

bool StatFunctions::checkIsParriedHit(model::gameobjects::Creature& attacker, model::gameobjects::Creature& attacked, int32_t accMod) {
	using controllers::attack::AttackStatus;
	// check always parry
	if (attacked.getObserveController()->checkAttackStatus(AttackStatus::PARRY))
		return true;

	float accuracy = static_cast<float>(attacker.getGameStats()->getMainHandPAccuracy()->getCurrent()) + static_cast<float>(accMod);
	float parry = static_cast<float>(attacked.getGameStats()->getParry()->getBonus())
		+ adjustStatByMovementModifier(attacked, StatEnum::PARRY, static_cast<float>(attacked.getGameStats()->getParry()->getBase()));
	return commons::utils::Rnd::nextInt(1000) < limit(StatEnum::PARRY, parry - accuracy);
}

bool StatFunctions::checkIsBlockedHit(model::gameobjects::Creature& attacker, model::gameobjects::Creature& attacked, int32_t accMod) {
	using controllers::attack::AttackStatus;
	// check always block
	if (attacked.getObserveController()->checkAttackStatus(AttackStatus::BLOCK))
		return true;

	float accuracy = static_cast<float>(attacker.getGameStats()->getMainHandPAccuracy()->getCurrent()) + static_cast<float>(accMod);

	float block = static_cast<float>(attacked.getGameStats()->getBlock()->getBonus())
		+ adjustStatByMovementModifier(attacked, StatEnum::BLOCK, static_cast<float>(attacked.getGameStats()->getBlock()->getBase()));
	return commons::utils::Rnd::nextInt(1000) < limit(StatEnum::BLOCK, block - accuracy);
}

bool StatFunctions::checkIsPhysicalCriticalHit(model::gameobjects::Creature& attacker, model::gameobjects::Creature& attacked, bool isMainHand,
	int32_t criticalProb, bool isSkill) {
	using controllers::attack::AttackStatus;
	if (runtime::as<model::gameobjects::Servant>(attacker) || runtime::as<model::gameobjects::Homing>(attacker))
		return false;
	float criticalRate;
	Ptr<model::gameobjects::player::Player> attackingPlayer = runtime::as<model::gameobjects::player::Player>(attacker);
	if (attackingPlayer && !isMainHand)
		criticalRate = static_cast<float>(attackingPlayer->getGameStats()->getOffHandPCritical()->getCurrent());
	else
		criticalRate = static_cast<float>(attacker.getGameStats()->getMainHandPCritical()->getCurrent());

	// check one time boost skill critical
	Ref<controllers::observer::AttackerCriticalStatus> acStatus =
		attacker.getObserveController()->checkAttackerCriticalStatus(AttackStatus::CRITICAL, isSkill);
	if (acStatus->isResult()) {
		if (acStatus->isPercent())
			// Java: criticalRate *= (1 + acStatus.getValue() / 100) - `getValue() / 100` is an int division, so a value below 100 adds nothing
			criticalRate *= static_cast<float>(addInt(1, acStatus->getValue() / 100));
		else
			return commons::utils::Rnd::nextInt(1000) < acStatus->getValue();
	}

	criticalRate -= static_cast<float>(attacked.getGameStats()->getPCR()->getCurrent());

	// add critical Prob
	if (criticalProb != 100 && criticalRate > 0) {
		criticalRate *= static_cast<float>(criticalProb) / 100.0f;
	}
	return commons::utils::Rnd::nextInt(1000) < limit(StatEnum::PHYSICAL_CRITICAL, criticalRate);
}

int32_t StatFunctions::calculateMagicalResistRate(model::gameobjects::Creature& attacker, model::gameobjects::Creature& attacked, int32_t accMod,
	model::SkillElement element) {
	using controllers::attack::AttackStatus;
	if (attacked.getObserveController()->checkAttackStatus(AttackStatus::RESIST))
		return 1000;
	if (element != model::SkillElement::NONE) {
		if (Ptr<model::gameobjects::Summon> summon = runtime::as<model::gameobjects::Summon>(attacked);
			summon && element == summon->getAlwaysResistElement())
			return 1000;
	}

	int32_t levelDiff = subInt(attacked.getLevel(), attacker.getLevel());
	int32_t mResi = attacked.getGameStats()->getMResist()->getCurrent();
	int32_t resistRate = subInt(subInt(mResi, attacker.getGameStats()->getMAccuracy()->getCurrent()), accMod);

	if (mResi > 0 && levelDiff > 4) // only apply if creature has mres > 0 (to keep effect of AI#modifyOwnerStat)
		resistRate = addInt(resistRate, mulInt(subInt(levelDiff, 4), 100));

	// checked on retail: only applies to PvP
	if (runtime::as<model::gameobjects::player::Player>(attacker) && runtime::as<model::gameobjects::player::Player>(attacked))
		return std::min(500, resistRate);

	// Java: `(int) limit(StatEnum.MAGICAL_RESIST, resistRate)` - limit returns a float, so this is the saturating (int) cast of a float
	return model::templates::detail::floatToInt(limit(StatEnum::MAGICAL_RESIST, static_cast<float>(resistRate)));
}

int32_t StatFunctions::calculateFallDamage(model::gameobjects::player::Player& player, float distance) {
	int32_t fallDamage = 0;
	if (distance >= configs::main::FallDamageConfig::MAXIMUM_DISTANCE_DAMAGE.load()) {
		fallDamage = player.getLifeStats()->getCurrentHp();
	} else if (distance >= configs::main::FallDamageConfig::MINIMUM_DISTANCE_DAMAGE.load()) {
		float dmgPerMeter = player.getLifeStats()->getMaxHp() * configs::main::FallDamageConfig::FALL_DAMAGE_PERCENTAGE.load() / 100.0f;
		fallDamage = model::templates::detail::floatToInt(distance * dmgPerMeter);
	}
	return fallDamage;
}

float StatFunctions::adjustStatByMovementModifier(model::gameobjects::Creature& creature, std::optional<StatEnum> stat, float value) {
	runtime::Ptr<model::gameobjects::player::Player> player = runtime::as<model::gameobjects::player::Player>(creature);
	if (!player || !stat)
		return value;

	using Direction = controllers::movement::PlayableMoveController::MovementModifierDirection;
	// https://web.archive.org/web/20170429204823/gameguide.na.aiononline.com/aion/Combat
	switch (player->getMoveController()->getMovementDirection()) {
		case Direction::FORWARD:
			switch (*stat) {
				case StatEnum::PHYSICAL_ATTACK:
				case StatEnum::MAGICAL_ATTACK:
					return value * 1.1f; // verified on 4.6 PTS
				case StatEnum::FIRE_RESISTANCE:
				case StatEnum::EARTH_RESISTANCE:
				case StatEnum::WATER_RESISTANCE:
				case StatEnum::WIND_RESISTANCE:
				case StatEnum::LIGHT_RESISTANCE:
				case StatEnum::DARK_RESISTANCE:
					return value - 50; // verified on 4.6 PTS
				case StatEnum::MAGICAL_DEFEND:
				case StatEnum::PHYSICAL_DEFENSE:
					return value * 0.8f; // verified on 4.6 PTS
				default:
					break;
			}
			break;
		case Direction::SIDEWAYS:
			switch (*stat) {
				case StatEnum::PHYSICAL_ATTACK:
				case StatEnum::MAGICAL_ATTACK:
				case StatEnum::SPEED:
					return value * 0.8f; // verified on 4.6 PTS
				case StatEnum::EVASION:
					return value + 300;
				default:
					break;
			}
			break;
		case Direction::BACKWARD:
			switch (*stat) {
				case StatEnum::PHYSICAL_ATTACK:
				case StatEnum::MAGICAL_ATTACK:
					return value * 0.8f; // verified on 4.6 PTS
				case StatEnum::SPEED:
					return value * 0.6f; // verified on 4.6 PTS
				case StatEnum::PARRY:
				case StatEnum::BLOCK:
					return value + 500;
				default:
					break;
			}
			break;
		default:
			break;
	}
	return value;
}

float StatFunctions::getNpcLevelDiffMod(model::gameobjects::Creature& target, model::gameobjects::Creature& attacker) {
	int32_t levelDiff = target.getLevel() - attacker.getLevel();
	if (levelDiff <= 2)
		return 0.0f;
	if (levelDiff >= 12)
		return 1.0f;
	return (levelDiff - 2) * 0.1f;
}

float StatFunctions::limit(StatEnum statEnum, float value) {
	// Java: Math.min(int differenceLimit, float value) - the limit is an int, so only a NaN or a -0.0f value needs Java's special cases
	float differenceLimit = static_cast<float>(model::stats::calc::StatCapUtil::getDifferenceLimit(statEnum));
	if (differenceLimit == 0.0f && value == 0.0f && std::signbit(value))
		return value;
	return differenceLimit <= value ? differenceLimit : value;
}

} // namespace aion::gameserver::utils::stats
