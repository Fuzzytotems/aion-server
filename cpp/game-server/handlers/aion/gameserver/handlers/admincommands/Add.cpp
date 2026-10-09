#include "aion/gameserver/handlers/admincommands/Add.h"

#include <cstdint>
#include <string>

#include "aion/commons/utils/Numbers.h"
#include "aion/commons/utils/StringUtils.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/ItemData.h"
#include "aion/gameserver/model/items/ItemId.h"
#include "aion/gameserver/model/items/storage/Storage.h"
#include "aion/gameserver/model/templates/item/ItemTemplate.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/services/AdminService.h"
#include "aion/gameserver/services/item/ItemService.h"
#include "aion/gameserver/utils/ChatUtil.h"
#include "aion/gameserver/utils/Util.h"
#include "aion/gameserver/world/World.h"

namespace aion::gameserver::handlers::admincommands {

AION_ADMIN_COMMAND(Add);

Add::Add()
	: AdminCommand("add", "Adds Kinah or items to a player's inventory.",
		  "kinah <amount> - Adds the specified amount of Kinah to your inventory.\n"
		  "<item link|ID> [count] - Adds the specified item(s) to your inventory.\n"
		  "<player> kinah <amount> - Adds the specified amount of Kinah to the player's inventory.\n"
		  "<player> <item link|ID> [count] - Adds the specified item(s) to the player's inventory.\n") {
}

// Java Add.java:35-77
void Add::execute(Player& player, std::span<const std::string> params) {
	if (params.size() < 1) { // parity= if (params.length < 1) {
		sendInfo(player);
		return;
	}

	int32_t index = 0;
	runtime::Ptr<Player> receiver = runtime::Ptr<Player>(player); // parity= Player receiver = player;
	int32_t itemId = params.size() == 2 && commons::utils::StringUtils::equalsIgnoreCase("Kinah", params[index]) ? ItemId::KINAH : ChatUtil::getItemId(params[index]); // parity= int itemId = params.length == 2 && "Kinah".equalsIgnoreCase(params[index]) ? ItemId.KINAH : ChatUtil.getItemId(params[index]);
	if (itemId == 0) {
		std::string playerName = Util::convertName(params[index]);
		receiver = World::getInstance().getPlayer(playerName);
		if (receiver == nullptr) {
			PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_NO_SUCH_USER(playerName));
			return;
		}
		if (++index < static_cast<int32_t>(params.size())) // parity= if (++index < params.length)
			itemId = commons::utils::StringUtils::equalsIgnoreCase("Kinah", params[index]) ? ItemId::KINAH : ChatUtil::getItemId(params[index]); // parity= itemId = "Kinah".equalsIgnoreCase(params[index]) ? ItemId.KINAH : ChatUtil.getItemId(params[index]);
	}
	const ItemTemplate* itemTemplate = nullptr; // parity= ItemTemplate itemTemplate;
	if (itemId == 0 || (itemTemplate = DataManager::ITEM_DATA->getItemTemplate(itemId)) == nullptr) {
		sendInfo(player, "Invalid item.");
		return;
	}
	int64_t itemCount = static_cast<int32_t>(params.size()) > ++index ? commons::utils::parseLong(params[index]) : 1; // parity= long itemCount = params.length > ++index ? Long.parseLong(params[index]) : 1;
	if (itemCount <= 0
		|| (itemId == ItemId::KINAH ? static_cast<int64_t>(static_cast<uint64_t>(receiver->getInventory().getKinah()) + static_cast<uint64_t>(itemCount)) < 0 // parity= || (itemId == ItemId.KINAH ? receiver.getInventory().getKinah() + itemCount < 0 // Java's long overflow wraps
			: itemCount / itemTemplate->getMaxStackCount() > 126)) {
		sendInfo(player, "Invalid item count.");
		return;
	}
	if (!AdminService::getInstance().canOperate(player, receiver, itemId, "command //add"))
		return;

	int64_t notAddedCount = ItemService::addItem(*receiver, itemId, itemCount, true);
	if (notAddedCount == 0) {
		if (&player != receiver.get()) { // parity= if (player != receiver) {
			sendInfo(player, "You gave " + std::to_string(itemCount) + " x " + ChatUtil::item(itemId) + " to " + name(*receiver) + ".");
			sendInfo(*receiver, "You received " + std::to_string(itemCount) + " x " + ChatUtil::item(itemId) + " from " + name(player) + ".");
		}
	} else {
		sendInfo(player, "Item couldn't be added.");
	}
}

} // namespace aion::gameserver::handlers::admincommands
