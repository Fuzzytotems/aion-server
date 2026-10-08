#include "aion/gameserver/handlers/admincommands/Say.h"

#include "aion/gameserver/model/gameobjects/Npc.h"
#include "aion/gameserver/network/aion/serverpackets/SM_MESSAGE.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"

namespace aion::gameserver::handlers::admincommands {

AION_ADMIN_COMMAND(Say);

Say::Say() : AdminCommand("say", "Lets your target say a message.", "<message> - Sends the message as your target (NPC only).\n") {
}

// Java Say.java:22-35
void Say::execute(Player& admin, std::span<const std::string> params) {
	if (params.empty()) {
		sendInfo(admin);
		return;
	}

	runtime::Ptr<Npc> npc = runtime::as<Npc>(admin.getTarget()); // Java: admin.getTarget() instanceof Npc npc
	if (npc == nullptr) {
		PacketSendUtility::sendPacket(admin, SM_SYSTEM_MESSAGE::STR_INVALID_TARGET());
		return;
	}

	PacketSendUtility::broadcastPacket(admin, SM_MESSAGE(*npc, join(params, 0), ChatType::NORMAL), true); // parity= PacketSendUtility.broadcastPacket(admin, new SM_MESSAGE(npc, String.join(" ", params), ChatType.NORMAL), true);
}

} // namespace aion::gameserver::handlers::admincommands
