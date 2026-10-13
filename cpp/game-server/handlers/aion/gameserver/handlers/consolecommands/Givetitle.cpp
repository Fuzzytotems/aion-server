#include "aion/gameserver/handlers/consolecommands/Givetitle.h"

#include "aion/commons/utils/Numbers.h"
#include "aion/gameserver/model/gameobjects/player/PlayerCommonData.h"
#include "aion/gameserver/model/gameobjects/player/title/TitleList.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/utils/chathandlers/ChatProcessor.h"

namespace aion::gameserver::handlers::consolecommands {

AION_CONSOLE_COMMAND(Givetitle);

// Java Givetitle.java:14-16
Givetitle::Givetitle() : ConsoleCommand("givetitle") {
}

// Java Givetitle.java:18-42
void Givetitle::execute(Player& admin, std::span<const std::string> params) {
	if (params.size() < 1) {
		info(admin, std::nullopt); // parity= info(admin, null);
		return;
	}

	if (!ChatProcessor::getInstance().isCommandAllowed(admin, "set")) {
		PacketSendUtility::sendMessage(admin, "You dont have enough rights to execute this command");
		return;
	}

	runtime::Ptr<Player> target = runtime::as<Player>(admin.getTarget()); // parity= final Player player = admin.getTarget() instanceof Player target ? target : admin;
	Player& player = target != nullptr ? *target : admin; // parity: (the same)

	int32_t titleId;
	try {
		titleId = commons::utils::parseInt(params[0]); // parity= titleId = Integer.parseInt(params[0]);
	} catch (const commons::utils::NumberFormatException&) { // parity= } catch (NumberFormatException e) {
		PacketSendUtility::sendMessage(admin, "You should enter valid second params!");
		return;
	}
	if (titleId <= 263)
		setTitle(player, titleId);
	PacketSendUtility::sendMessage(admin, "Added " + std::to_string(titleId) + " to" + player.getCommonData()->getName()); // parity= PacketSendUtility.sendMessage(admin, "Added " + titleId + " to" + player.getCommonData().getName());
}

// Java Givetitle.java:44-47
void Givetitle::info(Player& admin, std::optional<std::string_view> /*message*/) {
	PacketSendUtility::sendMessage(admin, "syntax ///addskill <named name>");
}

// Java Givetitle.java:49-56
void Givetitle::setTitle(Player& player, int32_t value) {

	if (player.getTitleList().contains(value)) {
		PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_TOOLTIP_LEARNED_TITLE());
		return;
	}
	player.getTitleList().addTitle(value, false, 0);
}

} // namespace aion::gameserver::handlers::consolecommands
