#include "aion/gameserver/model/templates/teleport/TeleporterTemplate.h"

#include <algorithm>
#include <string>

#include "aion/gameserver/runtime/base/Exceptions.h"

namespace aion::gameserver::model::templates::teleport {

bool TeleporterTemplate::containNpc(int32_t npcId) const {
	if (!npcIds)
		throw runtime::NullPointerException("TeleporterTemplate " + std::to_string(teleportId) + " has no npc ids");
	return std::ranges::find(*npcIds, npcId) != npcIds->end();
}

} // namespace aion::gameserver::model::templates::teleport
