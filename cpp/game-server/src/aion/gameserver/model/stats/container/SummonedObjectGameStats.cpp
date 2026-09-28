#include "aion/gameserver/model/stats/container/SummonedObjectGameStats.h"

#include <cstdint>

#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/model/gameobjects/Npc.h"
#include "aion/gameserver/model/stats/calc/Stat2.h"
#include "aion/gameserver/model/stats/container/StatEnum.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"

namespace aion::gameserver::model::stats::container {

SummonedObjectGameStats::SummonedObjectGameStats(gameobjects::Npc& ownerValue) : NpcGameStats(ownerValue) {
}

SummonedObjectGameStats::~SummonedObjectGameStats() = default;

std::unique_ptr<calc::Stat2> SummonedObjectGameStats::getStat(StatEnum statEnum, float base,
	const std::unordered_set<utils::stats::CalculationType>& calculationTypes) {
	std::unique_ptr<calc::Stat2> stat = NpcGameStats::getStat(statEnum, base, calculationTypes);
	runtime::Ptr<gameobjects::Creature> master = static_cast<gameobjects::Npc&>(owner).getMaster();
	if (!master)
		return stat;
	// Java returns the Stat2 that getItemStatBoost gives back, which is the argument itself.
	switch (statEnum) {
		case StatEnum::MAGICAL_ATTACK:
		case StatEnum::MAGICAL_ACCURACY:
		case StatEnum::MAGICAL_RESIST:
			stat->setBonusRate(0.2f);
			master->getGameStats()->getItemStatBoost(statEnum, *stat);
			return stat;
		case StatEnum::PHYSICAL_ACCURACY:
			stat->setBonusRate(0.2f);
			master->getGameStats()->getItemStatBoost(StatEnum::MAIN_HAND_ACCURACY, *stat);
			master->getGameStats()->getItemStatBoost(statEnum, *stat);
			return stat;
		case StatEnum::PHYSICAL_ATTACK:
			stat->setBonusRate(0.2f);
			master->getGameStats()->getItemStatBoost(StatEnum::MAIN_HAND_POWER, *stat);
			master->getGameStats()->getItemStatBoost(statEnum, *stat);
			return stat;
		default:
			break;
	}
	return stat;
}

std::unique_ptr<calc::Stat2> SummonedObjectGameStats::getMBoost() {
	// Java: getStat(StatEnum.BOOST_MAGICAL_SKILL, (int) (owner.getMaster().getGameStats().getMBoost().getCurrent() * 0.6f))
	runtime::Ptr<gameobjects::Creature> master = static_cast<gameobjects::Npc&>(owner).getMaster();
	// C++ only (deviation, docs/deviations/P5-01.md): SummonedObject.getMaster() returns the object itself when its creator is no Creature (a
	// house npc), so Java recurses here until the stack is exhausted and throws StackOverflowError, which the calling thread survives. The same
	// recursion is an unrecoverable crash of the whole process here, so the self-master case falls back to the npc's own template boost
	// (CreatureGameStats.getMBoost). No M5a path reads it: house npcs never cast and SM_STATS_INFO reads PlayerGameStats.
	gameobjects::Creature& ownerCreature = static_cast<gameobjects::Npc&>(owner);
	if (master.rawPointer() == &ownerCreature)
		return NpcGameStats::getMBoost();
	int32_t base = static_cast<int32_t>(static_cast<float>(master->getGameStats()->getMBoost()->getCurrent()) * 0.6f);
	return getStat(StatEnum::BOOST_MAGICAL_SKILL, static_cast<float>(base));
}

} // namespace aion::gameserver::model::stats::container
