#pragma once

#include <cstdint>
#include <vector>

#include "aion/gameserver/model/templates/npcshout/ShoutList.xml.h"

namespace aion::gameserver::model::templates::npcshout {

/**
 * Java com.aionemu.gameserver.model.templates.npcshout.ShoutList.
 * <p>
 * C++: the getters are const (docs/deviations/P4-07b.md). The holder (P4-09) builds its `shoutsByWorldNpcs` index from `const NpcShout*` into
 * the bound lists, visiting the groups forward and the lists and npc ids backwards like Java (upstream 15eaf77b6 removed makeNull and the list
 * clearing of NpcShoutData.afterUnmarshal).
 *
 * @author Rolandas
 */
class ShoutList : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/model/templates/npcshout/ShoutList.xml.inc"
public:
	/** Java: an empty list without shouts (List.of()); the C++ list always exists */
	const std::vector<NpcShout>& getNpcShouts() const { return npcShouts; }

	/** Java: an empty list without npc_ids (List.of()) */
	const std::vector<int32_t>& getNpcIds() const;
};

} // namespace aion::gameserver::model::templates::npcshout
