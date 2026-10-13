#include "aion/gameserver/handlers/admincommands/MoveToObject.h"

#include "aion/commons/utils/Exception.h"
#include "aion/commons/utils/Numbers.h"
#include "aion/gameserver/model/templates/spawns/SpawnTemplate.h"
#include "aion/gameserver/services/teleport/TeleportService.h"
#include "aion/gameserver/world/World.h"

namespace aion::gameserver::handlers::admincommands {

AION_ADMIN_COMMAND(MoveToObject);

MoveToObject::MoveToObject() : AdminCommand("movetoobj") {
}

// Java MoveToObject.java:19-43
void MoveToObject::execute(Player& admin, std::span<const std::string> params) {
	if (params.size() != 1) { // parity= if (params == null || params.length != 1) {
		PacketSendUtility::sendMessage(admin, "Syntax : //movetoobj <object id>");
		return;
	}

	int32_t objectId = 0;

	try {
		objectId = commons::utils::parseInt(params[0]); // parity= objectId = Integer.valueOf(params[0]);
	} catch (const commons::utils::NumberFormatException&) { // parity= } catch (NumberFormatException e) {
		PacketSendUtility::sendMessage(admin, "Only numbers please!!!");
		return; // correction J-S4CP6-1 (owner's decision 2026-10-13): Java goes on with object id 0
	}

	runtime::Ptr<VisibleObject> object = World::getInstance().findVisibleObject(objectId);
	if (object == nullptr) {
		PacketSendUtility::sendMessage(admin, "Cannot find object for spawn #" + std::to_string(objectId));
		return;
	}

	runtime::Ptr<VisibleObject> spawn = object;

	TeleportService::teleportTo(admin, spawn->getWorldId(), spawn->getSpawn()->getX(), spawn->getSpawn()->getY(), spawn->getSpawn()->getZ());
}

// Java MoveToObject.java:45-48
void MoveToObject::info(Player& player, std::optional<std::string_view> /*message*/) {
	PacketSendUtility::sendMessage(player, "Syntax : //movetoobj <object id>");
}

} // namespace aion::gameserver::handlers::admincommands
