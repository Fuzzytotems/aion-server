#include "aion/gameserver/network/aion/clientpackets/CM_QUIT.h"

#include <memory>

#include "aion/gameserver/dao/PlayerPunishmentsDAO.h"
#include "aion/gameserver/handlers/HandlerRegistry.h"
#include "aion/gameserver/model/DialogAction.h"
#include "aion/gameserver/model/account/Account.h"
#include "aion/gameserver/model/account/CharacterBanInfo.h"
#include "aion/gameserver/model/account/PlayerAccountData.h"
#include "aion/gameserver/model/gameobjects/Npc.h"
#include "aion/gameserver/model/gameobjects/VisibleObject.h"
#include "aion/gameserver/model/gameobjects/player/Equipment.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/gameobjects/player/PlayerCommonData.h"
#include "aion/gameserver/model/templates/npc/NpcTemplate.h"
#include "aion/gameserver/network/aion/AionConnection.h"
#include "aion/gameserver/network/aion/serverpackets/SM_QUIT_RESPONSE.h"
#include "aion/gameserver/services/player/PlayerLeaveWorldService.h"
#include "aion/gameserver/utils/PositionUtil.h"
#include "aion/gameserver/utils/audit/AuditLogger.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets {

CM_QUIT::CM_QUIT(int32_t opcode, const StateSet& validStates) : AionClientPacket(opcode, validStates) {
}

void CM_QUIT::readImpl() {
	stayConnected = readC() == 1;
}

void CM_QUIT::runImpl() {
	const std::shared_ptr<AionConnection>& con = getConnection();
	runtime::Ptr<model::gameobjects::player::Player> player = con->getActivePlayer();
	bool charEditScreen = false;
	if (player) {
		charEditScreen = player->getCommonData()->isInEditMode();
		if (charEditScreen) {
			runtime::Ptr<model::gameobjects::Npc> npc = runtime::as<model::gameobjects::Npc>(player->getTarget());
			if (!npc ||
				(!npc->getObjectTemplate()->supportsAction(model::DialogAction::EDIT_CHARACTER_ALL) &&
					!npc->getObjectTemplate()->supportsAction(model::DialogAction::EDIT_CHARACTER_GENDER)) ||
				!utils::PositionUtil::isInTalkRange(*player, *npc)) {
				utils::audit::AuditLogger::log(*player, "tried to enter the plastic surgery screen without targeting the respective npc within talk distance");
				return;
			}
		}
		if (stayConnected) { // update char selection info
			player->getAccountData()->setVisibleItems(player->getEquipment().getEquippedForAppearance());
			for (runtime::Ptr<model::account::PlayerAccountData> plAccData : con->getAccount()->getPlayerAccDataList())
				plAccData->setCharBanInfo(dao::PlayerPunishmentsDAO::getCharBanInfo(plAccData->getPlayerCommonData()->getPlayerObjId()));
		}
		// java-race: CM_QUIT's leaveWorld is not synchronized with AionConnection.safeLogout, so a shutdown during CM_QUIT can run it twice (m5a-plan.md §7)
		services::player::PlayerLeaveWorldService::leaveWorld(*player);
	}
	if (stayConnected)
		sendPacket(serverpackets::SM_QUIT_RESPONSE(charEditScreen));
	else
		con->close(serverpackets::SM_QUIT_RESPONSE(charEditScreen)); // makes sure this packet will be sent before closing connection
}

AION_CLIENT_PACKET(CM_QUIT);

} // namespace aion::gameserver::network::aion::clientpackets
