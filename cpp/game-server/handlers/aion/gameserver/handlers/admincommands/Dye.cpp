#include "aion/gameserver/handlers/admincommands/Dye.h"

#include <cstdint>
#include <format>
#include <optional>
#include <string>
#include <vector>

#include "aion/commons/utils/Exception.h"
#include "aion/commons/utils/Numbers.h"
#include "aion/commons/utils/StringUtils.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/ItemData.h"
#include "aion/gameserver/model/gameobjects/Item.h"
#include "aion/gameserver/model/gameobjects/Persistable_PersistentState.h"
#include "aion/gameserver/model/gameobjects/player/Equipment.h"
#include "aion/gameserver/model/templates/item/ItemTemplate.h"
#include "aion/gameserver/model/templates/item/actions/DyeAction.h"
#include "aion/gameserver/model/templates/item/actions/ItemActions.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/network/aion/serverpackets/SM_UPDATE_PLAYER_APPEARANCE.h"
#include "aion/gameserver/services/item/ItemPacketService.h"
#include "aion/gameserver/utils/ChatUtil.h"
#include "aion/gameserver/utils/JavaColor.h"

namespace aion::gameserver::handlers::admincommands {

AION_ADMIN_COMMAND(Dye);

Dye::Dye()
	: AdminCommand("dye", "Dyes a player's visible equipment.",
		  "<color> - Dyes the selected player's equipment in the specified color (can be dye item link/ID, color name or color HEX code).\n"
		  "0 - Removes all dyes from the selected player's equipment.\n") {
}

// Java Dye.java:36-97. Java's reflective java.awt.Color field lookup is JavaColor::byName (std::nullopt where getField throws).
void Dye::execute(Player& player, std::span<const std::string> params) {
	if (params.empty()) {
		sendInfo(player);
		return;
	}

	runtime::Ptr<Player> p = runtime::as<Player>(player.getTarget()); // parity: the pattern variable of the instanceof below
	Player& target = p != nullptr ? *p : player; // parity= Player target = player.getTarget() instanceof Player p ? p : player;
	std::optional<int32_t> itemColor = std::nullopt; // null = default item color
	std::string colorText = "default";
	std::string colorParam = params[0];
	if (!commons::utils::StringUtils::equalsIgnoreCase("0", colorParam)) { // parity= if (!"0".equalsIgnoreCase(colorParam)) {
		// try to get itemId of a dyeing item
		itemColor = ChatUtil::getItemId(colorParam);
		const ItemTemplate* dyeItemTemplate = DataManager::ITEM_DATA->getItemTemplate(*itemColor);
		if (itemColor != 0 && dyeItemTemplate != nullptr && dyeItemTemplate->getActions() != nullptr && dyeItemTemplate->getActions()->getDyeAction() != nullptr) {
			itemColor = dyeItemTemplate->getActions()->getDyeAction()->getColor();
			colorText = ChatUtil::item(dyeItemTemplate->getTemplateId());
		} else {
			try {
				// try to get color by name
				std::optional<utils::JavaColor> named = utils::JavaColor::byName(commons::utils::StringUtils::toUpperCase(colorParam)); // parity= try { itemColor = ((Color) Class.forName("java.awt.Color").getField(colorParam.toUpperCase()).get(null)).getRGB();
				if (named) { // parity: the getField that did not throw
					itemColor = named->getRGB(); // parity: (the same statement)
				} else { // parity= } catch (Exception e) {
					// try to get color by hex code
					if (commons::utils::StringUtils::utf16Length(colorParam) <= 8) { // parity= if (colorParam.length() <= 8) {
						if (colorParam.starts_with("#")) // parity= if (colorParam.startsWith("#"))
							colorParam = commons::utils::StringUtils::substring(colorParam, 1);
						else if (colorParam.starts_with("0x") || colorParam.starts_with("0X")) // parity= else if (colorParam.startsWith("0x") || colorParam.startsWith("0X"))
							colorParam = commons::utils::StringUtils::substring(colorParam, 2);
					}
					itemColor = commons::utils::parseInt(colorParam, 16); // parity= itemColor = Integer.valueOf(colorParam, 16);
				}
				colorText = ChatUtil::color("#" + std::format("{:06X}", static_cast<uint32_t>(*itemColor & 0xFFFFFF)), *itemColor); // parity= colorText = ChatUtil.color("#" + String.format("%06X", itemColor & 0xFFFFFF), itemColor);
			} catch (const commons::utils::NumberFormatException&) { // parity= } catch (NumberFormatException _) {
				sendInfo(player, "Invalid color.");
				return;
			}
		}
	}

	std::vector<runtime::Ptr<Item>> appearanceItems = target.getEquipment().getEquippedForAppearance();
	if (appearanceItems.empty()) {
		PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_CHANGE_ITEM_SKIN_NO_TARGET_ITEM());
		return;
	}
	if (std::none_of(appearanceItems.begin(), appearanceItems.end(), [](const runtime::Ptr<Item>& item) { return item->getItemTemplate()->isItemDyePermitted(); })) { // parity= if (appearanceItems.stream().noneMatch(item -> item.getItemTemplate().isItemDyePermitted())) {
		PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_ITEM_COLOR_CHANGE_ERROR_CANNOTDYE(appearanceItems.front()->getL10n()));
		return;
	}
	for (const runtime::Ptr<Item>& item : appearanceItems) {
		if (item->getItemSkinTemplate()->isItemDyePermitted())
			item->setItemColor(itemColor);
		ItemPacketService::updateItemAfterInfoChange(target, *item);
	}
	PacketSendUtility::broadcastPacket(target, SM_UPDATE_PLAYER_APPEARANCE(target.getObjectId(), appearanceItems), true);
	target.getEquipment().setPersistentState(model::gameobjects::Persistable_PersistentState::UPDATE_REQUIRED); // parity= target.getEquipment().setPersistentState(PersistentState.UPDATE_REQUIRED);
	if (itemColor == std::nullopt)
		sendInfo(player, "Removed dyeing from " + name(target) + "'s visible equipment.");
	else
		sendInfo(player, "Dyed " + name(target) + " (color: " + colorText + ")");
	if (!target.equals(player))
		sendInfo(target, name(player) + " has changed the color of your visible equipment to: " + colorText);
}

} // namespace aion::gameserver::handlers::admincommands
