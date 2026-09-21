#pragma once

#include <memory>
#include <unordered_set>

#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/model/stats/calc/fwd.h"
#include "aion/gameserver/model/stats/container/NpcGameStats.h"
#include "aion/gameserver/model/stats/container/fwd.h"
#include "aion/gameserver/utils/stats/fwd.h"

namespace aion::gameserver::model::stats::container {

/**
 * S0c declaration header (docs/design/hub-headers.md §3.5). The game stats part of SummonedObject (`setGameStats(new
 * SummonedObjectGameStats(this))` in SummonedObject::setupStatContainers, P4-11a). It borrows the master's item stat boosts for the five
 * combat stats Java lists and scales the master's magic boost.
 * <p>
 * Java widens the 3-argument getStat of CreatureGameStats (protected) to public, as SummonGameStats does; the 2-argument base overload stays
 * visible through a using-declaration.
 * <p>
 * Java note: SummonedObject.getMaster() returns the object itself when its creator is no Creature (a house npc, whose creator is a House), so
 * Java's getMBoost() calls itself and recurses until the stack is exhausted (StackOverflowError, which the calling thread survives). That
 * recursion would crash this process, so the self-master case falls back to CreatureGameStats.getMBoost (deviation,
 * docs/deviations/P5-01.md). No M5a path reaches it: house npcs never cast, and getMBoost is only read for magic attacks.
 *
 * @author ATracer
 */
class SummonedObjectGameStats : public NpcGameStats {
public:
	explicit SummonedObjectGameStats(gameobjects::Npc& owner);
	~SummonedObjectGameStats() override;

	using CreatureGameStats::getStat;

	std::unique_ptr<calc::Stat2> getStat(StatEnum statEnum, float base,
		const std::unordered_set<utils::stats::CalculationType>& calculationTypes) override;

	std::unique_ptr<calc::Stat2> getMBoost() override;
};

} // namespace aion::gameserver::model::stats::container
