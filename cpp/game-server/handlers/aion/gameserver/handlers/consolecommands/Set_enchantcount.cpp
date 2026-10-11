#include "aion/gameserver/handlers/consolecommands/Set_enchantcount.h"

#include <algorithm>

#include "aion/commons/utils/Exception.h"
#include "aion/commons/utils/Numbers.h"
#include "aion/gameserver/model/enchants/EnchantEffect.h"
#include "aion/gameserver/model/gameobjects/Item.h"
#include "aion/gameserver/model/gameobjects/Persistable.h"
#include "aion/gameserver/model/gameobjects/player/Equipment.h"
#include "aion/gameserver/model/items/storage/Storage.h"
#include "aion/gameserver/model/stats/container/PlayerGameStats.h"
#include "aion/gameserver/model/templates/item/ItemTemplate.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/services/EnchantService.h"
#include "aion/gameserver/services/item/ItemPacketService.h"

namespace aion::gameserver::handlers::consolecommands {

AION_CONSOLE_COMMAND(Set_enchantcount);

Set_enchantcount::Set_enchantcount() : ConsoleCommand("set_enchantcount") {
}

// Java Set_enchantcount.java:22-77
void Set_enchantcount::execute(Player& admin, std::span<const std::string> params) {
	using model::gameobjects::Persistable;
	if (params.size() < 2) {
		info(admin, std::nullopt); // parity= info(admin, null);
		return;
	}

	int32_t objId;
	int32_t enchant;

	try {
		objId = commons::utils::parseInt(params[0]); // parity= objId = Integer.parseInt(params[0]);
		enchant = commons::utils::parseInt(params[1]); // parity= enchant = Integer.parseInt(params[1]);
	} catch (const commons::utils::NumberFormatException&) { // parity= } catch (NumberFormatException e) {
		PacketSendUtility::sendMessage(admin, "Invalid params.");
		return;
	}

	runtime::Ptr<Item> item = admin.getInventory().getItemByObjId(objId);
	if (item == nullptr)
		item = admin.getEquipment().getEquippedItemByObjId(objId);

	if (item != nullptr) {
		int32_t curEnchant = item->getEnchantLevel();
		int32_t maxEnchant = item->getItemTemplate()->getMaxEnchantLevel();
		maxEnchant += item->getItemTemplate()->getMaxEnchantBonus();

		if (curEnchant + enchant <= maxEnchant) {
			curEnchant += enchant;
			item->setEnchantLevel(std::min(curEnchant, maxEnchant)); // parity= item.setEnchantLevel(Math.min(curEnchant, maxEnchant));
			if (item->isEquipped())
				admin.getGameStats()->updateStatsVisually();

			services::item::ItemPacketService::updateItemAfterInfoChange(admin, *item, services::item::ItemPacketService::ItemUpdateType::STATS_CHANGE); // parity= ItemPacketService.updateItemAfterInfoChange(admin, item, ItemUpdateType.STATS_CHANGE);
			if (item->getEnchantEffect() != nullptr) {
				item->getEnchantEffect()->endEffect(admin);
				item->setEnchantEffect(nullptr);
			}

			if (item->getEnchantLevel() > 0 && item->isEquipped()) {
				EnchantService::applyEnchantEffect(*item, admin, item->getEnchantLevel());
			}
			if (item->isEquipped())
				admin.getEquipment().setPersistentState(Persistable::PersistentState::UPDATE_REQUIRED); // parity= admin.getEquipment().setPersistentState(PersistentState.UPDATE_REQUIRED);
			else
				admin.getInventory().setPersistentState(Persistable::PersistentState::UPDATE_REQUIRED); // parity= admin.getInventory().setPersistentState(PersistentState.UPDATE_REQUIRED);

			PacketSendUtility::sendPacket(admin, SM_SYSTEM_MESSAGE::STR_MSG_ENCHANT_ITEM_SUCCEED_NEW(item->getL10n(), enchant));
		} else {
			PacketSendUtility::sendMessage(admin, "Max Enchat for this item is: " + std::to_string(maxEnchant)); // parity= PacketSendUtility.sendMessage(admin, "Max Enchat for this item is: " + maxEnchant);
		}
	} else {
		PacketSendUtility::sendMessage(admin, "Invalid item.");
		return;
	}
}

// Java Set_enchantcount.java:79-82
void Set_enchantcount::info(Player& admin, std::optional<std::string_view> /*message*/) {
	PacketSendUtility::sendMessage(admin, "syntax ///set_enchantcount <item Id> <enchantcount> <unk>");
}

} // namespace aion::gameserver::handlers::consolecommands
