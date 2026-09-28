#include "aion/gameserver/model/templates/event/BuffMapTypeInfo.h"

#include "aion/gameserver/model/templates/world/WorldMapTemplate.h"
#include "aion/gameserver/world/WorldMapInstance.h"

namespace aion::gameserver::model::templates::event {

bool matches(Buff_BuffMapType type, ::aion::gameserver::world::WorldMapInstance& worldMapInstance) {
	switch (type) {
		case Buff_BuffMapType::WORLD_MAP:
			return !worldMapInstance.getTemplate()->isInstance();
		case Buff_BuffMapType::SOLO_INSTANCE:
			return worldMapInstance.getMaxPlayers() == 1;
		case Buff_BuffMapType::GROUP_INSTANCE:
			return worldMapInstance.getMaxPlayers() > 1 && worldMapInstance.getMaxPlayers() <= 6;
		case Buff_BuffMapType::ALLIANCE_INSTANCE:
			return worldMapInstance.getMaxPlayers() > 6 && worldMapInstance.getMaxPlayers() <= 24;
	}
	return false;
}

} // namespace aion::gameserver::model::templates::event
