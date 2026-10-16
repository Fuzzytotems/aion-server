#include "aion/gameserver/model/stats/container/TrapGameStats.h"

#include <cstdint>
#include <memory>
#include <string>

#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/model/gameobjects/Npc.h"
#include "aion/gameserver/model/stats/calc/Stat2.h"
#include "aion/gameserver/model/stats/container/StatEnum.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"

namespace aion::gameserver::model::stats::container {

TrapGameStats::TrapGameStats(gameobjects::Npc& ownerValue) : NpcGameStats(ownerValue) {
}

TrapGameStats::~TrapGameStats() = default;

std::unique_ptr<calc::Stat2> TrapGameStats::getStat(StatEnum statEnum, float base,
	const std::unordered_set<utils::stats::CalculationType>& calculationTypes) {
	std::unique_ptr<calc::Stat2> stat = NpcGameStats::getStat(statEnum, base, calculationTypes);
	runtime::Ptr<gameobjects::Creature> master = static_cast<gameobjects::Npc&>(owner).getMaster();
	if (master == nullptr)
		return stat;
	switch (statEnum) {
		case StatEnum::BOOST_MAGICAL_SKILL:
		case StatEnum::MAGICAL_ACCURACY:
			// bonus is calculated from stat bonus of master (only green value)
			stat->setBonusRate(0.7f); // TODO: retail formula?
			// Java returns the Stat2 that getItemStatBoost gives back, which is the argument itself
			master->getGameStats()->getItemStatBoost(statEnum, *stat);
			return stat;
		default:
			break;
	}
	return stat;
}

std::unique_ptr<calc::Stat2> TrapGameStats::getAttackRange() {
	int32_t base = 5;
	const std::string ownerName = static_cast<gameobjects::Npc&>(owner).getName();
	if (ownerName == "destruction trap" || ownerName == "explosion trap" || ownerName == "sandstorm trap" || ownerName == "skybound trap" ||
		ownerName == "spike bite trap" || ownerName == "storm mine" || ownerName == "scrapped mechanisms") {
		base = 10;
	} else if (ownerName == "trap of clairvoyance") {
		base = 30;
	} else if (ownerName == "propelling trap") {
		base = 3;
	}
	return getStat(StatEnum::ATTACK_RANGE, static_cast<float>(base));
}

std::unique_ptr<calc::Stat2> TrapGameStats::getMAccuracy() {
	int32_t value = 1000;
	const std::string name = static_cast<gameobjects::Npc&>(owner).getName();
	if (name == "destruction trap") {
		value = 1876;
	} else if (name == "spike bite trap" || name == "explosion trap" || name == "spike trap" || name == "sleep trap" || name == "sandstorm trap" ||
		name == "propelling trap" || name == "poisoning trap" || name == "trap of slowing" || name == "blazing trap" ||
		name == "glue trap" // spike trap
		|| name == "trap of dust" // sandstorm
		|| name == "shock trap" // propelling trap
		|| name == "trap of sleep" || name == "trap of burst" || name == "collision trap") {
		value = 2361;
	} else if (name == "storm mine" || name == "skybound trap" || name == "trap of vengeful spirit") { // trap of vengeful spirit: skybound trap
		value = 2406;
	} else if (name == "trap of clairvoyance") {
		value = 1050;
	} else if (name == "snare trap" || name == "scrapped mechanisms") {
		value = 2528;
	}
	return getStat(StatEnum::MAGICAL_ACCURACY, static_cast<float>(value));
}

} // namespace aion::gameserver::model::stats::container
