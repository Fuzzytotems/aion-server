#include "aion/gameserver/model/stats/container/SummonGameStats.h"

#include <cstdint>
#include <memory>
#include <string>
#include <unordered_set>

#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/model/gameobjects/Summon.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/stats/calc/Stat2.h"
#include "aion/gameserver/model/stats/calc/functions/IStatFunction.h"
#include "aion/gameserver/model/stats/container/CreatureLifeStats.h"
#include "aion/gameserver/model/stats/container/StatEnum.h"
#include "aion/gameserver/model/summons/SummonMode.h"
#include "aion/gameserver/model/templates/npc/NpcTemplate.h"
#include "aion/gameserver/model/templates/stats/StatsTemplate.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SUMMON_UPDATE.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/utils/JavaMath.h"
#include "aion/gameserver/utils/PacketSendUtility.h"

namespace aion::gameserver::model::stats::container {

namespace {

/** Java: getObjectTemplate().getX() - a NullPointerException where the summon has no template (like NpcGameStats.cpp) */
const templates::npc::NpcTemplate& nonNull(const templates::npc::NpcTemplate* npcTemplate) {
	if (npcTemplate == nullptr)
		throw runtime::NullPointerException("the summon has no object template");
	return *npcTemplate;
}

/** Java: getStatsTemplate().getX() - NpcTemplate.statsTemplate is a nullable field */
const templates::stats::StatsTemplate& nonNull(const templates::stats::StatsTemplate* statsTemplate) {
	if (statsTemplate == nullptr)
		throw runtime::NullPointerException("the summon has no stats template");
	return *statsTemplate;
}

/** Java int a * b (wraps on overflow) */
constexpr int32_t mulInt(int32_t a, int32_t b) noexcept {
	return static_cast<int32_t>(static_cast<uint32_t>(a) * static_cast<uint32_t>(b));
}

} // namespace

SummonGameStats::SummonGameStats(gameobjects::Summon& ownerValue) : CreatureGameStats(ownerValue) {
}

SummonGameStats::~SummonGameStats() = default;

void SummonGameStats::onStatsChange(runtime::Ptr<skillengine::model::Effect> /*effect*/) {
	updateStatsAndSpeedVisually();
}

void SummonGameStats::updateStatsAndSpeedVisually() {
	updateStatsVisually();
	checkSpeedStats();
}

void SummonGameStats::updateStatsVisually() {
	static_cast<gameobjects::Summon&>(owner).getGameStats()->updateStatInfo();
}

std::unique_ptr<calc::Stat2> SummonGameStats::getStat(StatEnum statEnum, float base,
	const std::unordered_set<utils::stats::CalculationType>& calculationTypes) {
	std::unique_ptr<calc::Stat2> stat = CreatureGameStats::getStat(statEnum, base, calculationTypes);
	gameobjects::Summon& summon = static_cast<gameobjects::Summon&>(owner);
	if (summon.getMaster() == nullptr)
		return stat;
	const std::string name = nonNull(summon.getObjectTemplate()).getName();
	switch (statEnum) {
		case StatEnum::MAXHP:
		case StatEnum::PHYSICAL_ATTACK:
		case StatEnum::MAGICAL_ATTACK:
		case StatEnum::EVASION:
		case StatEnum::PARRY:
		case StatEnum::PHYSICAL_DEFENSE:
		case StatEnum::MAGICAL_DEFEND:
		case StatEnum::MAGIC_SKILL_BOOST_RESIST:
		case StatEnum::MAGICAL_CRITICAL:
		case StatEnum::MAGICAL_RESIST:
			getStatWithBonusRate(statEnum, *stat, 0.5f);
			return stat;
		case StatEnum::PHYSICAL_CRITICAL:
			getStatWithBonusRate(StatEnum::MAIN_HAND_CRITICAL, *stat, 0.5f);
			return stat;
		case StatEnum::PHYSICAL_ACCURACY:
			getStatWithBonusRate(StatEnum::MAIN_HAND_ACCURACY, *stat, 0.5f);
			return stat;
		case StatEnum::BOOST_MAGICAL_SKILL:
		case StatEnum::MAGICAL_ACCURACY:
			getStatWithBonusRate(statEnum, *stat, 0.8f);
			return stat;
		case StatEnum::PARALYZE_RESISTANCE:
		case StatEnum::SLEEP_RESISTANCE:
		case StatEnum::POISON_RESISTANCE:
			if (name == "lava spirit" || name == "tempest spirit") {
				stat->addToBase(100);
			}
			break;
		case StatEnum::EARTH_RESISTANCE:
			if (name == "lava spirit") {
				stat->addToBase(200);
			} else if (name == "wind spirit") {
				stat->addToBase(-200);
			}
			break;
		case StatEnum::FIRE_RESISTANCE:
			if (name == "lava spirit") {
				stat->addToBase(200);
			} else if (name == "water spirit") {
				stat->addToBase(-200);
			}
			break;
		case StatEnum::WIND_RESISTANCE:
			if (name == "tempest spirit") {
				stat->addToBase(200);
			} else if (name == "earth spirit") {
				stat->addToBase(-200);
			}
			break;
		case StatEnum::WATER_RESISTANCE:
			if (name == "tempest spirit") {
				stat->addToBase(200);
			} else if (name == "fire spirit") {
				stat->addToBase(-200);
			}
			break;
		default:
			break;
	}
	return stat;
}

// Java returns the master's getItemStatBoost(statEnum, stat), which is `stat` itself with the master's item bonus applied
calc::Stat2& SummonGameStats::getStatWithBonusRate(StatEnum statEnum, calc::Stat2& stat, float bonusRate) {
	gameobjects::Summon& summon = static_cast<gameobjects::Summon&>(owner);
	calc::Stat2& statToReturn = summon.getMaster()->getGameStats()->getItemStatBoost(statEnum, stat);
	statToReturn.setBonusRate(bonusRate);
	return statToReturn;
}

const templates::stats::StatsTemplate* SummonGameStats::getStatsTemplate() {
	return nonNull(static_cast<gameobjects::Summon&>(owner).getObjectTemplate()).getStatsTemplate();
}

int32_t SummonGameStats::getBaseAttackSpeed() {
	return nonNull(static_cast<gameobjects::Summon&>(owner).getObjectTemplate()).getAttackSpeed();
}

std::unique_ptr<calc::Stat2> SummonGameStats::getMovementSpeed() {
	int32_t bonusSpeed = 0;
	runtime::Ptr<gameobjects::Creature> master = static_cast<gameobjects::Summon&>(owner).getMaster();
	if (master != nullptr && master->isFlying()) {
		bonusSpeed += 3000;
	}
	return getStat(StatEnum::SPEED, static_cast<float>(utils::JavaMath::round(nonNull(getStatsTemplate()).getRunSpeed() * 1000) + bonusSpeed));
}

std::unique_ptr<calc::Stat2> SummonGameStats::getAttackRange() {
	// Java: getAttackRange() * 1000 is an int product
	int32_t attackRange = nonNull(static_cast<gameobjects::Summon&>(owner).getObjectTemplate()).getAttackRange();
	return getStat(StatEnum::ATTACK_RANGE, static_cast<float>(mulInt(attackRange, 1000)));
}

std::unique_ptr<calc::Stat2> SummonGameStats::getHpRegenRate() {
	gameobjects::Summon& summon = static_cast<gameobjects::Summon&>(owner);
	// Java: (int) (maxHp * float), a narrowing of the float product
	int32_t base = static_cast<int32_t>(static_cast<float>(summon.getLifeStats()->getMaxHp()) *
		(summon.getMode() == summons::SummonMode::REST ? 0.05f : 0.025f));
	return getStat(StatEnum::REGEN_HP, static_cast<float>(base));
}

std::unique_ptr<calc::Stat2> SummonGameStats::getMpRegenRate() {
	throw runtime::IllegalStateException("No mp regen for Summon");
}

void SummonGameStats::updateStatInfo() {
	gameobjects::Summon& summon = static_cast<gameobjects::Summon&>(owner);
	if (runtime::Ptr<gameobjects::player::Player> master = runtime::as<gameobjects::player::Player>(summon.getMaster())) {
		utils::PacketSendUtility::sendPacket(*master, network::aion::serverpackets::SM_SUMMON_UPDATE(summon));
	}
}

} // namespace aion::gameserver::model::stats::container
