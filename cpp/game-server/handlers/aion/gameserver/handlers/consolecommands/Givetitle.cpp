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

	// correction J-S4CP4-3 (owner's decision 2026-10-13): commands are keyed with their prefix; Java's "set" never matched
	if (!ChatProcessor::getInstance().isCommandAllowed(admin, "//set")) { // parity= if (!ChatProcessor.getInstance().isCommandAllowed(admin, "set")) {
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
	// correction J-S4CP4-2 (owner's decision 2026-10-13): "Added" only when the title was added, with a space before the name
	if (titleId <= 263 && setTitle(player, titleId)) // parity= if (titleId <= 263) setTitle(player, titleId);
		PacketSendUtility::sendMessage(admin, "Added " + std::to_string(titleId) + " to " + player.getCommonData()->getName()); // parity= PacketSendUtility.sendMessage(admin, "Added " + titleId + " to" + player.getCommonData().getName());
}

// Java Givetitle.java:44-47
void Givetitle::info(Player& admin, std::optional<std::string_view> /*message*/) {
	PacketSendUtility::sendMessage(admin, "syntax ///givetitle <title id>"); // parity= PacketSendUtility.sendMessage(admin, "syntax ///addskill <named name>");
}

// Java Givetitle.java:49-56
bool Givetitle::setTitle(Player& player, int32_t value) { // parity= private void setTitle(Player player, int value) {

	if (player.getTitleList().contains(value)) {
		PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_TOOLTIP_LEARNED_TITLE());
		return false; // parity= return;
	}
	return player.getTitleList().addTitle(value, false, 0); // parity= player.getTitleList().addTitle(value, false, 0);
}

} // namespace aion::gameserver::handlers::consolecommands
