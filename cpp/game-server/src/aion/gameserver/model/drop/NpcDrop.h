#pragma once

#include <cstdint>
#include <vector>

#include "aion/gameserver/model/drop/NpcDrop.xml.h"

#include "aion/gameserver/model/drop/fwd.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/runtime/collections/Rc.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"

namespace aion::gameserver::model::drop {

/**
 * Java com.aionemu.gameserver.model.drop.NpcDrop.
 * <p>
 * C++: the result set and the group members follow DropGroup::tryAddDropItems.
 *
 * @author MrPoke
 */
class NpcDrop : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/model/drop/NpcDrop.xml.inc"
public:
	int32_t dropCalculator(runtime::RcHashSet<runtime::Ref<DropItem>>& result, int32_t index, DropModifiers& dropModifiers,
		const std::vector<runtime::Ptr<gameobjects::player::Player>>& groupMembers) const;
};

} // namespace aion::gameserver::model::drop
