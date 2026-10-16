#include "aion/gameserver/handlers/ai/instance/linkgateFoundry/LinkgateFoundryBossTeleportAI.h"

#include "aion/gameserver/ai/AIActions.h"
#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/model/DialogAction.h"
#include "aion/gameserver/model/DialogPageInfo.h"
#include "aion/gameserver/model/items/storage/Storage.h"
#include "aion/gameserver/network/aion/serverpackets/SM_DIALOG_WINDOW.h"
#include "aion/gameserver/services/teleport/TeleportService.h"
#include "aion/gameserver/world/WorldMapInstance.h"
#include "aion/gameserver/world/WorldPosition.h"

namespace aion::gameserver::handlers::ai::instance::linkgateFoundry {

AION_AI(LinkgateFoundryBossTeleportAI, "linkgateFoundryBossTeleport");

// Java LinkgateFoundryBossTeleportAI.java:28-31
void LinkgateFoundryBossTeleportAI::handleUseItemFinish(Player& player) {
	PacketSendUtility::sendPacket(player, SM_DIALOG_WINDOW(getObjectId(), 1011));
}

// Java LinkgateFoundryBossTeleportAI.java:33-37
bool LinkgateFoundryBossTeleportAI::onDialogSelect(Player& player, int32_t dialogActionId, int32_t questId, int32_t extendedRewardIndex) {
	selectBoss(player, dialogActionId);
	return true;
}

// Java LinkgateFoundryBossTeleportAI.java:39-82
void LinkgateFoundryBossTeleportAI::selectBoss(Player& player, int32_t dialogActionId) {
	int32_t keyId = 185000196;
	int32_t minKeyCount, bossId, msgId;
	bool spawnGuardian;
	switch (dialogActionId) {
		case model::DialogAction::SELECT_BOSS_LEVEL2:
			minKeyCount = 1;
			bossId = 234990;
			msgId = 1402440;
			spawnGuardian = true;
			break;
		case model::DialogAction::SELECT_BOSS_LEVEL3:
			minKeyCount = 5;
			bossId = 233898;
			msgId = 1402441;
			spawnGuardian = true;
			break;
		case model::DialogAction::SELECT_BOSS_LEVEL4:
			minKeyCount = 7;
			bossId = 234991;
			msgId = 1402442;
			spawnGuardian = false;
			break;
		default:
			return;
	}
	int64_t keyCount = player.getInventory().getItemCountByItemId(keyId);
	if (keyCount < minKeyCount) {
		PacketSendUtility::sendPacket(player, SM_DIALOG_WINDOW(getObjectId(), model::id(DialogPage::NO_RIGHT)));
		return;
	}
	player.getInventory().decreaseByItemId(keyId, keyCount);
	// replace portal
	AIActions::deleteOwner(*this);
	spawn(702592, getOwner().getX(), getOwner().getY(), getOwner().getZ(), int8_t{0}, 51);
	// spawn boss and guardian
	PacketSendUtility::broadcastToMap(getOwner(), msgId);
	getOwner().getPosition()->getWorldMapInstance()->getNpc(233898)->getController().delete_(); // default belsagos spawn (visible from outside)
	spawn(bossId, 252.2439f, 259.3866f, 312.3536f, int8_t{41});
	if (spawnGuardian)
		spawn(player.getRace() == Race::ELYOS ? 855087 : 855088, 226.76f, 256.7708f, 312.577f, int8_t{0});
	// teleport player
	TeleportService::teleportTo(player, getOwner().getWorldId(), 211.32f, 260.0f, 314.0f, int8_t{0}, TeleportAnimation::FADE_OUT_BEAM);
}

} // namespace aion::gameserver::handlers::ai::instance::linkgateFoundry
