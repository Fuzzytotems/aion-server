#include "aion/gameserver/handlers/ai/instance/empyreanCrucible/EmpyreanArbiterAI.h"

#include "aion/gameserver/instance/handlers/InstanceHandler.h"
#include "aion/gameserver/model/DialogAction.h"
#include "aion/gameserver/model/instance/instancescore/InstanceScore.h"
#include "aion/gameserver/model/instance/playerreward/CruciblePlayerReward.h"
#include "aion/gameserver/model/items/storage/Storage.h"
#include "aion/gameserver/network/aion/serverpackets/SM_DIALOG_WINDOW.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/services/teleport/TeleportService.h"
#include "aion/gameserver/world/WorldMapInstance.h"
#include "aion/gameserver/world/WorldPosition.h"

namespace aion::gameserver::handlers::ai::instance::empyreanCrucible {

AION_AI(EmpyreanArbiterAI, "empyreanarbiter");

// Java EmpyreanArbiterAI.java:28-36
void EmpyreanArbiterAI::handleDialogStart(Player& player) {
	if (player.getInventory().getFirstItemByItemId(186000124) != nullptr) {
		PacketSendUtility::sendPacket(player, SM_DIALOG_WINDOW(getObjectId(), 1011));
	} else {
		// to do
		PacketSendUtility::sendPacket(player, SM_DIALOG_WINDOW(getObjectId(), 0));
	}
}

// Java EmpyreanArbiterAI.java:38-85 (the anonymous Consumer is a lambda capturing the player)
bool EmpyreanArbiterAI::onDialogSelect(Player& player, int32_t dialogActionId, int32_t questId, int32_t extendedRewardIndex) {
	int32_t instanceId = getPosition()->getInstanceId();

	if (dialogActionId == model::DialogAction::SETPRO1 && player.getInventory().decreaseByItemId(186000124, 1)) {
		switch (getNpcId()) {
			case 799573:
				TeleportService::teleportTo(player, 300300000, instanceId, 358.2547f, 349.26443f, 96.09108f, int8_t{59});
				break;
			case 205426:
				TeleportService::teleportTo(player, 300300000, instanceId, 1260.15f, 812.34f, 358.6056f, int8_t{90});
				break;
			case 205427:
				TeleportService::teleportTo(player, 300300000, instanceId, 1616.0248f, 154.43837f, 126.0f, int8_t{10});
				break;
			case 205428:
				TeleportService::teleportTo(player, 300300000, instanceId, 1793.9233f, 796.92f, 469.36542f, int8_t{60});
				break;
			case 205429:
				TeleportService::teleportTo(player, 300300000, instanceId, 1776.4169f, 1749.9952f, 303.69553f, int8_t{0});
				break;
			case 205430:
				TeleportService::teleportTo(player, 300300000, instanceId, 1328.935f, 1742.0771f, 316.74188f, int8_t{0});
				break;
			case 205431:
				TeleportService::teleportTo(player, 300300000, instanceId, 1760.9441f, 1278.033f, 394.23764f, int8_t{0});
				break;
		}
		runtime::Ptr<model::instance::instancescore::InstanceScore> instance =
			getPosition()->getWorldMapInstance()->getInstanceHandler()->getInstanceScore();
		if (instance != nullptr) {
			runtime::Ptr<model::instance::playerreward::CruciblePlayerReward> reward =
				runtime::cast<model::instance::playerreward::CruciblePlayerReward>(instance->getPlayerReward(player.getObjectId()));
			if (reward != nullptr) {
				reward->setPlayerDefeated(false);
			}
		}
		PacketSendUtility::sendPacket(player, SM_DIALOG_WINDOW(getObjectId(), 0));

		getPosition()->getWorldMapInstance()->forEachPlayer([player = runtime::Ref<Player>(player)](Player& p) {
			PacketSendUtility::sendPacket(p, SM_SYSTEM_MESSAGE::STR_MSG_FRIENDLY_MOVE_COMBATAREA_IDARENA(player->getName()));
		});
	}
	return true;
}

} // namespace aion::gameserver::handlers::ai::instance::empyreanCrucible
