#include "aion/gameserver/handlers/admincommands/Gag.h"

#include "aion/commons/utils/Numbers.h"
#include "aion/commons/utils/StringUtils.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/services/ban/ChatBanService.h"
#include "aion/gameserver/utils/Util.h"

namespace aion::gameserver::handlers::admincommands {

AION_ADMIN_COMMAND(Gag);

Gag::Gag()
	: AdminCommand("gag", "Bans a player from all chats.",
		  "<player> <duration> <reason> - Chat bans the player for the specified time in minutes.\n"
		  "<player> remove - Removes the chat ban of this player.\n") {
}

// Java Gag.java:26-62
void Gag::execute(Player& admin, std::span<const std::string> params) {
	if (params.size() < 2) {
		sendInfo(admin);
		return;
	}
	std::string playerName = Util::convertName(params[0]);
	runtime::Ptr<Player> player = World::getInstance().getPlayer(playerName);
	if (player == nullptr) {
		PacketSendUtility::sendPacket(admin, SM_SYSTEM_MESSAGE::STR_NO_SUCH_USER(playerName));
		return;
	}
	if (commons::utils::StringUtils::equalsIgnoreCase(params[1], "remove")) {
		if (ChatBanService::isBanned(*player)) {
			ChatBanService::unbanPlayer(*player);
			sendInfo(admin, "Unbanned " + name(*player) + " from all chats.");
		} else {
			sendInfo(admin, name(*player) + " can already chat.");
		}
	} else {
		int32_t durationMinutes = commons::utils::parseInt(params[1]); // Java: Integer.parseInt (a NumberFormatException is an IllegalArgumentException)
		if (durationMinutes < 1) {
			sendInfo(admin, "Duration must be at least 1 minute.");
			return;
		}
		std::string reason = join(params, 2);
		if (reason.empty()) {
			sendInfo(admin, "Reason must be specified.");
			return;
		}
		ChatBanService::banPlayer(*player, static_cast<int64_t>(durationMinutes) * 60000); // parity= ChatBanService.banPlayer(player, Duration.ofMinutes(durationMinutes).toMillis());
		PacketSendUtility::sendPacket(*player, SM_SYSTEM_MESSAGE::STR_INGAME_BLOCK_ENABLE_NO_CHAT(durationMinutes));
		sendInfo(*player, reason);
		sendInfo(admin, name(*player) + " is now gagged for " + std::to_string(durationMinutes) + " minute(s).");
	}
}

} // namespace aion::gameserver::handlers::admincommands
