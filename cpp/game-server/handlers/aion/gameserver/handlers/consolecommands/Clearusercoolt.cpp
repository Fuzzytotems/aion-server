#include "aion/gameserver/handlers/consolecommands/Clearusercoolt.h"

#include <vector>

#include "aion/gameserver/model/gameobjects/player/PortalCooldownList.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/utils/ChatUtil.h"

namespace aion::gameserver::handlers::consolecommands {

AION_CONSOLE_COMMAND(Clearusercoolt);

Clearusercoolt::Clearusercoolt()
	: ConsoleCommand("clearusercoolt", "Removes cooldowns for a player.", "<player> - Removes the instance cooldowns of the given player.\n") {
}

// Java Clearusercoolt.java:26-40
void Clearusercoolt::execute(Player& admin, std::span<const std::string> params) {
	if (params.empty()) {
		sendInfo(admin);
		return;
	}

	std::string playerName = utils::ChatUtil::getRealCharName(params[0], true);
	runtime::Ptr<Player> player = World::getInstance().getPlayer(playerName);
	if (player == nullptr) {
		PacketSendUtility::sendPacket(admin, network::aion::serverpackets::SM_SYSTEM_MESSAGE::STR_NO_SUCH_USER(playerName));
		return;
	}
	clearAllInstanceCooldowns(admin, *player);
}

// Java Clearusercoolt.java:42-59
void Clearusercoolt::clearAllInstanceCooldowns(Player& admin, Player& player) {
	if (player.getPortalCooldownList().getPortalCoolDowns() == nullptr) {
		PacketSendUtility::sendMessage(admin, (player.equals(admin) ? std::string("You have") : name(player) + " has") + " no instance cooldowns to remove.");
		return;
	}

	std::vector<int32_t> worldIds;
	for (const auto& entry : player.getPortalCooldownList().getPortalCoolDowns()->snapshot())
		worldIds.push_back(entry.getKey());
	player.getPortalCooldownList().setPortalCoolDowns(nullptr);
	for (int32_t worldId : worldIds)
		player.getPortalCooldownList().sendEntryInfo(worldId);

	if (player.equals(admin)) {
		PacketSendUtility::sendMessage(admin, "Your instance cooldowns were removed.");
	} else {
		PacketSendUtility::sendMessage(admin, "You have removed instance cooldowns of " + name(player) + '.');
		PacketSendUtility::sendMessage(player, name(admin) + " removed your instance cooldowns.");
	}
}

} // namespace aion::gameserver::handlers::consolecommands
