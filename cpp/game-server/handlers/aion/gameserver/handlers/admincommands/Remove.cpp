#include "aion/gameserver/handlers/admincommands/Remove.h"

#include <cstdint>
#include <regex>
#include <string>

#include "aion/commons/utils/Exception.h"
#include "aion/commons/utils/Numbers.h"
#include "aion/gameserver/model/items/storage/Storage.h"
#include "aion/gameserver/utils/Util.h"
#include "aion/gameserver/world/World.h"

namespace aion::gameserver::handlers::admincommands {

AION_ADMIN_COMMAND(Remove);

Remove::Remove() : AdminCommand("remove") {
}

// Java Remove.java:22-83
void Remove::execute(Player& admin, std::span<const std::string> params) {
	if (params.size() < 2) { // parity= if (params.length < 2) {
		info(admin, std::nullopt);
		return;
	}

	int32_t itemId = 0;
	int64_t itemCount = 1;
	int8_t itemCountIndex = 2;
	runtime::Ptr<Player> target = World::getInstance().getPlayer(Util::convertName(params[0]));
	if (target == nullptr) {
		info(admin, "Player isn't online.");
		return;
	}

	std::string itemString = params[1];
	if (itemString == "[item:" && params.size() > 2) { // parity= if (itemString.equals("[item:") && params.length > 2) {
		// some item links have space before their ID
		itemString += params[2];
		if (params.size() > 3) { // parity= if (params.length > 3) {
			itemCountIndex = 3;
		}
	}

	try {
		if (params.size() > 2 && (itemCountIndex < static_cast<int32_t>(params.size()))) { // parity= if (params.length > 2 && (itemCountIndex < params.length)) {
			// count parameter was passed
			itemCount = commons::utils::parseLong(params[itemCountIndex]);
		}
		static const std::regex id(R"((?:\[item:)??(\d{9}))"); // parity= Pattern id = Pattern.compile("(?:\\[item:)??(\\d{9})");
		std::smatch result; // parity= Matcher result = id.matcher(itemString);
		if (std::regex_search(itemString, result, id)) { // parity= if (result.find()) {
			itemId = commons::utils::parseInt(result[1].str()); // parity= itemId = Integer.parseInt(result.group(1));
		}
	} catch (const commons::utils::NumberFormatException&) { // parity= } catch (NumberFormatException e) {
		info(admin, "Invalid number parameter passed.");
		return;
	}
	if (itemId > 0) {
		if (itemCount > 0) {
			Storage& bag = target->getInventory();

			int64_t bagItemCount = bag.getItemCountByItemId(itemId);
			if (bagItemCount >= 1) {
				if (itemCount <= bagItemCount) {
					bag.decreaseByItemId(itemId, itemCount);
					PacketSendUtility::sendMessage(admin, "Successfully removed " + std::to_string(itemCount) + "x [item:" + std::to_string(itemId) + "] from " + target->getName()
						+ "'s inventory.");
					PacketSendUtility::sendMessage(*target, "Admin removed " + std::to_string(itemCount) + "x [item:" + std::to_string(itemId) + "] from your inventory.");
				} else {
					info(admin, "Player only has " + std::to_string(bagItemCount) + " of this item.");
				}
			} else {
				info(admin, "Player doesn't have that item.");
			}
		} else {
			info(admin, "Invalid item count.");
		}
	} else {
		info(admin, "Invalid item ID.");
	}
}

// Java Remove.java:85-91
void Remove::info(Player& player, std::optional<std::string_view> message) {
	if (message != std::nullopt && !message->empty()) { // parity= if (message != null && !message.isEmpty()) {
		PacketSendUtility::sendMessage(player, *message);
	}
	PacketSendUtility::sendMessage(player, "Syntax: //remove <player> <item ID|item @link> [quantity]");
}

} // namespace aion::gameserver::handlers::admincommands
