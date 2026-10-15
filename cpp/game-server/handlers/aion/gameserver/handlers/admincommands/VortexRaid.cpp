#include "aion/gameserver/handlers/admincommands/VortexRaid.h"

#include <string>

#include "aion/commons/utils/StringUtils.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/VortexData.h"
#include "aion/gameserver/model/vortex/VortexLocation.h"
#include "aion/gameserver/services/VortexService.h"
#include "aion/gameserver/world/WorldMapTypeInfo.h"

namespace aion::gameserver::handlers::admincommands {

AION_ADMIN_COMMAND(VortexRaid);

VortexRaid::VortexRaid()
	: AdminCommand("vortexraid", "Starts/stops a raid in Theobomos or Brusthonin.", R"(start <Theobomos|Brusthonin> - Starts the raid at the given location.
stop <Theobomos|Brusthonin> - Stops the raid at the given location.
)") {
}

// Java VortexRaid.java:20-49
void VortexRaid::execute(Player& player, std::span<const std::string> params) {
	namespace StringUtils = commons::utils::StringUtils;
	if (params.size() < 2) {
		sendInfo(player);
		return;
	}

	const int32_t mapId = world::getWorldMapTypeMapId(params[1]);
	runtime::Ptr<model::vortex::VortexLocation> loc = dataholders::DataManager::VORTEX_DATA->getVortexLocation(mapId);
	if (!loc) {
		sendInfo(player, "Invalid location.");
		return;
	}
	if (StringUtils::equalsIgnoreCase("start", params[0])) {
		if (services::VortexService::getInstance().isInvasionInProgress(loc->getId())) {
			sendInfo(player, worldName(mapId) + " is already under siege.");
		} else {
			sendInfo(player, worldName(mapId) + " raid started.");
			services::VortexService::getInstance().startInvasion(loc->getId());
		}
	} else if (StringUtils::equalsIgnoreCase("stop", params[0])) {
		if (!services::VortexService::getInstance().isInvasionInProgress(loc->getId())) {
			sendInfo(player, worldName(mapId) + " is not under siege.");
		} else {
			sendInfo(player, worldName(mapId) + " raid stopped.");
			services::VortexService::getInstance().stopInvasion(loc->getId());
		}
	} else {
		sendInfo(player);
	}
}

} // namespace aion::gameserver::handlers::admincommands
