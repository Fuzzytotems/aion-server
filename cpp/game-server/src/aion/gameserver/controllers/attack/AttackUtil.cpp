#include "aion/gameserver/controllers/attack/AttackUtil.h"

#include <algorithm>
#include <limits>
#include <string>

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/commons/utils/Rnd.h"
#include "aion/gameserver/ai/AbstractAI.h"
#include "aion/gameserver/controllers/CreatureController.h"
#include "aion/gameserver/controllers/ObserveController.h"
#include "aion/gameserver/controllers/attack/AttackResult.h"
#include "aion/gameserver/controllers/attack/AttackStatusInfo.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/NpcData.h"
#include "aion/gameserver/model/SkillElement.h"
#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/model/gameobjects/Item.h"
#include "aion/gameserver/model/gameobjects/Npc.h"
#include "aion/gameserver/model/gameobjects/player/Equipment.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/items/ItemSlot.h"
#include "aion/gameserver/model/items/NpcEquippedGear.h"
#include "aion/gameserver/model/stats/calc/Stat2.h"
#include "aion/gameserver/model/stats/container/CreatureGameStats.h"
#include "aion/gameserver/model/stats/container/StatEnum.h"
#include "aion/gameserver/model/templates/detail/JavaCasts.h"
#include "aion/gameserver/model/templates/item/ItemTemplate.h"
#include "aion/gameserver/model/templates/item/WeaponStats.h"
#include "aion/gameserver/model/templates/item/enums/ItemSubType.h"
#include "aion/gameserver/model/templates/npc/NpcTemplate.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/skillengine/model/Skill.h"
#include "aion/gameserver/utils/stats/StatFunctions.h"
#include "aion/gameserver/world/knownlist/KnownList.h"

namespace aion::gameserver::controllers::attack {

using model::gameobjects::Creature;
using model::gameobjects::player::Player;
using model::templates::item::enums::ItemGroup;
using runtime::Ptr;
using runtime::Ref;

namespace {

// logger name = the Java class name, so log output and per-logger configuration match the Java server
const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.controllers.attack.AttackUtil");

/** Java's implicit null check of a dereference: the object, NullPointerException for null */
template <class T>
const T& nonNull(const T* value, const char* what) {
	if (value == nullptr)
		throw runtime::NullPointerException(std::string(what) + " is null");
	return *value;
}

/** Java unboxing of a nullable enum (`switch (boxed)`) */
template <class T>
T unbox(const std::optional<T>& value, const char* what) {
	if (!value)
		throw runtime::NullPointerException(std::string(what) + " is null");
	return *value;
}

/** Java int a + b (wraps on overflow) */
constexpr int32_t addInt(int32_t a, int32_t b) noexcept {
	return static_cast<int32_t>(static_cast<uint32_t>(a) + static_cast<uint32_t>(b));
}

/** Java `(int) doubleValue`: NaN becomes 0, out-of-range values saturate */
constexpr int32_t doubleToInt(double value) noexcept {
	if (value != value)
		return 0;
	if (value >= 2147483648.0)
		return std::numeric_limits<int32_t>::max();
	if (value <= -2147483648.0)
		return std::numeric_limits<int32_t>::min();
	return static_cast<int32_t>(value);
}

/** Java List.get(index): IndexOutOfBoundsException (JDK wording) outside [0, size) */
AttackResult& listGet(const std::vector<Ref<AttackResult>>& list, size_t index) {
	if (index >= list.size())
		throw runtime::IndexOutOfBoundsException(
			"Index " + std::to_string(index) + " out of bounds for length " + std::to_string(list.size()));
	return *list[index];
}

/** The result list as the borrowed view ObserveController::checkShieldStatus takes (CreatureController::attackTarget does the same) */
std::vector<Ptr<AttackResult>> toPtrList(const std::vector<Ref<AttackResult>>& list) {
	return std::vector<Ptr<AttackResult>>(list.begin(), list.end());
}

} // namespace

std::vector<runtime::Ref<AttackResult>> AttackUtil::calculatePhysAttackResult(Creature& attacker, Creature& attacked,
	const std::unordered_set<utils::stats::CalculationType>& calculationTypes) {
	AttackStatus attackStatus = calculatePhysicalStatus(attacker, attacked, true, 0, 100, false, false);
	std::vector<Ref<AttackResult>> attackResultList =
		utils::stats::StatFunctions::calculateAttackDamage(attacker, model::SkillElement::NONE, attackStatus, calculationTypes);
	// header-request: m5b-1 (the three helpers take the Ref list this one owns; amplifyDamageByAdditionalHitCount appends to it)
	adjustDamageByStatModifiers(attacker, attacked, attackStatus, attackResultList, model::SkillElement::NONE);
	amplifyDamageByAdditionalHitCount(attacker, attackStatus, attackResultList);
	modifyDamageByNpcAi(attacker, attacked, attackResultList);
	attacked.getObserveController()->checkShieldStatus(toPtrList(attackResultList), nullptr, attacker);
	return attackResultList;
}

void AttackUtil::adjustDamageByStatModifiers(Creature& attacker, Creature& attacked, AttackStatus status,
	const std::vector<Ref<AttackResult>>& attackResultList, model::SkillElement element) {
	using model::stats::container::StatEnum;

	float mainMultiplier = 1;
	float offMultiplier = 1;
	int32_t reduceMax = std::numeric_limits<int32_t>::max();
	float reduceRatio = 0;
	switch (getBaseStatus(status)) {
		case AttackStatus::DODGE:
		case AttackStatus::RESIST:
			return;
		case AttackStatus::BLOCK:
			if (Ptr<Player> p = runtime::as<Player>(attacked)) {
				Ptr<model::gameobjects::Item> shield = p->getEquipment().getEquippedShield();
				if (shield) {
					reduceMax = nonNull(nonNull(shield->getItemTemplate(), "itemTemplate").getWeaponStats(), "weaponStats").getReduceMax();
					reduceRatio = static_cast<float>(attacked.getGameStats()->getReverseStat(StatEnum::DAMAGE_REDUCE, 100)->getCurrent()) / 100.0f;
				}
			} else {
				reduceRatio = 10; // NPCs reduce damage by min. 10%. TODO: Implement blocking for npcs without shield + check ratio for different npcs
			}
			break;
		case AttackStatus::PARRY:
			mainMultiplier *= 0.6f;
			offMultiplier *= 0.6f;
			break;
		default:
			break;
	}

	if (isCritical(status)) {
		mainMultiplier = 1.5f;
		if (element == model::SkillElement::NONE) {
			std::optional<ItemGroup> mainHandGroup = getWeaponGroup(attacker, true);
			if (mainHandGroup) {
				mainMultiplier = getWeaponMultiplier(mainHandGroup);
				std::optional<ItemGroup> offHandGroup = getWeaponGroup(attacker, false);
				if (offHandGroup) {
					offMultiplier = getWeaponMultiplier(offHandGroup);
				}
			}
		}
		if (runtime::as<Player>(attacked)) {
			int32_t fortitude;
			if (element == model::SkillElement::NONE) { // if stat != null ? why
				fortitude = attacked.getGameStats()->getStat(StatEnum::PHYSICAL_CRITICAL_DAMAGE_REDUCE, 0)->getCurrent();
			} else {
				fortitude = attacked.getGameStats()->getStat(StatEnum::MAGICAL_CRITICAL_DAMAGE_REDUCE, 0)->getCurrent();
			}
			mainMultiplier = (mainMultiplier - static_cast<float>(fortitude) / 1000.0f);
			offMultiplier = (offMultiplier + static_cast<float>(fortitude) / 1000.0f);
		}
	}

	size_t maxListIndex = std::min<size_t>(attackResultList.size(), 2);
	if (maxListIndex < attackResultList.size()) // should never happen but log just in case
		log.warn("attackResultList has more elements than expected (" + std::to_string(attackResultList.size()) + ")");
	for (size_t i = 0; i < maxListIndex; i++) {
		float damageMultiplier = i == 0 ? mainMultiplier : offMultiplier;
		bool isPhysical = element == model::SkillElement::NONE;
		StatEnum attackStat = isPhysical ? StatEnum::PHYSICAL_ATTACK : StatEnum::MAGICAL_ATTACK;
		StatEnum defenseStat = isPhysical ? StatEnum::PHYSICAL_DEFENSE : StatEnum::MAGICAL_DEFEND;
		float defenseStatValue = isPhysical ? static_cast<float>(attacked.getGameStats()->getPDef()->getCurrent())
											: static_cast<float>(attacked.getGameStats()->getMDef()->getCurrent());
		float defense = utils::stats::StatFunctions::adjustStatByMovementModifier(attacked, defenseStat, defenseStatValue);
		float damage = static_cast<float>(attackResultList[i]->getDamage()) - (defense / 10);
		damage *= damageMultiplier;
		damage = utils::stats::StatFunctions::adjustStatByMovementModifier(attacker, attackStat, damage);
		if (reduceRatio > 0) {
			float dmgToReduce = damage - (damage * reduceRatio);
			if (dmgToReduce > static_cast<float>(reduceMax)) {
				dmgToReduce = static_cast<float>(reduceMax);
			}
			damage -= dmgToReduce;
		}
		damage = utils::stats::StatFunctions::adjustDamageByPvpOrPveModifiers(attacker, attacked, damage, 0, false, element);
		if (damage < 1) {
			damage = 1;
		}
		attackResultList[i]->setDamage(damage);
	}
}

std::vector<int32_t> AttackUtil::calculateAdditionalHitCount(Creature& attacker, AttackStatus status,
	const std::vector<Ref<AttackResult>>& attackList) {
	std::vector<int32_t> hitCount(2, 0);
	Ptr<Player> p = runtime::as<Player>(attacker);
	if (p && (status != AttackStatus::DODGE && status != AttackStatus::RESIST)) {
		Ptr<model::gameobjects::Item> mainHandWeapon = p->getEquipment().getMainHandWeapon();
		if (mainHandWeapon) {
			hitCount[0] = commons::utils::Rnd::get(
							  0, nonNull(nonNull(mainHandWeapon->getItemTemplate(), "itemTemplate").getWeaponStats(), "weaponStats").getHitCount())
				- 1;
			if (attackList.size() > 1) {
				Ptr<model::gameobjects::Item> offHandWeapon = p->getEquipment().getOffHandWeapon();
				if (offHandWeapon
					&& nonNull(offHandWeapon->getItemTemplate(), "itemTemplate").getItemSubType() != model::templates::item::enums::ItemSubType::SHIELD) {
					hitCount[1] = commons::utils::Rnd::get(
						0, nonNull(nonNull(offHandWeapon->getItemTemplate(), "itemTemplate").getWeaponStats(), "weaponStats").getHitCount() - 1);
				}
			}
		}
	}
	return hitCount;
}

void AttackUtil::amplifyDamageByAdditionalHitCount(Creature& attacker, AttackStatus status, std::vector<Ref<AttackResult>>& attackList) {
	std::vector<int32_t> hitCount = calculateAdditionalHitCount(attacker, status, attackList);
	// Java: `for (int i = 0; i < hitCount[0] + hitCount[1]; i++)` - the sum wraps like every int addition, and Rnd.get(0, n) - 1 can be -1
	for (int32_t i = 0; i < addInt(hitCount[0], hitCount[1]); i++) {
		if (i < hitCount[0]) { // amplify main hand damage
			// Java: `(int) (attackList.get(0).getDamage() * 0.1)` - 0.1 is a double literal, so the product is a double and the cast saturates
			AttackResult& main = listGet(attackList, 0);
			if (main.getDamage() >= 10)
				attackList.push_back(AttackResult::create(static_cast<float>(doubleToInt(static_cast<double>(listGet(attackList, 0).getDamage()) * 0.1)),
					AttackStatus::NORMALHIT, listGet(attackList, 0).getHitType()));
		} else { // amplify off hand damage
			AttackResult& off = listGet(attackList, 1);
			if (off.getDamage() >= 10)
				attackList.push_back(AttackResult::create(static_cast<float>(doubleToInt(static_cast<double>(listGet(attackList, 1).getDamage()) * 0.1)),
					AttackStatus::OFFHAND_NORMALHIT, listGet(attackList, 1).getHitType()));
		}
	}
}

void AttackUtil::modifyDamageByNpcAi(Creature& attacker, Creature& attacked, const std::vector<Ref<AttackResult>>& attackStatus) {
	if (!(runtime::as<model::gameobjects::Npc>(attacker) || runtime::as<model::gameobjects::Npc>(attacked)))
		return;
	for (const Ref<AttackResult>& status : attackStatus) {
		float modifiedDamage = static_cast<float>(status->getDamage());
		if (runtime::as<model::gameobjects::Npc>(attacker))
			modifiedDamage = attacker.getAi().modifyOwnerDamage(modifiedDamage, attacked, nullptr);
		if (runtime::as<model::gameobjects::Npc>(attacked))
			modifiedDamage = attacked.getAi().modifyDamage(attacker, modifiedDamage, nullptr);
		status->setDamage(modifiedDamage);
	}
}

float AttackUtil::calculateBlockedDamage(Creature& attacked, float damage) {
	using model::stats::container::StatEnum;

	int32_t reduceStat = attacked.getGameStats()->getReverseStat(StatEnum::DAMAGE_REDUCE, 100)->getCurrent();
	float reduceVal = damage - (damage * static_cast<float>(reduceStat) / 100);
	if (Ptr<Player> attackedPlayer = runtime::as<Player>(attacked)) {
		Ptr<model::gameobjects::Item> shield = attackedPlayer->getEquipment().getEquippedShield();
		if (shield) {
			int32_t reduceMax = nonNull(nonNull(shield->getItemTemplate(), "itemTemplate").getWeaponStats(), "weaponStats").getReduceMax();
			if (reduceMax > 0 && static_cast<float>(reduceMax) < reduceVal)
				reduceVal = static_cast<float>(reduceMax);
		}
	}
	return damage - reduceVal;
}

float AttackUtil::calculateWeaponCritical(model::SkillElement element, Creature& attacked, float damage, std::optional<ItemGroup> group,
	int32_t critAddDmg, model::stats::container::StatEnum stat, bool isMain) {
	using model::stats::container::StatEnum;

	float coeficient = 1.5f;
	if (element == model::SkillElement::NONE && group) {
		coeficient = getWeaponMultiplier(group);
	}

	// Java: `if (stat != null && attacked instanceof Player)` - the C++ parameter is not nullable, because both call sites pass a constant stat
	if (runtime::as<Player>(attacked)) { // Strike Fortitude lowers the crit multiplier
		switch (stat) {
			case StatEnum::PHYSICAL_CRITICAL_DAMAGE_REDUCE:
			case StatEnum::MAGICAL_CRITICAL_DAMAGE_REDUCE: {
				int32_t fortitude = attacked.getGameStats()->getStat(stat, 0)->getCurrent();
				coeficient = isMain ? (coeficient - static_cast<float>(fortitude) / 1000.0f) : (coeficient + static_cast<float>(fortitude) / 1000.0f);
				break;
			}
			default:
				break;
		}
	}

	// add critical add dmg
	coeficient += static_cast<float>(critAddDmg) / 100.0f;
	return damage * coeficient;
}

float AttackUtil::getWeaponMultiplier(std::optional<ItemGroup> group) {
	// Java: switch (group) on a null ItemGroup throws a NullPointerException; no call site passes null
	switch (unbox(group, "group")) {
		case ItemGroup::DAGGER:
			return 2.3f;
		case ItemGroup::SWORD:
			return 2.2f;
		case ItemGroup::MACE:
			return 2.0f;
		case ItemGroup::GREATSWORD:
		case ItemGroup::POLEARM:
			return 1.8f;
		case ItemGroup::STAFF:
		case ItemGroup::BOW:
			return 1.7f;
		default:
			return 1.5f;
	}
}

void AttackUtil::calculateSkillResult(skillengine::model::Effect& effect, int32_t skillDamage, const skillengine::effect::DamageEffect* template_,
	bool ignoreShield) {
	AION_UNPORTED();
}

float AttackUtil::randomizeDamage(int32_t randomDamageType, float damage) {
	float multiplier;
	switch (randomDamageType) {
		case 1: {
			int32_t roll = commons::utils::Rnd::get(0, 19);
			multiplier = roll <= 6 ? 0.5f : roll <= 12 ? 1.0f : 1.5f;
			break;
		}

		case 2:
			multiplier = commons::utils::Rnd::chance() < 70.0f ? 0.6f : 2.0f;
			break;

		case 3: {
			int32_t roll = commons::utils::Rnd::get(0, 19);
			multiplier = roll <= 6 ? 0.9f : roll <= 12 ? 1.0f : 1.1f;
			break;
		}

		case 6:
			multiplier = commons::utils::Rnd::chance() < 70.0f ? 1.0f : 2.0f;
			break;

		case 4:
		case 5:
		case 7:
		case 8:
		case 9:
		case 10:
			multiplier = 1.0f;
			break;

		default:
			throw runtime::IllegalArgumentException("Unhandled random damage type rnddmg=\"" + std::to_string(randomDamageType) + "\"");
	}

	return damage * multiplier;
}

void AttackUtil::calculateEffectResult(skillengine::model::Effect& effect, Creature& effected, int32_t damage, AttackStatus status,
	skillengine::model::HitType hitType, bool ignoreShield, int32_t position, bool send) {
	AION_UNPORTED();
}

std::vector<runtime::Ref<AttackResult>> AttackUtil::calculateMagAttackResult(Creature& attacker, Creature& attacked, model::SkillElement element,
	const std::unordered_set<utils::stats::CalculationType>& calculationTypes) {
	AION_UNPORTED();
}

int32_t AttackUtil::calculateMagicalOverTimeSkillResult(skillengine::model::Effect& effect, float skillDamage,
	const skillengine::effect::EffectTemplate* template_, bool useMagicBoost) {
	AION_UNPORTED();
}

AttackStatus AttackUtil::calculatePhysicalStatus(Creature& attacker, Creature& attacked, const skillengine::effect::EffectTemplate* template_,
	skillengine::model::Effect& effect) {
	AION_UNPORTED();
}

AttackStatus AttackUtil::calculatePhysicalStatus(Creature& attacker, Creature& attacked, bool isMainHand, int32_t accMod, int32_t criticalProb, bool isSkill,
	bool cannotMiss) {
	AttackStatus status = AttackStatus::NORMALHIT;

	if (!cannotMiss) {
		Ptr<Player> attackedPlayer = runtime::as<Player>(attacked);
		if (!isSkill && utils::stats::StatFunctions::checkIsDodgedHit(attacker, attacked, accMod))
			status = AttackStatus::DODGE;
		else if (attackedPlayer && attackedPlayer->getEquipment().isShieldEquipped()
			&& utils::stats::StatFunctions::checkIsBlockedHit(attacker, attacked, accMod))
			status = AttackStatus::BLOCK;
		else if (attackedPlayer && utils::stats::StatFunctions::checkIsParriedHit(attacker, attacked, accMod))
			status = AttackStatus::PARRY;
	} else {
		// the three checks are called for their side effects (each consumes one always-dodge/block/parry effect activation)
		static_cast<void>(utils::stats::StatFunctions::checkIsDodgedHit(attacker, attacked, accMod));
		static_cast<void>(utils::stats::StatFunctions::checkIsBlockedHit(attacker, attacked, accMod));
		static_cast<void>(utils::stats::StatFunctions::checkIsParriedHit(attacker, attacked, accMod));
	}
	if (utils::stats::StatFunctions::checkIsPhysicalCriticalHit(attacker, attacked, isMainHand, criticalProb, isSkill)) {
		switch (status) {
			case AttackStatus::BLOCK:
				status = AttackStatus::CRITICAL_BLOCK;
				break;
			case AttackStatus::PARRY:
				status = AttackStatus::CRITICAL_PARRY;
				break;
			case AttackStatus::DODGE:
				status = AttackStatus::CRITICAL_DODGE;
				break;
			default:
				status = AttackStatus::CRITICAL;
				break;
		}
	}
	return isMainHand ? status : getOffHandStats(status);
}

AttackStatus AttackUtil::calculateMagicalStatus(Creature& attacker, Creature& attacked, int32_t criticalProb, bool isSkill) {
	AION_UNPORTED();
}

void AttackUtil::cancelCastOn(Creature& target) {
	target.getKnownList().forEachObject([&target](model::gameobjects::VisibleObject& visibleObject) {
		Ptr<Creature> creature = runtime::as<Creature>(visibleObject);
		if (creature && visibleObject.getTarget().get() == &target) {
			Ptr<skillengine::model::Skill> castingSkill = creature->getCastingSkill();
			if (castingSkill) {
				Ptr<Creature> firstTarget = castingSkill->getFirstTarget();
				if (!firstTarget) // Java: getFirstTarget().equals(target) on a null first target
					throw runtime::NullPointerException("casting skill has no first target");
				if (firstTarget->equals(target))
					creature->getController().cancelCurrentSkill(nullptr);
			}
		}
	});
}

void AttackUtil::removeTargetFrom(Creature& object) {
	removeTargetFrom(object, false);
}

void AttackUtil::removeTargetFrom(Creature& object, bool validateSee) {
	object.getKnownList().forEachPlayer([&object, validateSee](model::gameobjects::player::Player& player) {
		if (player.getTarget().get() == &object && (!validateSee || !player.canSee(Ptr<model::gameobjects::VisibleObject>(object))))
			player.setTarget(nullptr);
	});
}

std::optional<ItemGroup> AttackUtil::getWeaponGroup(Creature& effector, bool mainHand) {
	if (Ptr<Player> player = runtime::as<Player>(effector)) {
		Ptr<model::gameobjects::Item> weapon = mainHand ? player->getEquipment().getMainHandWeapon() : player->getEquipment().getOffHandWeapon();
		if (weapon) {
			return nonNull(weapon->getItemTemplate(), "itemTemplate").getItemGroup();
		}
	} else if (Ptr<model::gameobjects::Npc> npc = runtime::as<model::gameobjects::Npc>(effector)) {
		// Java: DataManager.NPC_DATA.getNpcTemplate(npc.getNpcId()).getEquipment() - a NullPointerException for an unknown npc id
		const model::templates::npc::NpcTemplate& temp =
			nonNull(dataholders::DataManager::NPC_DATA->getNpcTemplate(npc->getNpcId()), "npcTemplate");
		Ptr<model::items::NpcEquippedGear> npcGear = temp.getEquipment();
		model::items::ItemSlot slot = mainHand ? model::items::ItemSlot::MAIN_HAND : model::items::ItemSlot::MAIN_OFF_HAND;
		if (npcGear && npcGear->getItem(slot) != nullptr) {
			return npcGear->getItem(slot)->getItemGroup();
		}
	}
	return std::nullopt;
}

} // namespace aion::gameserver::controllers::attack
