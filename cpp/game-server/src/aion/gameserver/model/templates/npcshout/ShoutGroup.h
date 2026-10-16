#pragma once

#include <vector>

#include "aion/gameserver/model/templates/npcshout/ShoutGroup.xml.h"

namespace aion::gameserver::model::templates::npcshout {

/**
 * Java com.aionemu.gameserver.model.templates.npcshout.ShoutGroup.
 * <p>
 * C++: `getShoutNpcs()` is const, so NpcShoutData.afterUnmarshal's `group.getShoutNpcs().remove(i)` and `group.makeNull()` are skipped by the
 * holder (P4-09), which keeps the bound lists its index points into (ShoutList.h).
 *
 * @author Rolandas
 */
class ShoutGroup : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/model/templates/npcshout/ShoutGroup.xml.inc"
public:
	/** Java creates the list on first use (a write to the template); the C++ list always exists */
	const std::vector<ShoutList>& getShoutNpcs() const { return shoutNpcs; }

	/**
	 * Java: drops the shout lists and the client ai (NpcShoutData.afterUnmarshal). C++: destroys the bound lists, so NpcShoutData never calls it
	 * (its index points into them, static-data.md §2.6); ported for completeness only.
	 */
	void makeNull();
};

} // namespace aion::gameserver::model::templates::npcshout
