#include "aion/gameserver/handlers/ai/PlatinumFountainAI.h"

#include "aion/commons/utils/Rnd.h"
#include "aion/gameserver/model/items/storage/Storage.h"
#include "aion/gameserver/services/item/ItemService.h"

namespace aion::gameserver::handlers::ai {

AION_AI(PlatinumFountainAI, "fountain");

// Java PlatinumFountainAI.java:22-29
void PlatinumFountainAI::handleDialogStart(Player& player) {
	if (player.getInventory().getItemCountByItemId(186000030) > 0) {
		ActionItemNpcAI::handleDialogStart(player);
		PacketSendUtility::sendMessage(player, "Du forderst dein Gl\u00fcck heraus und wirfst eine Goldmedaille in den Brunnen!");
	} else
		PacketSendUtility::sendMessage(player, "Du hast leider keine Goldmedaillen bei dir, die du in den Brunnen werfen k\u00f6nntest.");
}

// Java PlatinumFountainAI.java:31-42
void PlatinumFountainAI::handleUseItemFinish(Player& player) {
	if (!player.getInventory().decreaseByItemId(186000030, 1))
		return;
	if (commons::utils::Rnd::chance() < 10) {
		ItemService::addItem(player, 186000096, 1);
		PacketSendUtility::sendMessage(player, "Du hattest Gl\u00fcck! Eine Medaille aus reinem Platin springt dir entgegen!");
	} else {
		ItemService::addItem(player, 182005205, 1);
		PacketSendUtility::sendMessage(player, "Du findest leider nur eine alte, verrostete Medaille. Vielleicht hast du beim n\u00e4chsten Mal mehr Gl\u00fcck!");
	}
}

} // namespace aion::gameserver::handlers::ai
