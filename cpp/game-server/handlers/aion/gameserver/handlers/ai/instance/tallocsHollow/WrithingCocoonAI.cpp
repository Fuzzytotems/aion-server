#include "aion/gameserver/handlers/ai/instance/tallocsHollow/WrithingCocoonAI.h"

#include <string>
#include <vector>

#include "aion/gameserver/ai/AIActions.h"
#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/model/DialogAction.h"
#include "aion/gameserver/model/items/storage/Storage.h"
#include "aion/gameserver/network/aion/serverpackets/SM_DIALOG_WINDOW.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/world/WorldMapInstance.h"
#include "aion/gameserver/world/WorldPosition.h"

namespace aion::gameserver::handlers::ai::instance::tallocsHollow {

AION_AI(WrithingCocoonAI, "writhingcocoon");

// Java WrithingCocoonAI.java:24-51
bool WrithingCocoonAI::onDialogSelect(Player& player, int32_t dialogActionId, int32_t questId, int32_t extendedRewardIndex) {
	if (dialogActionId == model::DialogAction::SELECT1_1 && player.getInventory().decreaseByItemId(185000088, 1)) {
		PacketSendUtility::sendPacket(player, SM_DIALOG_WINDOW(getObjectId(), 0));
		switch (getNpcId()) {
			case 730232: {
				runtime::Ptr<Npc> npc = getPosition()->getWorldMapInstance()->getNpc(730233);
				if (npc != nullptr) {
					npc->getController().delete_();
				}
				spawn(799500, getPosition()->getX(), getPosition()->getY(), getPosition()->getZ(), getPosition()->getHeading());
				PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE(390510, std::vector<std::string>{})); // Will you accompany me? Tell me if you will.
				break;
			}
			case 730233: {
				runtime::Ptr<Npc> npc1 = getPosition()->getWorldMapInstance()->getNpc(730232);
				if (npc1 != nullptr) {
					npc1->getController().delete_();
				}
				spawn(799501, getPosition()->getX(), getPosition()->getY(), getPosition()->getZ(), getPosition()->getHeading());
				PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE(390511, std::vector<std::string>{})); // Let me know if you need my help.
				break;
			}
			default:
				break;
		}
		AIActions::deleteOwner(*this);
	} else if (dialogActionId == model::DialogAction::SELECT1_1) {
		PacketSendUtility::sendPacket(player, SM_DIALOG_WINDOW(getObjectId(), 1097));
	}
	return true;
}

// Java WrithingCocoonAI.java:53-56
void WrithingCocoonAI::handleDialogStart(Player& player) {
	PacketSendUtility::sendPacket(player, SM_DIALOG_WINDOW(getObjectId(), 1011));
}

} // namespace aion::gameserver::handlers::ai::instance::tallocsHollow
