#include "aion/gameserver/network/aion/clientpackets/CM_LEGION_WH_KINAH.h"

#include <string>

#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/items/storage/Storage.h"
#include "aion/gameserver/model/items/storage/StorageType.h"
#include "aion/gameserver/model/items/storage/StorageTypeInfo.h"
#include "aion/gameserver/model/team/legion/LegionHistoryAction.h"
#include "aion/gameserver/model/team/legion/LegionMember.h"
#include "aion/gameserver/model/team/legion/LegionPermissionsMask.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/services/LegionService.h"
#include "aion/gameserver/utils/PacketSendUtility.h"
#include "aion/gameserver/handlers/HandlerRegistry.h"
#include "aion/gameserver/network/aion/AionConnection.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets {

using model::gameobjects::player::Player;
using model::team::legion::LegionHistoryAction;
using model::team::legion::LegionPermissionsMask;
using serverpackets::SM_SYSTEM_MESSAGE;


CM_LEGION_WH_KINAH::CM_LEGION_WH_KINAH(int32_t opcode, const StateSet& validStates) : AionClientPacket(opcode, validStates) {
}

// Java CM_LEGION_WH_KINAH.java:30-33
void CM_LEGION_WH_KINAH::readImpl() {
	amount = readQ();
	actionType = readC();
}

// Java CM_LEGION_WH_KINAH.java:36-64
void CM_LEGION_WH_KINAH::runImpl() {
	const runtime::Ptr<Player> activePlayer = getConnection()->getActivePlayer();
	const runtime::Ptr<model::team::legion::LegionMember> legionMember = activePlayer->getLegionMember();
	if (!legionMember)
		return;
	const int32_t legionWarehouse = model::items::storage::getId(model::items::storage::StorageType::LEGION_WAREHOUSE);
	switch (actionType) {
		case 0:
			if (!legionMember->hasRights(LegionPermissionsMask::WH_WITHDRAWAL)) {
				// You do not have the authority to use the Legion warehouse.
				utils::PacketSendUtility::sendPacket(*activePlayer, SM_SYSTEM_MESSAGE::STR_GUILD_WAREHOUSE_NO_RIGHT());
				return;
			}
			if (activePlayer->getStorage(legionWarehouse)->tryDecreaseKinah(amount)) {
				activePlayer->getInventory().increaseKinah(amount);
				services::LegionService::getInstance().addHistory(*legionMember->getLegion(), activePlayer->getName(), LegionHistoryAction::KINAH_WITHDRAW,
					std::to_string(amount));
			}
			break;
		case 1:
			if (!legionMember->hasRights(LegionPermissionsMask::WH_DEPOSIT)) {
				// You do not have the authority to use the Legion warehouse.
				utils::PacketSendUtility::sendPacket(*activePlayer, SM_SYSTEM_MESSAGE::STR_GUILD_WAREHOUSE_NO_RIGHT());
				return;
			}
			if (activePlayer->getInventory().tryDecreaseKinah(amount)) {
				activePlayer->getStorage(legionWarehouse)->increaseKinah(amount);
				services::LegionService::getInstance().addHistory(*legionMember->getLegion(), activePlayer->getName(), LegionHistoryAction::KINAH_DEPOSIT,
					std::to_string(amount));
			}
			break;
		default:
			break;
	}
}

AION_CLIENT_PACKET(CM_LEGION_WH_KINAH);

} // namespace aion::gameserver::network::aion::clientpackets
