#include "aion/gameserver/handlers/ai/portals/LegionDominionPortalAI.h"

#include <chrono>

#include "aion/gameserver/configs/main/LegionConfig.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/Portal2Data.h"
#include "aion/gameserver/instance/handlers/InstanceHandler.h"
#include "aion/gameserver/model/DialogAction.h"
#include "aion/gameserver/model/gameobjects/player/PortalCooldownList.h"
#include "aion/gameserver/model/items/storage/Storage.h"
#include "aion/gameserver/model/team/alliance/PlayerAlliance.h"
#include "aion/gameserver/model/team/legion/Legion.h"
#include "aion/gameserver/network/aion/serverpackets/SM_DIALOG_WINDOW.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/services/instance/InstanceService.h"
#include "aion/gameserver/services/teleport/PortalService.h"
#include "aion/gameserver/utils/time/ServerTime.h"
#include "aion/gameserver/world/WorldMapInstance.h"

namespace aion::gameserver::handlers::ai::portals {

AION_AI(LegionDominionPortalAI, "legion_dominion_portal");

// Java LegionDominionPortalAI.java:33-112
bool LegionDominionPortalAI::onDialogSelect(Player& player, int32_t dialogActionId, int32_t questId, int32_t extendedRewardIndex) {
	if (dialogActionId != model::DialogAction::SETPRO1) {
		PacketSendUtility::sendPacket(player, SM_DIALOG_WINDOW(getObjectId(), 0, questId));
		return true;
	}
	utils::time::ServerTime::ZonedDateTime now = utils::time::ServerTime::now();
	const std::chrono::local_time<std::chrono::milliseconds> local = now.get_local_time();
	const std::chrono::local_days day = std::chrono::floor<std::chrono::days>(local);
	const int64_t hour = std::chrono::hh_mm_ss(local - day).hours().count();
	if (std::chrono::weekday(day) == std::chrono::Wednesday && hour >= 8 && hour <= 10) {
		PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_MSG_INSTANCE_CLOSED_TIME(301500000));
		PacketSendUtility::sendPacket(player, SM_DIALOG_WINDOW(getObjectId(), 0, questId));
		return true;
	}
	if (player.getLevel() < 65) {
		PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_MSG_CANT_INSTANCE_ENTER_LEVEL());
		PacketSendUtility::sendPacket(player, SM_DIALOG_WINDOW(getObjectId(), 0, questId));
		return true;
	}
	if (player.isInAlliance() && !player.isInLeague()) {
		const model::templates::portal::PortalPath* portalPath =
			dataholders::DataManager::PORTAL2_DATA->getPortalDialogPath(getNpcId(), dialogActionId, player);
		runtime::Ptr<WorldMapInstance> instance = InstanceService::getRegisteredInstance(301500000, player.getPlayerAlliance()->getObjectId());
		if (portalPath == nullptr) {
			PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_MSG_NEW_MAP_INFO_CANT_FIND_INSTANCE());
			PacketSendUtility::sendPacket(player, SM_DIALOG_WINDOW(getObjectId(), 0, questId));
			return true;
		}
		if (instance == nullptr) {
			if (hasCd(player)) {
				return true;
			}
			// only alliance leader can open this instance
			if (player.getPlayerAlliance()->isSomeCaptain(player) && player.getLegion() != nullptr && player.getLegion()->getCurrentLegionDominion() > 0) {
				if (!configs::main::LegionConfig::REQUIRE_KEY_FOR_STONESPEAR_REACH.load() || player.getInventory().decreaseByItemId(185000230, 1)) {
					PortalService::port(portalPath, player, getOwner());
					return true;
				} else {
					PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_MSG_INSTANCE_CANT_ENTER_WITHOUT_ITEM());
					PacketSendUtility::sendPacket(player, SM_DIALOG_WINDOW(getObjectId(), 0));
					return true;
				}
			} else {
				PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_MSG_CANT_INSTANCE_NOT_LEADER());
				PacketSendUtility::sendPacket(player, SM_DIALOG_WINDOW(getObjectId(), 0, questId));
				return true;
			}
		} else if (!instance->isRegistered(player.getObjectId())) {
			if (hasCd(player)) {
				return true;
			}
			runtime::Ptr<InstanceHandler> handler = instance->getInstanceHandler();
			if (handler != nullptr && handler->canEnter(player)) {
				PortalService::port(portalPath, player, getOwner());
			} else {
				PacketSendUtility::sendPacket(player, SM_DIALOG_WINDOW(getObjectId(), 0, questId));
				return true;
			}
		} else if (instance->isRegistered(player.getObjectId())) {
			runtime::Ptr<InstanceHandler> handler = instance->getInstanceHandler();
			if (handler != nullptr && handler->canEnter(player)) {
				PortalService::port(portalPath, player, getOwner());
			} else {
				PacketSendUtility::sendPacket(player, SM_DIALOG_WINDOW(getObjectId(), 0, questId));
				return true;
			}
			return true;
		} else {
			PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_MSG_CANT_INSTANCE_ENTER_STATE());
			PacketSendUtility::sendPacket(player, SM_DIALOG_WINDOW(getObjectId(), 0, questId));
			return true;
		}
	} else {
		PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_MSG_ENTER_ONLY_FORCE_DON());
	}
	PacketSendUtility::sendPacket(player, SM_DIALOG_WINDOW(getObjectId(), 0, questId));
	return true;
}

// Java LegionDominionPortalAI.java:114-121
bool LegionDominionPortalAI::hasCd(Player& player) {
	if (player.getPortalCooldownList().isPortalUseDisabled(301500000)) {
		PacketSendUtility::sendPacket(player, SM_DIALOG_WINDOW(getObjectId(), 0));
		PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_MSG_CANNOT_MAKE_INSTANCE_COOL_TIME());
		return true;
	}
	return false;
}

} // namespace aion::gameserver::handlers::ai::portals
