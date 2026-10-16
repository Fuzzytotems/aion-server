#include "aion/gameserver/network/aion/clientpackets/CM_GATHER.h"

#include <string>

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/gameserver/controllers/GatherableController.h"
#include "aion/gameserver/handlers/HandlerRegistry.h"
#include "aion/gameserver/model/gameobjects/Gatherable.h"
#include "aion/gameserver/model/gameobjects/VisibleObject.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/network/aion/AionConnection.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/skillengine/task/AbstractInteractionTask.h"
#include "aion/gameserver/skillengine/task/GatheringTask.h"
#include "aion/gameserver/utils/audit/AuditLogger.h"
#include "aion/gameserver/world/WorldPosition.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets {

static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.network.aion.clientpackets.CM_GATHER");

using model::gameobjects::Gatherable;
using model::gameobjects::VisibleObject;
using model::gameobjects::player::Player;

namespace {

// Java CM_GATHER.java:40-45
void startGathering(Player& player) {
	const runtime::Ptr<VisibleObject> target = player.getTarget();
	if (const runtime::Ptr<Gatherable> gatherable = runtime::as<Gatherable>(target))
		gatherable->getController().startGathering(player);
	else // Java `"..." + player.getTarget()`: String.valueOf, so no target reads "null"
		utils::audit::AuditLogger::log(player, "tried to gather from " + (target ? target->toString() : std::string("null")));
}

// Java CM_GATHER.java:47-50
void cancelGathering(Player& player) {
	if (const runtime::Ptr<skillengine::task::GatheringTask> gatheringTask = runtime::as<skillengine::task::GatheringTask>(player.getInteractionTask()))
		gatheringTask->abort();
}

} // namespace

CM_GATHER::CM_GATHER(int32_t opcode, const StateSet& validStates) : AionClientPacket(opcode, validStates) {
}

// Java CM_GATHER.java:26-28
void CM_GATHER::readImpl() {
	actionId = readD();
}

// Java CM_GATHER.java:31-38
void CM_GATHER::runImpl() {
	const runtime::Ptr<Player> player = getConnection()->getActivePlayer();
	switch (actionId) {
		case -1:
			cancelGathering(*player);
			break;
		case 0:
		case 128: // 128 is sent when using /attack chat command
			startGathering(*player);
			break;
		default:
			log.warn("Unhandled gathering action ID " + std::to_string(actionId) + " (sent by " + player->toString() + " at " +
				player->getPosition()->toString() + ")");
	}
}

AION_CLIENT_PACKET(CM_GATHER);

} // namespace aion::gameserver::network::aion::clientpackets
