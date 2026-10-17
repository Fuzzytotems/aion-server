#include "aion/gameserver/handlers/ai/instance/rakes/CentralDeckMobileCannonAI.h"

#include <string>
#include <vector>

#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/model/items/storage/Storage.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/world/WorldMapInstance.h"
#include "aion/gameserver/world/WorldPosition.h"

namespace aion::gameserver::handlers::ai::instance::rakes {

AION_AI(CentralDeckMobileCannonAI, "centralcannon");

// Java CentralDeckMobileCannonAI.java:22-39
void CentralDeckMobileCannonAI::handleUseItemFinish(Player& player) {
	if (!player.getInventory().decreaseByItemId(185000052, 1)) {
		PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE(1111302, std::vector<std::string>{}));
		return;
	}

	if (player.getWorldId() == 300100000) {
		runtime::Ptr<WorldMapInstance> worldMapInstance = player.getPosition()->getWorldMapInstance();
		// need check
		// getOwner().getController().useSkill(18572);

		for (const runtime::Ptr<Npc>& npc : worldMapInstance->getNpcs({215402, 215403, 215404, 215405, 230727, 231460, 214968})) {
			npc->getController().die(getOwner());
		}
		spawn(215069, 473.178f, 508.966f, 1032.84f, int8_t{0}); // Chief Mate Menekiki
	}
}

} // namespace aion::gameserver::handlers::ai::instance::rakes
