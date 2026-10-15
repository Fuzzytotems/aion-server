#include "aion/gameserver/network/aion/clientpackets/CM_TUNE_RESULT.h"

#include "aion/gameserver/handlers/HandlerRegistry.h"
#include "aion/gameserver/model/gameobjects/Item.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/items/PendingTuneResult.h"
#include "aion/gameserver/model/items/storage/Storage.h"
#include "aion/gameserver/network/aion/AionConnection.h"
#include "aion/gameserver/network/aion/serverpackets/SM_INVENTORY_UPDATE_ITEM.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/services/item/ItemActionService.h"
#include "aion/gameserver/utils/PacketSendUtility.h"
#include "aion/gameserver/utils/audit/AuditLogger.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets {

using model::gameobjects::Item;
using model::gameobjects::player::Player;
using serverpackets::SM_INVENTORY_UPDATE_ITEM;
using serverpackets::SM_SYSTEM_MESSAGE;
using utils::PacketSendUtility;

CM_TUNE_RESULT::CM_TUNE_RESULT(int32_t opcode, const StateSet& validStates) : AionClientPacket(opcode, validStates) {
}

// Java CM_TUNE_RESULT.java:28-31
void CM_TUNE_RESULT::readImpl() {
	itemObjectId = readD();
	hasAccepted = readC() == 1;
}

// Java CM_TUNE_RESULT.java:34-50
void CM_TUNE_RESULT::runImpl() {
	const runtime::Ptr<Player> player = getConnection()->getActivePlayer();
	const runtime::Ptr<Item> itemToTune = player->getInventory().getItemByObjId(itemObjectId);
	if (itemToTune) {
		bool auditInvalidEvent = !hasAccepted && itemToTune->getPendingTuneResult() && itemToTune->getPendingTuneResult()->isAttributeOnly();
		if (hasAccepted || auditInvalidEvent) {
			if (auditInvalidEvent)
				utils::audit::AuditLogger::log(*player, "tried to cancel a attribute re-identification which is not possible by default");
			services::item::ItemActionService::applyTuneResult(*player, *itemToTune);
			PacketSendUtility::sendPacket(*player, SM_SYSTEM_MESSAGE::STR_MSG_ITEM_REIDENTIFY_APPLY_YES(itemToTune->getL10n()));
		} else {
			itemToTune->setPendingTuneResult(nullptr);
			PacketSendUtility::sendPacket(*player, SM_SYSTEM_MESSAGE::STR_MSG_ITEM_REIDENTIFY_APPLY_NO());
		}
		PacketSendUtility::sendPacket(*player, SM_INVENTORY_UPDATE_ITEM(*player, *itemToTune));
	}
}

AION_CLIENT_PACKET(CM_TUNE_RESULT);

} // namespace aion::gameserver::network::aion::clientpackets
