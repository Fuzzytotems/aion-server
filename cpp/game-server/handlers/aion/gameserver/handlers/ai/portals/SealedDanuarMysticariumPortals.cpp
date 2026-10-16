#include "aion/gameserver/handlers/ai/portals/SealedDanuarMysticariumPortals.h"

#include "aion/gameserver/ai/AIActions.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/Portal2Data.h"
#include "aion/gameserver/model/DialogAction.h"
#include "aion/gameserver/model/gameobjects/player/PortalCooldownList.h"
#include "aion/gameserver/model/items/storage/Storage.h"
#include "aion/gameserver/network/aion/serverpackets/SM_DIALOG_WINDOW.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/services/instance/InstanceService.h"
#include "aion/gameserver/services/teleport/PortalService.h"
#include "aion/gameserver/world/WorldMapInstance.h"

namespace aion::gameserver::handlers::ai::portals {

AION_AI(SealedDanuarMysticariumPortals, "sealed_danuar_mysticarium_portal");

// Java SealedDanuarMysticariumPortals.java:28-65
bool SealedDanuarMysticariumPortals::onDialogSelect(Player& player, int32_t dialogActionId, int32_t questId, int32_t extendedRewardIndex) {
	if (getNpcId() == 730721 || getNpcId() == 730722) {
		const model::templates::portal::PortalPath* portalPath =
			dataholders::DataManager::PORTAL2_DATA->getPortalDialogPath(getNpcId(), dialogActionId, player);
		runtime::Ptr<WorldMapInstance> instance = InstanceService::getRegisteredInstance(300480000, player.getObjectId());
		if (instance == nullptr) {
			if (!player.getPortalCooldownList().isPortalUseDisabled(300480000)) {
				if (player.getInventory().decreaseByItemId(185000223, 1)) {
					PortalService::port(portalPath, player, getOwner());
					return true;
				} else {
					PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_MSG_INSTANCE_CANT_ENTER_WITHOUT_ITEM());
					PacketSendUtility::sendPacket(player, SM_DIALOG_WINDOW(getObjectId(), 0));
					return true;
				}
			} else {
				PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_MSG_CANNOT_MAKE_INSTANCE_COOL_TIME());
				PacketSendUtility::sendPacket(player, SM_DIALOG_WINDOW(getObjectId(), 0));
				return true;
			}
		} else {
			PortalService::port(portalPath, player, getOwner());
			PacketSendUtility::sendPacket(player, SM_DIALOG_WINDOW(getObjectId(), 0));
			return true;
		}
	} else {
		if (!getOwner().isInInstance()) {
			return true;
		}
		if (getNpcId() == 731583) {
			if (dialogActionId == model::DialogAction::SETPRO1) {
				AIActions::handleUseItemFinish(*this, player);
				PacketSendUtility::sendPacket(player, SM_DIALOG_WINDOW(getObjectId(), 0, questId));
			}
		}
	}
	return true;
}

} // namespace aion::gameserver::handlers::ai::portals
