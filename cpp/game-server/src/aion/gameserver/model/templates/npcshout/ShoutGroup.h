#pragma once

#include <vector>

#include "aion/gameserver/model/templates/npcshout/ShoutGroup.xml.h"

namespace aion::gameserver::model::templates::npcshout {

/**
 * Java com.aionemu.gameserver.model.templates.npcshout.ShoutGroup.
 * <p>
 * C++: `getShoutNpcs()` is const; the holder (P4-09) keeps the bound lists its index points into (ShoutList.h).
 *
 * @author Rolandas
 */
class ShoutGroup : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/model/templates/npcshout/ShoutGroup.xml.inc"
public:
	/** Java: an empty list without shout lists (List.of()); the C++ list always exists */
	const std::vector<ShoutList>& getShoutNpcs() const { return shoutNpcs; }
};

} // namespace aion::gameserver::model::templates::npcshout
