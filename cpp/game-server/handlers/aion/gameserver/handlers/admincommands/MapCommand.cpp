#include "aion/gameserver/handlers/admincommands/MapCommand.h"

#include <algorithm>
#include <vector>

#include "aion/commons/utils/StringUtils.h"
#include "aion/gameserver/ai/AbstractAI.h"
#include "aion/gameserver/world/WorldMapInstance.h"
#include "aion/gameserver/world/WorldPosition.h"

namespace aion::gameserver::handlers::admincommands {

AION_ADMIN_COMMAND(MapCommand);

// Java MapCommand.java:16-21
MapCommand::MapCommand()
	: AdminCommand("map", "Offers different functions for the current map instance.",
		  "freeze - Freezes all NPCs on this map instance.\n"
		  "unfreeze - Unfreezes all NPCs on this map instance.\n") {
}

// Java MapCommand.java:23-40
void MapCommand::execute(Player& admin, std::span<const std::string> params) {
	if (params.empty()) { // parity= if (params.length == 0) {
		sendInfo(admin);
		return;
	}

	if (commons::utils::StringUtils::equalsIgnoreCase("freeze", params[0])) { // parity= if ("freeze".equalsIgnoreCase(params[0])) {
		std::vector<runtime::Ptr<Npc>> npcs = admin.getPosition()->getWorldMapInstance()->getNpcs();
		for (const runtime::Ptr<Npc>& npc : npcs) // parity= npcs.forEach(npc -> npc.getAi().onGeneralEvent(AIEventType.FREEZE));
			npc->getAi().onGeneralEvent(AIEventType::FREEZE); // parity: (the forEach lambda, above)
		sendInfo(admin, "World map is frozen!");
		int64_t walkerCount = std::ranges::count_if(npcs, [](const runtime::Ptr<Npc>& o) { return o->getAi().getState() == AIState::WALKING; }); // parity= long walkerCount = npcs.stream().filter(o -> o.getAi().getState() == AIState.WALKING).count();
		sendInfo(admin, "There are " + std::to_string(walkerCount) + " walkers remaining on map " +
			std::to_string(admin.getPosition()->getWorldMapInstance()->getMapId()));
	} else if (commons::utils::StringUtils::equalsIgnoreCase("unfreeze", params[0])) { // parity= } else if ("unfreeze".equalsIgnoreCase(params[0])) {
		admin.getPosition()->getWorldMapInstance()->forEachNpc([](Npc& npc) { npc.getAi().onGeneralEvent(AIEventType::UNFREEZE); });
		sendInfo(admin, "World map is unfrozen!");
	}
}

} // namespace aion::gameserver::handlers::admincommands
