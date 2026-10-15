#include "aion/gameserver/handlers/playercommands/Del.h"

#include <cstdint>
#include <string>

#include "aion/commons/utils/Numbers.h"
#include "aion/gameserver/model/items/storage/Storage.h"
#include "aion/gameserver/utils/ChatUtil.h"

namespace aion::gameserver::handlers::playercommands {

AION_PLAYER_COMMAND(Del);

Del::Del()
	: PlayerCommand("del", "Deletes items from your inventory.",
		  "<item link|ID> [count] - Removes item(s) with the specified name/ID (default: 1, optional: number of items to delete).\n") {
}

// Java Del.java:21-50
void Del::execute(Player& player, std::span<const std::string> params) {
	if (params.empty()) {
		sendInfo(player);
		return;
	}

	int32_t itemId = ChatUtil::getItemId(params[0]);
	if (itemId == 0) {
		sendInfo(player, "Invalid item.");
		return;
	}

	int32_t itemCount = params.size() > 1 ? commons::utils::parseInt(params[1]) : 1; // parity= int itemCount = params.length > 1 ? Integer.parseInt(params[1]) : 1;
	if (itemCount <= 0) {
		sendInfo(player, "Invalid item count.");
		return;
	}

	Storage& inv = player.getInventory();
	int64_t invCount = inv.getItemCountByItemId(itemId);
	if (invCount == 0) {
		sendInfo(player, "You don't have that item.");
		return;
	}
	if (itemCount > invCount) {
		sendInfo(player, "You only have " + std::to_string(invCount) + ".");
		return;
	}

	inv.decreaseByItemId(itemId, itemCount);
	sendInfo(player, "Deleted " + std::to_string(itemCount) + "x " + ChatUtil::item(itemId) + " from your inventory.");
}

} // namespace aion::gameserver::handlers::playercommands
