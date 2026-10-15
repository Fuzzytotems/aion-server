#include "aion/gameserver/handlers/ai/instance/dredgion/ShulackDrudgeAI.h"

#include "aion/gameserver/model/items/storage/Storage.h"
#include "aion/gameserver/network/aion/serverpackets/SM_DIALOG_WINDOW.h"
#include "aion/gameserver/services/item/ItemService.h"

namespace aion::gameserver::handlers::ai::instance::dredgion {

AION_AI(ShulackDrudgeAI, "shulackdrudge");

// Java ShulackDrudgeAI.java:26-29
void ShulackDrudgeAI::handleDialogFinish(Player& player) {
	addItems(player);
	GeneralNpcAI::handleDialogFinish(player);
}

// Java ShulackDrudgeAI.java:32-34
void ShulackDrudgeAI::handleDialogStart(Player& player) {
	PacketSendUtility::sendPacket(player, network::aion::serverpackets::SM_DIALOG_WINDOW(getObjectId(), 1011));
}

// Java ShulackDrudgeAI.java:36-43
void ShulackDrudgeAI::addItems(Player& player) {
	int32_t itemId = player.getRace() == Race::ELYOS ? 182212606 : 182212607;
	runtime::Ptr<Item> dredgionSupplies = player.getInventory().getFirstItemByItemId(itemId);
	if (dredgionSupplies == nullptr) {
		services::item::ItemService::addItem(player, itemId, 1);
		getOwner().overrideNpcType(CreatureType::PEACE);
	}
}

} // namespace aion::gameserver::handlers::ai::instance::dredgion
