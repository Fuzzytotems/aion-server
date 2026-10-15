#pragma once

#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/world/knownlist/CreatureAwareKnownList.h"
#include "aion/gameserver/world/knownlist/fwd.h"

namespace aion::gameserver::world::knownlist {

/**
 * The known list of npcs: it is only updated while the npc's map region is active, otherwise it is cleared.
 * <p>
 * C++: a KnownList part (`setKnownlist(std::make_unique<NpcKnownList>(*this))`).
 *
 * @author ATracer
 */
class NpcKnownList : public CreatureAwareKnownList {
public:
	explicit NpcKnownList(model::gameobjects::VisibleObject& owner);
	~NpcKnownList() override;

	void update() override;
};

} // namespace aion::gameserver::world::knownlist
