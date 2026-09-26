#pragma once

#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/model/stats/container/CreatureLifeStats.h"
#include "aion/gameserver/model/stats/container/fwd.h"

namespace aion::gameserver::model::stats::container {

/**
 * S0c declaration header (docs/design/hub-headers.md §3.5). The life stats part of Npc (`setLifeStats(std::make_unique<NpcLifeStats>(*this))`
 * in Npc::setupStatContainers, after the game stats). The constructor reads the owner's max HP/MP from its game stats (Java super call) and is
 * ported.
 *
 * @author ATracer
 */
class NpcLifeStats : public CreatureLifeStats {
public:
	explicit NpcLifeStats(gameobjects::Npc& owner);
	~NpcLifeStats() override;

	/** Narrowing accessor (Java CreatureLifeStats<Npc>.getOwner(), hub-headers.md §8.2) */
	gameobjects::Npc& getOwner() const;

	void triggerRestoreTask() override; // synchronized (restoreLock)
};

} // namespace aion::gameserver::model::stats::container
