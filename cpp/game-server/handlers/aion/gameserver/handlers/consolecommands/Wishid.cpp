#include "aion/gameserver/handlers/consolecommands/Wishid.h"

#include "aion/commons/utils/Numbers.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/ItemData.h"
#include "aion/gameserver/services/AdminService.h"
#include "aion/gameserver/services/item/ItemService.h"

namespace aion::gameserver::handlers::consolecommands {

AION_CONSOLE_COMMAND(Wishid);

// Java Wishid.java:16-18
Wishid::Wishid() : ConsoleCommand("wishid") {
}

// Java Wishid.java:20-62
void Wishid::execute(Player& admin, std::span<const std::string> params) {
	if (params.size() < 2) {
		info(admin, std::nullopt); // parity= info(admin, null);
		return;
	}

	runtime::Ptr<Player> target(&admin); // parity= Player target = admin;
	if (runtime::Ptr<Player> player = runtime::as<Player>(admin.getTarget())) { // parity= if (admin.getTarget() instanceof Player player) {
		target = player;
	}

	int64_t itemCount;
	int32_t itemId;

	try {
		itemCount = commons::utils::parseLong(params[0]); // parity= itemCount = Long.parseLong(params[0]);
		itemId = commons::utils::parseInt(params[1]); // parity= itemId = Integer.parseInt(params[1]);
	} catch (const commons::utils::NumberFormatException&) { // parity= } catch (NumberFormatException e) {
		info(admin, std::nullopt); // parity= info(admin, null);
		return;
	}

	if (DataManager::ITEM_DATA->getItemTemplate(itemId) == nullptr) {
		PacketSendUtility::sendMessage(admin, "Item id is incorrect: " + std::to_string(itemId)); // parity= PacketSendUtility.sendMessage(admin, "Item id is incorrect: " + itemId);
		return;
	}

	if (!AdminService::getInstance().canOperate(admin, target, itemId, "command ///wishid"))
		return;

	int64_t count = ItemService::addItem(*target, itemId, itemCount, true); // parity= long count = ItemService.addItem(target, itemId, itemCount, true);

	if (count == 0) {
		if (&admin != target.get()) { // parity= if (admin != target) {
			PacketSendUtility::sendMessage(admin, "You successfully gave " + std::to_string(itemCount) + " x [item:" + std::to_string(itemId) + "] to " + target->getName() + "."); // parity= PacketSendUtility.sendMessage(admin, "You successfully gave " + itemCount + " x [item:" + itemId + "] to " + target.getName() + ".");
			PacketSendUtility::sendMessage(*target, "You successfully received " + std::to_string(itemCount) + " x [item:" + std::to_string(itemId) + "] from " + admin.getName() + "."); // parity= PacketSendUtility.sendMessage(target, "You successfully received " + itemCount + " x [item:" + itemId + "] from " + admin.getName() + ".");
		} else
			PacketSendUtility::sendMessage(admin, "You successfully received " + std::to_string(itemCount) + " x [item:" + std::to_string(itemId) + "]"); // parity= PacketSendUtility.sendMessage(admin, "You successfully received " + itemCount + " x [item:" + itemId + "]");
	} else {
		PacketSendUtility::sendMessage(admin, "Item couldn't be added");
	}
}

// Java Wishid.java:64-67
void Wishid::info(Player& admin, std::optional<std::string_view> /*message*/) {
	PacketSendUtility::sendMessage(admin, "syntax ///wishid <Quantity> <item Id>");
}

} // namespace aion::gameserver::handlers::consolecommands
