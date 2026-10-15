#include "aion/gameserver/handlers/consolecommands/Delete_items.h"

#include "aion/commons/utils/Numbers.h"
#include "aion/gameserver/dataholders/loadingutils/EnumTraits.h"
#include "aion/gameserver/model/gameobjects/Item.h"
#include "aion/gameserver/model/items/storage/Storage.h"
#include "aion/gameserver/model/templates/item/ItemQualityInfo.h"
#include "aion/gameserver/model/templates/item/ItemTemplate.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/services/item/ItemPacketService.h"

namespace aion::gameserver::handlers::consolecommands {

AION_CONSOLE_COMMAND(Delete_items);

// Java Delete_items.java:15-17
Delete_items::Delete_items() : ConsoleCommand("delete_items") {
}

// Java Delete_items.java:19-47
void Delete_items::execute(Player& admin, std::span<const std::string> params) {
	if (params.size() < 1) {
		info(admin, std::nullopt); // parity= info(admin, null);
		return;
	}

	runtime::Ptr<Player> target = runtime::as<Player>(admin.getTarget()); // parity= final Player player = admin.getTarget() instanceof Player target ? target : admin;
	Player& player = target != nullptr ? *target : admin; // parity: (the same)

	int32_t quality;

	try {
		quality = commons::utils::parseInt(params[0]); // parity= quality = Integer.parseInt(params[0]);
	} catch (const commons::utils::NumberFormatException&) { // parity= } catch (NumberFormatException e) {
		PacketSendUtility::sendMessage(admin, "Parameters need to be an integer.");
		return;
	}

	if (quality < 0 || quality >= static_cast<int32_t>(xml::EnumTraits<ItemQuality>::names.size())) { // parity= if (quality < 0 || quality >= ItemQuality.values().length) {
		PacketSendUtility::sendMessage(admin, "Invalid QualityId.");
		return;
	}

	for (const runtime::Ptr<Item>& item : player.getInventory().getItems()) { // parity= for (Item item : player.getInventory().getItems()) {
		std::optional<ItemQuality> itemQuality = item->getItemTemplate()->getItemQuality(); // parity= if (item.getItemTemplate().getItemQuality().getQualityId() <= quality) {
		if (!itemQuality) // parity: Java's NullPointerException of a template without quality, explicit
			throw runtime::NullPointerException("ItemTemplate.getItemQuality()"); // parity: (the same)
		if (getQualityId(*itemQuality) <= quality) { // parity: (the same)
			player.getInventory().delete_(*item, ItemPacketService::ItemDeleteType::DISCARD); // parity= player.getInventory().delete(item, ItemDeleteType.DISCARD);
		}
	}
}

// Java Delete_items.java:49-52
void Delete_items::info(Player& admin, std::optional<std::string_view> /*message*/) {
	PacketSendUtility::sendMessage(admin, "syntax ///delete_items <item quality>");
}

} // namespace aion::gameserver::handlers::consolecommands
