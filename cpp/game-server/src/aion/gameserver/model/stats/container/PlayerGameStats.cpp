#include "aion/gameserver/model/stats/container/PlayerGameStats.h"

#include <limits>
#include <string_view>

#include "aion/commons/utils/Rnd.h"
#include "aion/gameserver/configs/main/CustomConfig.h"
#include "aion/gameserver/model/EmotionType.h"
#include "aion/gameserver/model/PlayerClassInfo.h"
#include "aion/gameserver/model/actions/PlayerMode.h"
#include "aion/gameserver/model/gameobjects/Item.h"
#include "aion/gameserver/model/gameobjects/player/Equipment.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/gameobjects/state/CreatureState.h"
#include "aion/gameserver/model/stats/calc/AdditionStat.h"
#include "aion/gameserver/model/stats/calc/Stat2.h"
#include "aion/gameserver/model/stats/calc/functions/IStatFunction.h"
#include "aion/gameserver/model/stats/container/StatEnum.h"
#include "aion/gameserver/model/templates/item/ItemAttackTypeInfo.h"
#include "aion/gameserver/model/templates/item/ItemTemplate.h"
#include "aion/gameserver/model/templates/item/WeaponStats.h"
#include "aion/gameserver/model/templates/item/enums/ItemSubType.h"
#include "aion/gameserver/model/templates/ride/RideInfo.h"
#include "aion/gameserver/model/templates/stats/StatsTemplate.h"
#include "aion/gameserver/network/aion/serverpackets/SM_EMOTION.h"
#include "aion/gameserver/network/aion/serverpackets/SM_STATS_INFO.h"
#include "aion/gameserver/model/templates/detail/JavaCasts.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/utils/JavaMath.h"
#include "aion/gameserver/utils/PacketSendUtility.h"
#include "aion/gameserver/utils/stats/CalculationType.h"

namespace aion::gameserver::model::stats::container {

using calc::AdditionStat;
using calc::Stat2;
using gameobjects::Item;
using gameobjects::player::Equipment;
using gameobjects::player::Player;
using gameobjects::state::CreatureState;
using runtime::Ptr;
using utils::stats::CalculationType;

namespace {

/** Java: a dereference of a null template reference (NullPointerException) */
template <class T>
const T& nonNull(const T* value, std::string_view what) {
	if (value == nullptr)
		throw runtime::NullPointerException(std::string(what) + " is null");
	return *value;
}

/** Java: item.getItemTemplate().getWeaponStats() */
const templates::item::WeaponStats& weaponStatsOf(Item& item) {
	return nonNull(nonNull(item.getItemTemplate(), "itemTemplate").getWeaponStats(), "weaponStats");
}

/** Java: item.getItemTemplate().getAttackType().isMagical() */
bool isMagicalWeapon(Item& item) {
	std::optional<templates::item::ItemAttackType> attackType = nonNull(item.getItemTemplate(), "itemTemplate").getAttackType();
	if (!attackType)
		throw runtime::NullPointerException("attackType is null");
	return templates::item::isMagical(*attackType);
}

/** Java: offHandWeapon.equals(mainHandWeapon) (AionObject.equals, false for null) */
bool sameItem(Item& item, Ptr<Item> other) {
	return other && item.equals(*other);
}

} // namespace

PlayerGameStats::PlayerGameStats(gameobjects::player::Player& ownerValue) : CreatureGameStats(ownerValue) {
	updateStatsTemplate();
}

PlayerGameStats::~PlayerGameStats() = default;

void PlayerGameStats::onStatsChange(Ptr<skillengine::model::Effect> effect) {
	CreatureGameStats::onStatsChange(effect);
	updateStatsVisually();
	checkSpeedStats();
}

void PlayerGameStats::updateStatsAndSpeedVisually() {
	onStatsChange(nullptr);
}

void PlayerGameStats::updateStatsVisually() {
	updateStatInfo();
}

bool PlayerGameStats::checkSpeedStats() {
	bool speedChanged = CreatureGameStats::checkSpeedStats();
	int32_t currentAttackSpeed = getAttackSpeed()->getCurrent();
	if (currentAttackSpeed != cachedAttackSpeed.get()) {
		if (!speedChanged) // prevent double packet broadcast (super.checkSpeedStats() already broadcasts on true)
			updateSpeedInfo();
		cachedAttackSpeed = currentAttackSpeed;
		return true;
	}
	return speedChanged;
}

void PlayerGameStats::updateStatsTemplate() {
	Player& player = static_cast<Player&>(owner);
	this->statsTemplate = createStatsTemplate(player.getPlayerClass(), player.getLevel());
}

std::unique_ptr<Stat2> PlayerGameStats::getMaxDp() {
	return getStat(StatEnum::MAXDP, 4000);
}

std::unique_ptr<Stat2> PlayerGameStats::getFlyTime() {
	return getStat(StatEnum::FLY_TIME, static_cast<float>(configs::main::CustomConfig::BASE_FLYTIME.load()));
}

int32_t PlayerGameStats::getBaseAttackSpeed() {
	Player& player = static_cast<Player&>(owner);
	int32_t base = 1500;
	Ptr<Item> mainHandWeapon = player.getEquipment().getMainHandWeapon();
	if (mainHandWeapon) {
		base = weaponStatsOf(*mainHandWeapon).getAttackSpeed();
		Ptr<Item> offWeapon = player.getEquipment().getOffHandWeapon();
		if (offWeapon.get() == mainHandWeapon.get())
			offWeapon = nullptr;
		if (offWeapon)
			base += weaponStatsOf(*offWeapon).getAttackSpeed() / 4;
	}
	return base;
}

std::unique_ptr<Stat2> PlayerGameStats::getMovementSpeed() {
	Player& player = static_cast<Player&>(owner);
	std::unique_ptr<Stat2> movementSpeed;
	const templates::stats::StatsTemplate& pst = nonNull(getStatsTemplate(), "statsTemplate");
	if (player.isInPlayerMode(actions::PlayerMode::RIDE)) {
		const templates::ride::RideInfo& ride = nonNull(player.ride.get(), "ride");
		// Java: (int) pst.getRunSpeed() * 1000 - the cast binds to the speed
		int32_t runSpeed = templates::detail::floatToInt(pst.getRunSpeed()) * 1000;
		if (player.isInState(CreatureState::FLYING)) {
			movementSpeed = std::make_unique<AdditionStat>(StatEnum::FLY_SPEED, static_cast<float>(runSpeed), player);
			movementSpeed->addToBonus(static_cast<float>(templates::detail::floatToInt(ride.getFlySpeed() * 1000) - runSpeed));
		} else {
			float speed = player.isInSprintMode() ? ride.getSprintSpeed() : ride.getMoveSpeed();
			movementSpeed = std::make_unique<AdditionStat>(StatEnum::SPEED, static_cast<float>(runSpeed), player);
			movementSpeed->addToBonus(static_cast<float>(templates::detail::floatToInt(speed * 1000) - runSpeed));
		}
	} else if (player.isInFlyingState())
		movementSpeed = getStat(StatEnum::FLY_SPEED, static_cast<float>(utils::JavaMath::round(pst.getFlySpeed() * 1000)));
	else if (player.isInState(CreatureState::FLYING) && !player.isInState(CreatureState::RESTING))
		movementSpeed = getStat(StatEnum::SPEED, 12000);
	else if (player.isInState(CreatureState::WALK_MODE))
		movementSpeed = getStat(StatEnum::SPEED, static_cast<float>(utils::JavaMath::round(pst.getWalkSpeed() * 1000)));
	else
		movementSpeed = getStat(StatEnum::SPEED, static_cast<float>(utils::JavaMath::round(pst.getRunSpeed() * 1000)));
	return movementSpeed;
}

std::unique_ptr<Stat2> PlayerGameStats::getAttackRange() {
	int32_t base = 1500;
	int32_t minWeaponRange = std::numeric_limits<int32_t>::max();
	Equipment& equipment = static_cast<Player&>(owner).getEquipment();
	Ptr<Item> mainHandWeapon = equipment.getMainHandWeapon();
	Ptr<Item> offHandWeapon = equipment.getOffHandWeapon();
	if (mainHandWeapon)
		minWeaponRange = weaponStatsOf(*mainHandWeapon).getAttackRange();
	if (offHandWeapon && !equipment.isShieldEquipped())
		minWeaponRange = std::min(minWeaponRange, weaponStatsOf(*offHandWeapon).getAttackRange());
	return getStat(StatEnum::ATTACK_RANGE, static_cast<float>(minWeaponRange == std::numeric_limits<int32_t>::max() ? base : minWeaponRange));
}

std::unique_ptr<Stat2> PlayerGameStats::getParry() {
	int32_t base = nonNull(getStatsTemplate(), "statsTemplate").getParry();
	Ptr<Item> mainHandWeapon = static_cast<Player&>(owner).getEquipment().getMainHandWeapon();
	if (mainHandWeapon) {
		base += weaponStatsOf(*mainHandWeapon).getParry();
	}
	return getStat(StatEnum::PARRY, static_cast<float>(base));
}

std::unique_ptr<Stat2> PlayerGameStats::getMainHandPAttack(const std::unordered_set<CalculationType>& calculationTypes) {
	float base = static_cast<float>(nonNull(getStatsTemplate(), "statsTemplate").getAttack());
	Equipment& equipment = static_cast<Player&>(owner).getEquipment();
	Ptr<Item> mainHandWeapon = equipment.getMainHandWeapon();
	if (mainHandWeapon) {
		if (isMagicalWeapon(*mainHandWeapon))
			return std::make_unique<AdditionStat>(StatEnum::PHYSICAL_ATTACK, 0.0f, owner);
		const templates::item::WeaponStats& weaponStats = weaponStatsOf(*mainHandWeapon);
		if (calculationTypes.contains(CalculationType::DISPLAY)) {
			base = weaponStats.getMeanDamage();
		} else {
			base = static_cast<float>(commons::utils::Rnd::get(weaponStats.getMinDamage(), weaponStats.getMaxDamage()));
		}
		if (calculationTypes.contains(CalculationType::APPLY_POWER_SHARD_DAMAGE)) {
			base += getPowerShardDamage(true, calculationTypes.contains(CalculationType::REMOVE_POWER_SHARD));
		}
	}
	std::unique_ptr<Stat2> stat = getStat(StatEnum::PHYSICAL_ATTACK, base, copyWith(calculationTypes, CalculationType::MAIN_HAND));
	applyStatFunctions(StatEnum::MAIN_HAND_POWER, *stat, calculationTypes);
	return stat;
}

std::unique_ptr<Stat2> PlayerGameStats::getOffHandPAttack(std::initializer_list<CalculationType> calculationTypes) {
	return getOffHandPAttack(toSet(std::span<const CalculationType>(calculationTypes.begin(), calculationTypes.size())));
}

std::unique_ptr<Stat2> PlayerGameStats::getOffHandPAttack(const std::unordered_set<CalculationType>& calculationTypes) {
	Equipment& equipment = static_cast<Player&>(owner).getEquipment();
	Ptr<Item> offHandWeapon = equipment.getOffHandWeapon();
	if (offHandWeapon && !sameItem(*offHandWeapon, equipment.getMainHandWeapon()) && nonNull(offHandWeapon->getItemTemplate(), "itemTemplate").isWeapon()) {
		float base;
		const templates::item::WeaponStats& weaponStats = weaponStatsOf(*offHandWeapon);
		if (calculationTypes.contains(CalculationType::DISPLAY)) {
			base = weaponStats.getMeanDamage();
		} else {
			base = static_cast<float>(commons::utils::Rnd::get(weaponStats.getMinDamage(), weaponStats.getMaxDamage()));
		}
		if (calculationTypes.contains(CalculationType::APPLY_POWER_SHARD_DAMAGE))
			base += getPowerShardDamage(false, calculationTypes.contains(CalculationType::REMOVE_POWER_SHARD));
		std::unique_ptr<Stat2> stat = getStat(StatEnum::PHYSICAL_ATTACK, base, copyWith(calculationTypes, CalculationType::OFF_HAND));
		if (calculationTypes.contains(CalculationType::DISPLAY)) {
			stat->setBaseRate(stat->getBaseRate() * getOffHandDamageRatio());
			stat->setBonusRate(stat->getBonusRate() * getOffHandDamageRatio());
		}
		applyStatFunctions(StatEnum::OFF_HAND_POWER, *stat, calculationTypes);
		return stat;
	}
	return std::make_unique<AdditionStat>(StatEnum::PHYSICAL_ATTACK, 0.0f, owner);
}

std::unique_ptr<Stat2> PlayerGameStats::getMainHandMAttack(const std::unordered_set<CalculationType>& calculationTypes) {
	float base = static_cast<float>(nonNull(getStatsTemplate(), "statsTemplate").getMagicalAttack());
	Equipment& equipment = static_cast<Player&>(owner).getEquipment();
	Ptr<Item> mainHandWeapon = equipment.getMainHandWeapon();
	if (mainHandWeapon) {
		if (!isMagicalWeapon(*mainHandWeapon))
			return std::make_unique<AdditionStat>(StatEnum::MAGICAL_ATTACK, 0.0f, owner);
		base = weaponStatsOf(*mainHandWeapon).getMeanDamage();
		if (calculationTypes.contains(CalculationType::APPLY_POWER_SHARD_DAMAGE))
			base += getPowerShardDamage(true, calculationTypes.contains(CalculationType::REMOVE_POWER_SHARD));
	}
	std::unique_ptr<Stat2> stat = getStat(StatEnum::MAGICAL_ATTACK, base, copyWith(calculationTypes, CalculationType::MAIN_HAND));
	applyStatFunctions(StatEnum::MAIN_HAND_POWER, *stat, calculationTypes);
	return stat;
}

std::unique_ptr<Stat2> PlayerGameStats::getOffHandMAttack(std::initializer_list<CalculationType> calculationTypes) {
	return getOffHandMAttack(toSet(std::span<const CalculationType>(calculationTypes.begin(), calculationTypes.size())));
}

std::unique_ptr<Stat2> PlayerGameStats::getOffHandMAttack(const std::unordered_set<CalculationType>& calculationTypes) {
	Equipment& equipment = static_cast<Player&>(owner).getEquipment();
	Ptr<Item> offHandWeapon = equipment.getOffHandWeapon();
	if (offHandWeapon && !sameItem(*offHandWeapon, equipment.getMainHandWeapon()) && nonNull(offHandWeapon->getItemTemplate(), "itemTemplate").isWeapon()) {
		float base = weaponStatsOf(*offHandWeapon).getMeanDamage();
		if (calculationTypes.contains(CalculationType::APPLY_POWER_SHARD_DAMAGE))
			base += getPowerShardDamage(false, calculationTypes.contains(CalculationType::REMOVE_POWER_SHARD));
		std::unique_ptr<Stat2> stat = getStat(StatEnum::MAGICAL_ATTACK, base, copyWith(calculationTypes, CalculationType::OFF_HAND));
		if (calculationTypes.contains(CalculationType::DISPLAY)) {
			stat->setBaseRate(stat->getBaseRate() * getOffHandDamageRatio());
			stat->setBonusRate(stat->getBonusRate() * getOffHandDamageRatio());
		}
		applyStatFunctions(StatEnum::OFF_HAND_POWER, *stat, calculationTypes);
		return stat;
	}
	return std::make_unique<AdditionStat>(StatEnum::MAGICAL_ATTACK, 0.0f, owner);
}

std::unique_ptr<Stat2> PlayerGameStats::getMainHandPCritical() {
	int32_t base = nonNull(getStatsTemplate(), "statsTemplate").getPcrit();
	Equipment& equipment = static_cast<Player&>(owner).getEquipment();
	Ptr<Item> mainHandWeapon = equipment.getMainHandWeapon();
	if (mainHandWeapon && !isMagicalWeapon(*mainHandWeapon)) {
		base += weaponStatsOf(*mainHandWeapon).getCritical();
	}
	return getStat(StatEnum::PHYSICAL_CRITICAL, static_cast<float>(base));
}

std::unique_ptr<Stat2> PlayerGameStats::getOffHandPCritical() {
	Equipment& equipment = static_cast<Player&>(owner).getEquipment();
	Ptr<Item> offHandWeapon = equipment.getOffHandWeapon();
	if (offHandWeapon && !sameItem(*offHandWeapon, equipment.getMainHandWeapon()) && nonNull(offHandWeapon->getItemTemplate(), "itemTemplate").isWeapon()
		&& !isMagicalWeapon(*offHandWeapon)) {
		int32_t base = nonNull(getStatsTemplate(), "statsTemplate").getPcrit();
		base += weaponStatsOf(*offHandWeapon).getCritical();
		return getStat(StatEnum::PHYSICAL_CRITICAL, static_cast<float>(base));
	}
	return std::make_unique<AdditionStat>(StatEnum::PHYSICAL_CRITICAL, 0.0f, owner);
}

std::unique_ptr<Stat2> PlayerGameStats::getMainHandPAccuracy() {
	int32_t base = nonNull(getStatsTemplate(), "statsTemplate").getAccuracy();
	Ptr<Item> mainHandWeapon = static_cast<Player&>(owner).getEquipment().getMainHandWeapon();
	if (mainHandWeapon) {
		base += weaponStatsOf(*mainHandWeapon).getPhysicalAccuracy();
	}
	return getStat(StatEnum::PHYSICAL_ACCURACY, static_cast<float>(base));
}

std::unique_ptr<Stat2> PlayerGameStats::getOffHandPAccuracy() {
	Equipment& equipment = static_cast<Player&>(owner).getEquipment();
	Ptr<Item> offHandWeapon = equipment.getOffHandWeapon();
	if (offHandWeapon && !sameItem(*offHandWeapon, equipment.getMainHandWeapon()) && nonNull(offHandWeapon->getItemTemplate(), "itemTemplate").isWeapon()) {
		int32_t base = nonNull(getStatsTemplate(), "statsTemplate").getAccuracy();
		base += weaponStatsOf(*offHandWeapon).getPhysicalAccuracy();
		return getStat(StatEnum::PHYSICAL_ACCURACY, static_cast<float>(base));
	}
	return std::make_unique<AdditionStat>(StatEnum::PHYSICAL_ACCURACY, 0.0f, owner);
}

std::unique_ptr<Stat2> PlayerGameStats::getMBoost() {
	int32_t base = nonNull(getStatsTemplate(), "statsTemplate").getMagicBoost();
	Ptr<Item> mainHandWeapon = static_cast<Player&>(owner).getEquipment().getMainHandWeapon();
	if (mainHandWeapon) {
		base += weaponStatsOf(*mainHandWeapon).getBoostMagicalSkill();
	}
	return getStat(StatEnum::BOOST_MAGICAL_SKILL, static_cast<float>(base));
}

std::unique_ptr<Stat2> PlayerGameStats::getMAccuracy() {
	int32_t base = nonNull(getStatsTemplate(), "statsTemplate").getMacc();
	Ptr<Item> mainHandWeapon = static_cast<Player&>(owner).getEquipment().getMainHandWeapon();
	if (mainHandWeapon) {
		base += weaponStatsOf(*mainHandWeapon).getMagicalAccuracy();
	}
	return getStat(StatEnum::MAGICAL_ACCURACY, static_cast<float>(base));
}

std::unique_ptr<Stat2> PlayerGameStats::getMCritical() {
	int32_t base = nonNull(getStatsTemplate(), "statsTemplate").getMcrit();
	Ptr<Item> mainHandWeapon = static_cast<Player&>(owner).getEquipment().getMainHandWeapon();
	if (mainHandWeapon && isMagicalWeapon(*mainHandWeapon)) {
		base += weaponStatsOf(*mainHandWeapon).getCritical();
	}
	return getStat(StatEnum::MAGICAL_CRITICAL, static_cast<float>(base));
}

std::unique_ptr<Stat2> PlayerGameStats::getHpRegenRate() {
	Player& player = static_cast<Player&>(owner);
	int32_t base = player.getLevel() + 3;
	if (player.isInState(CreatureState::RESTING))
		base *= 8;
	// Java: base *= getHealth().getCurrent() / 100f (a compound assignment with an implicit (int) cast)
	base = templates::detail::floatToInt(base * (getHealth()->getCurrent() / 100.0f));
	return getStat(StatEnum::REGEN_HP, static_cast<float>(base));
}

std::unique_ptr<Stat2> PlayerGameStats::getMpRegenRate() {
	Player& player = static_cast<Player&>(owner);
	int32_t base = player.getLevel() + 8;
	if (player.isInState(CreatureState::RESTING))
		base *= 8;
	// Java: base *= getWill().getCurrent() / 100f (a compound assignment with an implicit (int) cast)
	base = templates::detail::floatToInt(base * (getWill()->getCurrent() / 100.0f));
	return getStat(StatEnum::REGEN_MP, static_cast<float>(base));
}

void PlayerGameStats::updateStatInfo() {
	Player& player = static_cast<Player&>(owner);
	utils::PacketSendUtility::sendPacket(player, network::aion::serverpackets::SM_STATS_INFO(player));
}

void PlayerGameStats::updateSpeedInfo() {
	utils::PacketSendUtility::broadcastToSightedPlayers(owner, network::aion::serverpackets::SM_EMOTION(owner, EmotionType::CHANGE_SPEED), true);
}

int32_t PlayerGameStats::getHealthDependentAdditionalHp() {
	std::unique_ptr<Stat2> health = getHealth();
	return calculateBaseStatDependentAdditionalValue(*health, getHealthMultiplier(static_cast<Player&>(owner).getPlayerClass()));
}

int32_t PlayerGameStats::getWillDependentAdditionalMp() {
	std::unique_ptr<Stat2> will = getWill();
	return calculateBaseStatDependentAdditionalValue(*will, getWillMultiplier(static_cast<Player&>(owner).getPlayerClass()));
}

int32_t PlayerGameStats::getAgilityDependentAdditionalBaseBlock() {
	std::unique_ptr<Stat2> agility = getAgility();
	return calculateBaseStatDependentAdditionalValue(*agility, getAgilityMultiplier(static_cast<Player&>(owner).getPlayerClass()));
}

int32_t PlayerGameStats::getAgilityDependentAdditionalBaseParry() {
	std::unique_ptr<Stat2> agility = getAgility();
	return calculateBaseStatDependentAdditionalValue(*agility, getAgilityMultiplier(static_cast<Player&>(owner).getPlayerClass()));
}

int32_t PlayerGameStats::getAgilityDependentAdditionalBaseEvasion() {
	std::unique_ptr<Stat2> agility = getAgility();
	return calculateBaseStatDependentAdditionalValue(*agility, getAgilityMultiplier(static_cast<Player&>(owner).getPlayerClass()));
}

int32_t PlayerGameStats::getAccuracyDependentAdditionalBasePhysicalAccuracy() {
	std::unique_ptr<Stat2> accuracy = getAccuracy();
	return calculateBaseStatDependentAdditionalValue(*accuracy, getAccuracyMultiplier(static_cast<Player&>(owner).getPlayerClass()));
}

int32_t PlayerGameStats::getAccuracyDependentAdditionalBasePhysicalCritical() {
	std::unique_ptr<Stat2> accuracy = getAccuracy();
	return calculateBaseStatDependentAdditionalValue(*accuracy, getAccuracyMultiplier(static_cast<Player&>(owner).getPlayerClass()) / 20);
}

int32_t PlayerGameStats::calculateBaseStatDependentAdditionalValue(Stat2& baseStat, int32_t multiplier) {
	return templates::detail::floatToInt((baseStat.getCurrent() - 100) / 100.0f * multiplier);
}

int32_t PlayerGameStats::getPowerShardDamage(bool mainHand, bool removePowerShards) {
	Player& player = static_cast<Player&>(owner);
	if (player.isInState(CreatureState::POWERSHARD)) {
		Equipment& equipment = player.getEquipment();
		Ptr<Item> weapon = mainHand ? equipment.getMainHandWeapon() : equipment.getOffHandWeapon();
		Ptr<Item> firstShard = equipment.getMainHandPowerShard();
		Ptr<Item> secondShard = equipment.getOffHandPowerShard();
		if (weapon && nonNull(weapon->getItemTemplate(), "itemTemplate").getItemSubType() != templates::item::enums::ItemSubType::SHIELD) {
			int32_t dmg = 0;
			if (mainHand) {
				if (firstShard) {
					dmg = nonNull(firstShard->getItemTemplate(), "itemTemplate").getWeaponBoost();
					if (removePowerShards)
						player.getEquipment().usePowerShard(*firstShard, 1);
				}
				if (weapon->getItemTemplate()->isTwoHandWeapon() && secondShard) {
					dmg += nonNull(secondShard->getItemTemplate(), "itemTemplate").getWeaponBoost();
					if (removePowerShards)
						player.getEquipment().usePowerShard(*secondShard, 1);
				}
			} else if (secondShard) {
				dmg = nonNull(secondShard->getItemTemplate(), "itemTemplate").getWeaponBoost();
				if (removePowerShards)
					player.getEquipment().usePowerShard(*secondShard, 1);
			}
			return dmg;
		}
	}
	return 0;
}

float PlayerGameStats::getOffHandDamageRatio() {
	return getMinDamageRatio() * (1 - getMaxDamageChance() / 1000.0f) + getMaxDamageChance() / 1000.0f;
}

} // namespace aion::gameserver::model::stats::container
