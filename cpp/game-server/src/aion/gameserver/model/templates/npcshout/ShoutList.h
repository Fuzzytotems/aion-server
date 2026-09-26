#pragma once

#include <cstdint>
#include <vector>

#include "aion/gameserver/model/templates/npcshout/ShoutList.xml.h"

namespace aion::gameserver::model::templates::npcshout {

/**
 * Java com.aionemu.gameserver.model.templates.npcshout.ShoutList.
 * <p>
 * C++: the getters are const (docs/deviations/P4-07b.md), so Java NpcShoutData.afterUnmarshal's `getNpcIds().remove(j)`,
 * `getNpcShouts().clear()` and `makeNull()` have no C++ counterpart. The holder (P4-09) builds its `shoutsByWorldNpcs` index from
 * `const NpcShout*` into the bound lists, visiting the groups forward and the lists and npc ids backwards like Java, and leaves the lists
 * untouched: makeNull destroys the NpcShout objects such an index points to.
 *
 * @author Rolandas
 */
class ShoutList : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/model/templates/npcshout/ShoutList.xml.inc"
public:
	/** Java creates the list on first use (a write to the template); the C++ list always exists */
	const std::vector<NpcShout>& getNpcShouts() const { return npcShouts; }

	/** Java creates the list on first use; the C++ getter returns an empty list without npc_ids */
	const std::vector<int32_t>& getNpcIds() const;

	/** @return 0 without restrict_world */
	int32_t getRestrictWorld() const { return restrictWorld.value_or(0); }

	/**
	 * Java: drops the npc ids, shouts and world restriction (NpcShoutData.afterUnmarshal). C++: destroys the bound shouts, so NpcShoutData
	 * never calls it (its index points into them, static-data.md §2.6); ported for completeness only.
	 */
	void makeNull();
};

} // namespace aion::gameserver::model::templates::npcshout
