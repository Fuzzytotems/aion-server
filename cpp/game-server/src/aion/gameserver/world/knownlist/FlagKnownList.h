#pragma once

#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/world/knownlist/PlayerAwareKnownList.h"
#include "aion/gameserver/world/knownlist/fwd.h"

namespace aion::gameserver::world::knownlist {

/**
 * The known list of flag npcs: every player of the flag's map instance knows it, regardless of the distance.
 * <p>
 * C++: a KnownList part. Deviation (runtime-architecture.md §5.3 RR-17): Java's update removes players of other map instances only from this
 * list (`knownObjects.values().removeIf(...)`), which leaves the flag in those players' lists and breaks the two-way relation; the port removes
 * both edges with KnownList::delPair, without notifications like removeIf. The pair adds go through KnownList::addPair.
 */
class FlagKnownList : public PlayerAwareKnownList {
public:
	/** @throws IllegalArgumentException if the owner is not a flag */
	explicit FlagKnownList(model::gameobjects::Npc& owner);
	~FlagKnownList() override;

	void update() override; // synchronized

protected:
	float getVisibleDistance() override;
};

} // namespace aion::gameserver::world::knownlist
