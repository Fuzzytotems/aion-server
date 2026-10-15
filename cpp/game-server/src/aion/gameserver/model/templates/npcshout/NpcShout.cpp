#include "aion/gameserver/model/templates/npcshout/NpcShout.h"

#include "aion/gameserver/model/gameobjects/Npc.h"
#include "aion/gameserver/model/templates/npc/NpcTemplate.h"

namespace aion::gameserver::model::templates::npcshout {

int32_t NpcShout::getShoutRange(gameobjects::Npc& npc) const {
	return npc.getObjectTemplate()->getMinimumShoutRange();
}

} // namespace aion::gameserver::model::templates::npcshout
