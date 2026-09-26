#pragma once

#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/model/stats/container/CreatureLifeStats.h"
#include "aion/gameserver/model/stats/container/fwd.h"

namespace aion::gameserver::model::stats::container {

/**
 * S0c declaration header (docs/design/hub-headers.md §3.5). The life stats part of Summon (Summon::postConstruct, after the game stats). The
 * constructor reads the owner's max HP/MP from its game stats (Java super call) and is ported.
 *
 * @author ATracer
 */
class SummonLifeStats : public CreatureLifeStats {
public:
	explicit SummonLifeStats(gameobjects::Summon& owner);
	~SummonLifeStats() override;

	/** Narrowing accessor (Java CreatureLifeStats<Summon>.getOwner(), hub-headers.md §8.2) */
	gameobjects::Summon& getOwner() const;

	void triggerRestoreTask() override; // synchronized (restoreLock)
};

} // namespace aion::gameserver::model::stats::container
