#include "aion/gameserver/model/drop/NpcDrop.h"

#include "aion/gameserver/model/Race.h"
#include "aion/gameserver/model/drop/DropGroup.h"
#include "aion/gameserver/model/drop/DropItem.h"
#include "aion/gameserver/model/drop/DropModifiers.h"
#include "aion/gameserver/runtime/collections/HashSet.h"

namespace aion::gameserver::model::drop {

int32_t NpcDrop::dropCalculator(runtime::RcHashSet<runtime::Ref<DropItem>>& result, int32_t index, DropModifiers& dropModifiers,
	const std::vector<runtime::Ptr<gameobjects::player::Player>>& groupMembers) const {
	if (dropGroup.empty())
		return index;
	for (const DropGroup& dg : dropGroup) {
		if (dg.getRace() == Race::PC_ALL || dg.getRace() == dropModifiers.getDropRace()) {
			index = dg.tryAddDropItems(result, index, dropModifiers, groupMembers);
		}
	}
	return index;
}

} // namespace aion::gameserver::model::drop
