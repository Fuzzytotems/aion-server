#include "aion/gameserver/dataholders/StaticDoorData.h"

#include <string>

#include "aion/gameserver/dataholders/loadingutils/LoadContext.h"

namespace aion::gameserver::dataholders {

using model::templates::staticdoor::StaticDoorTemplate;
using model::templates::staticdoor::StaticDoorWorld;

void StaticDoorData::afterUnmarshal(xml::LoadContext& ctx, const xml::XmlParent& /*parent*/) {
	for (const StaticDoorWorld& world : staticDoorWorlds) {
		if (!doorWorlds.try_emplace(world.getWorldId(), &world).second)
			ctx.fail("Duplicate static door world " + std::to_string(world.getWorldId()));
	}
	// Java: staticDoorWorlds = null (the C++ index points into the storage, which stays)
}

int32_t StaticDoorData::size() const {
	return static_cast<int32_t>(doorWorlds.size());
}

const std::vector<const StaticDoorTemplate*>& StaticDoorData::getStaticDoors(int32_t worldId) const {
	static const std::vector<const StaticDoorTemplate*> EMPTY;
	auto it = doorWorlds.find(worldId);
	return it == doorWorlds.end() ? EMPTY : it->second->getStaticDoors();
}

const StaticDoorTemplate* StaticDoorData::getStaticDoor(int32_t worldId, int32_t staticId) const {
	auto it = doorWorlds.find(worldId);
	return it == doorWorlds.end() ? nullptr : it->second->getStaticDoor(staticId);
}

} // namespace aion::gameserver::dataholders
