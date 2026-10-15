#include "aion/gameserver/handlers/admincommands/AddSet.h"

#include <string>

#include "aion/commons/utils/Exception.h"
#include "aion/commons/utils/Numbers.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/ItemSetData.h"
#include "aion/gameserver/model/items/storage/Storage.h"
#include "aion/gameserver/model/templates/itemset/ItemPart.h"
#include "aion/gameserver/model/templates/itemset/ItemSetTemplate.h"
#include "aion/gameserver/services/item/ItemService.h"
#include "aion/gameserver/utils/Util.h"
#include "aion/gameserver/world/World.h"

namespace aion::gameserver::handlers::admincommands {

AION_ADMIN_COMMAND(AddSet);

AddSet::AddSet() : AdminCommand("addset") {
}

// Java AddSet.java:24-75
void AddSet::execute(Player& player, std::span<const std::string> params) {
	if (params.size() == 0 || params.size() > 2) { // parity= if (params.length == 0 || params.length > 2) {
		info(player, std::nullopt);
		return;
	}

	int32_t itemSetId = 0;
	runtime::Ptr<Player> receiver = nullptr;

	try {
		itemSetId = commons::utils::parseInt(params[0]);
		receiver = runtime::Ptr<Player>(player); // parity= receiver = player;
	} catch (const commons::utils::NumberFormatException&) { // parity= } catch (NumberFormatException e) {
		receiver = World::getInstance().getPlayer(Util::convertName(params[0]));
		if (receiver == nullptr) {
			PacketSendUtility::sendMessage(player, "Could not find a player by that name.");
			return;
		}

		try {
			if (params.size() < 2) // parity: Java's ArrayIndexOutOfBoundsException of params[1], caught by the catch (Exception ex2) below
				throw runtime::ArrayIndexOutOfBoundsException("Index 1 out of bounds for length 1"); // parity: (the same)
			itemSetId = commons::utils::parseInt(params[1]);
		} catch (const commons::utils::NumberFormatException&) { // parity= } catch (NumberFormatException ex) {
			PacketSendUtility::sendMessage(player, "You must give number to itemset ID.");
			return;
		} catch (const std::exception&) { // parity= } catch (Exception ex2) {
			PacketSendUtility::sendMessage(player, "Occurs an error.");
			return;
		}
	}

	const ItemSetTemplate* itemSet = DataManager::ITEM_SET_DATA->getItemSetTemplate(itemSetId);
	if (itemSet == nullptr) {
		PacketSendUtility::sendMessage(player, "ItemSet does not exist with id " + std::to_string(itemSetId));
		return;
	}

	if (receiver->getInventory().getFreeSlots() < static_cast<int32_t>(itemSet->getItempart().size())) { // parity= if (receiver.getInventory().getFreeSlots() < itemSet.getItempart().size()) {
		PacketSendUtility::sendMessage(player, "Inventory needs at least " + std::to_string(itemSet->getItempart().size()) + " free slots.");
		return;
	}

	for (const ItemPart& setPart : itemSet->getItempart()) {
		int64_t count = ItemService::addItem(*receiver, setPart.getItemId(), 1);
		if (count != 0) {
			PacketSendUtility::sendMessage(player, "Item " + std::to_string(setPart.getItemId()) + " couldn't be added");
			return;
		}
	}

	PacketSendUtility::sendMessage(player, "Item Set added successfully");
	PacketSendUtility::sendMessage(*receiver, "admin gives you an item set");
}

// Java AddSet.java:77-81
void AddSet::info(Player& player, std::optional<std::string_view> /*message*/) {
	PacketSendUtility::sendMessage(player, "syntax //addset <player> <itemset ID>");
	PacketSendUtility::sendMessage(player, "syntax //addset <itemset ID>");
}

} // namespace aion::gameserver::handlers::admincommands
