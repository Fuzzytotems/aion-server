#include "aion/gameserver/handlers/playercommands/Faction.h"

#include <string>

#include "aion/gameserver/configs/main/CustomConfig.h"
#include "aion/gameserver/model/items/storage/Storage.h"
#include "aion/gameserver/network/aion/serverpackets/SM_MESSAGE.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/restrictions/PlayerRestrictions.h"
#include "aion/gameserver/services/player/PlayerChatService.h"

namespace aion::gameserver::handlers::playercommands {

AION_PLAYER_COMMAND(Faction);

/** Java's String.formatted of the syntax text, read when the command is constructed */
std::string Faction::formattedSyntaxInfo() { // parity: (the constructor's text below)
	const int32_t price = configs::main::CustomConfig::FACTION_USE_PRICE.load(); // parity: (the same)
	return "<message> - Sends the message to all players of your faction" + (price > 0 ? " for " + std::to_string(price) + " Kinah" : std::string()) + ".\n"; // parity: (the same)
} // parity: (the same)

Faction::Faction() : PlayerCommand("faction", "Faction chat.", formattedSyntaxInfo()) { // parity= Faction(); PlayerCommand("faction", "Faction chat.", "<message> - Sends the message to all players of your faction%s.\n".formatted(CustomConfig.FACTION_USE_PRICE > 0 ? " for " + CustomConfig.FACTION_USE_PRICE + " Kinah" : ""));
}

// Java Faction.java:30-67
void Faction::execute(Player& player, std::span<const std::string> params) {
	using configs::main::CustomConfig;
	if (!CustomConfig::FACTION_CMD_CHANNEL.load()) { // parity= if (!CustomConfig.FACTION_CMD_CHANNEL) {
		sendInfo(player, "The faction channel is disabled.");
		return;
	}

	if (params.size() < 1) { // parity= if (params == null || params.length < 1) {
		sendInfo(player);
		return;
	}

	if (!PlayerRestrictions::canChat(runtime::Ptr<Player>(player))) // parity= if (!PlayerRestrictions.canChat(player))
		return;

	if (CustomConfig::FACTION_USE_PRICE.load() > 0) { // parity= if (CustomConfig.FACTION_USE_PRICE > 0) {
		if (CustomConfig::FACTION_USE_PRICE.load() > player.getInventory().getKinah()) { // parity= if (CustomConfig.FACTION_USE_PRICE > player.getInventory().getKinah()) {
			PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_NOT_ENOUGH_MONEY());
			return;
		}
		player.getInventory().decreaseKinah(CustomConfig::FACTION_USE_PRICE.load()); // parity= player.getInventory().decreaseKinah(CustomConfig.FACTION_USE_PRICE);
	}

	std::string senderName = player.getName(player.isStaff());
	std::string message = CustomConfig::FACTION_CHAT_CHANNEL.load() ? join(params, 0) : senderName + ": " + join(params, 0); // parity= String message = CustomConfig.FACTION_CHAT_CHANNEL ? String.join(" ", params) : senderName + ": " + String.join(" ", params);
	ChatType channel = CustomConfig::FACTION_CHAT_CHANNEL.load() ? ChatType::CH1 : ChatType::BRIGHT_YELLOW; // parity= ChatType channel = CustomConfig.FACTION_CHAT_CHANNEL ? ChatType.CH1 : ChatType.BRIGHT_YELLOW;

	PlayerChatService::logMessage(player, ChatType::NORMAL, "[Faction Msg] " + message);

	World::getInstance().forEachPlayer([&player, &senderName, &message, channel](Player& listener) { // parity= World.getInstance().forEachPlayer(new Consumer<Player>() { @Override public void accept(Player listener) {
		// GMs can read both factions (but only write to their own)
		if (listener.getRace() == player.getRace() || listener.isStaff()) {
			std::string name = listener.isStaff() ? (player.getRace() == Race::ASMODIANS ? "(A) " : "(E) ") + senderName : senderName; // parity= String name = listener.isStaff() ? (player.getRace() == Race.ASMODIANS ? "(A) " : "(E) ") + senderName : senderName;
			PacketSendUtility::sendPacket(listener, SM_MESSAGE(player.getObjectId(), name, message, channel));
		}
	});
}

} // namespace aion::gameserver::handlers::playercommands
