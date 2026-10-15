#include "aion/gameserver/handlers/admincommands/Equip.h"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <string>
#include <vector>

#include "aion/commons/utils/Numbers.h"
#include "aion/commons/utils/StringUtils.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/ItemData.h"
#include "aion/gameserver/model/gameobjects/Item.h"
#include "aion/gameserver/model/gameobjects/Persistable_PersistentState.h"
#include "aion/gameserver/model/gameobjects/player/Equipment.h"
#include "aion/gameserver/model/items/ManaStone.h"
#include "aion/gameserver/model/stats/container/PlayerGameStats.h"
#include "aion/gameserver/model/stats/listeners/ItemEquipmentListener.h"
#include "aion/gameserver/model/templates/item/ItemTemplate.h"
#include "aion/gameserver/model/templates/item/actions/TamperingAction.h"
#include "aion/gameserver/services/EnchantService.h"
#include "aion/gameserver/services/item/ItemPacketService.h"
#include "aion/gameserver/services/item/ItemSocketService.h"
#include "aion/gameserver/utils/ChatUtil.h"
#include "aion/gameserver/utils/Util.h"
#include "aion/gameserver/world/World.h"

namespace aion::gameserver::handlers::admincommands {

AION_ADMIN_COMMAND(Equip);

Equip::Equip()
	: AdminCommand("equip", "Enchants all equipped items.",
		  "socket <manastone link|ID> [limit] [player] - Sockets the manastone in all equipped items of your target or the given player.\n"
		  "unsocket [player] - Removes manastones from all equipped items of your target or the given player.\n"
		  "enchant <0-255> [player] - Enchants all equipped items of your target or the given player.\n"
		  "temper <0-255> [player] - Tempers all equipped items of your target or the given player.\n") {
}

// Java Equip.java:42-67
void Equip::execute(Player& admin, std::span<const std::string> params) {
	if (params.empty()) {
		sendInfo(admin);
		return;
	}
	runtime::Ptr<Player> player = World::getInstance().getPlayer(Util::convertName(params[params.size() - 1])); // parity= Player player = World.getInstance().getPlayer(Util.convertName(params[params.length - 1]));
	if (player == nullptr) {
		runtime::Ptr<Player> target = runtime::as<Player>(admin.getTarget()); // parity: the pattern variable of the instanceof below
		player = target != nullptr ? target : runtime::Ptr<Player>(admin); // parity= player = admin.getTarget() instanceof Player target ? target : admin;
	}
	if (commons::utils::StringUtils::equalsIgnoreCase("socket", params[0]) && params.size() >= 2) { // parity= if ("socket".equalsIgnoreCase(params[0]) && params.length >= 2) {
		const ItemTemplate* manastone = DataManager::ITEM_DATA->getItemTemplate(ChatUtil::getItemId(params[1]));
		int32_t count = params.size() < 3 ? std::numeric_limits<int32_t>::max() : commons::utils::parseInt(params[2]); // parity= int count = params.length < 3 ? Integer.MAX_VALUE : Integer.parseInt(params[2]);
		socket(admin, *player, manastone, count);
	} else if (commons::utils::StringUtils::equalsIgnoreCase("unsocket", params[0])) { // parity= } else if ("unsocket".equalsIgnoreCase(params[0])) {
		unsocket(admin, *player);
	} else if (commons::utils::StringUtils::equalsIgnoreCase("enchant", params[0]) && params.size() >= 2) { // parity= } else if ("enchant".equalsIgnoreCase(params[0]) && params.length >= 2) {
		int32_t enchant = std::clamp(commons::utils::parseInt(params[1]), 0, 255); // parity= int enchant = Math.clamp(Integer.parseInt(params[1]), 0, 255);
		this->enchant(admin, *player, enchant);
	} else if (commons::utils::StringUtils::equalsIgnoreCase("temper", params[0]) && params.size() >= 2) { // parity= } else if ("temper".equalsIgnoreCase(params[0]) && params.length >= 2) {
		int32_t temperingLevel = std::clamp(commons::utils::parseInt(params[1]), 0, 255); // parity= int temperingLevel = Math.clamp(Integer.parseInt(params[1]), 0, 255);
		temper(admin, *player, temperingLevel);
	} else {
		sendInfo(admin);
	}
}

// Java Equip.java:69-104
void Equip::socket(Player& admin, Player& player, const ItemTemplate* manastone, int32_t count) {
	if (count <= 0) {
		sendInfo(admin, "Count must be greater than 0.");
		return;
	}
	if (manastone == nullptr || manastone->getItemGroup() != ItemGroup::MANASTONE && manastone->getItemGroup() != ItemGroup::SPECIAL_MANASTONE) {
		sendInfo(admin, "Invalid manastone.");
		return;
	}
	int32_t manastoneId = manastone->getTemplateId();
	int32_t maxSocketed = 0;
	for (const runtime::Ptr<Item>& targetItem : player.getEquipment().getEquippedItemsWithoutStigma()) {
		if (targetItem->getSockets(false) == 0)
			continue;
		for (int32_t counter = 0; counter < count;) {
			runtime::Ptr<ManaStone> manaStone = ItemSocketService::addManaStone(targetItem, manastoneId, false);
			if (manaStone == nullptr)
				break;
			maxSocketed = std::max(maxSocketed, ++counter); // parity= maxSocketed = Math.max(maxSocketed, ++counter);
			ItemEquipmentListener::addStoneStats(*targetItem, manaStone, *player.getGameStats());
		}
		if (maxSocketed > 0) {
			ItemPacketService::updateItemAfterInfoChange(player, *targetItem);
			targetItem->setPersistentState(model::gameobjects::Persistable_PersistentState::UPDATE_REQUIRED); // parity= targetItem.setPersistentState(PersistentState.UPDATE_REQUIRED);
		}
	}
	player.getGameStats()->updateStatsVisually();
	if (maxSocketed == 0)
		sendInfo(admin, "There are no free slots on any equipped items.");
	else if (&player == &admin) // parity= else if (player == admin)
		sendInfo(player, std::to_string(maxSocketed) + "x " + ChatUtil::item(manastoneId) + " were added to free slots on all equipped items.");
	else {
		sendInfo(admin, std::to_string(maxSocketed) + "x " + ChatUtil::item(manastoneId) + " were added to free slots on all equipped items of " + name(player) + ".");
		sendInfo(player, name(admin) + " added " + std::to_string(count) + "x " + ChatUtil::item(manastoneId) + " to free slots on all your equipped items.");
	}
}

// Java Equip.java:106-122
void Equip::unsocket(Player& admin, Player& player) {
	for (const runtime::Ptr<Item>& targetItem : player.getEquipment().getEquippedItemsWithoutStigma()) {
		if (targetItem->getItemStonesSize() > 0) {
			ItemEquipmentListener::removeStoneStats(targetItem->getItemStones()->snapshot(), *player.getGameStats()); // parity= ItemEquipmentListener.removeStoneStats(targetItem.getItemStones(), player.getGameStats());
			ItemSocketService::removeAllManastone(player, targetItem);
			ItemPacketService::updateItemAfterInfoChange(player, *targetItem);
			targetItem->setPersistentState(model::gameobjects::Persistable_PersistentState::UPDATE_REQUIRED); // parity= targetItem.setPersistentState(PersistentState.UPDATE_REQUIRED);
		}
	}
	player.getGameStats()->updateStatsVisually();
	if (&player == &admin) // parity= if (player == admin)
		sendInfo(player, "Removed manastones from all equipped items.");
	else {
		sendInfo(admin, "Removed manastones from all equipped items of " + name(player) + ".");
		sendInfo(player, name(admin) + " removed all manastones from all your equipped items.");
	}
}

// Java Equip.java:124-141
void Equip::enchant(Player& admin, Player& player, int32_t enchant) {
	for (const runtime::Ptr<Item>& targetItem : player.getEquipment().getEquippedItemsWithoutStigma()) {
		if (targetItem->getItemTemplate()->isNoEnchant())
			continue;
		if (targetItem->getItemTemplate()->getMaxEnchantLevel() == 0 && !targetItem->getItemTemplate()->canExceedEnchant())
			continue;
		targetItem->setAmplified(enchant > targetItem->getItemTemplate()->getMaxEnchantLevel() + targetItem->getEnchantBonus());
		EnchantService::setEnchantLevel(player, *targetItem, enchant);
	}
	player.getEquipment().setPersistentState(model::gameobjects::Persistable_PersistentState::UPDATE_REQUIRED); // parity= player.getEquipment().setPersistentState(PersistentState.UPDATE_REQUIRED);
	if (&player == &admin) // parity= if (player == admin)
		sendInfo(player, "Enchanted all equipped items to +" + std::to_string(enchant) + ".");
	else {
		sendInfo(admin, "Enchanted all equipped items of " + name(player) + " to +" + std::to_string(enchant) + ".");
		sendInfo(player, name(admin) + " enchanted all your equipped items to +" + std::to_string(enchant) + ".");
	}
}

// Java Equip.java:143-155
void Equip::temper(Player& admin, Player& player, int32_t temperingLevel) {
	for (const runtime::Ptr<Item>& targetItem : player.getEquipment().getEquippedItemsWithoutStigma()) {
		if (targetItem->getItemTemplate()->getMaxTampering() > 0)
			TamperingAction::setTemperingLevel(*targetItem, player, std::min(temperingLevel, targetItem->getItemTemplate()->getMaxTampering())); // parity= TamperingAction.setTemperingLevel(targetItem, player, Math.min(temperingLevel, targetItem.getItemTemplate().getMaxTampering()));
	}
	player.getEquipment().setPersistentState(model::gameobjects::Persistable_PersistentState::UPDATE_REQUIRED); // parity= player.getEquipment().setPersistentState(PersistentState.UPDATE_REQUIRED);
	if (&player == &admin) // parity= if (player == admin)
		sendInfo(player, "Tempered all equipped items to +" + std::to_string(temperingLevel) + ".");
	else {
		sendInfo(admin, "Tempered all equipped items of " + name(player) + " to +" + std::to_string(temperingLevel) + ".");
		sendInfo(player, name(admin) + " tempered all your equipped items to +" + std::to_string(temperingLevel) + ".");
	}
}

} // namespace aion::gameserver::handlers::admincommands
