#include "aion/gameserver/network/aion/clientpackets/CM_VIEW_PLAYER_DETAILS.h"

#include "aion/gameserver/configs/administration/AdminConfig.h"
#include "aion/gameserver/handlers/HandlerRegistry.h"
#include "aion/gameserver/model/gameobjects/player/DeniedStatus.h"
#include "aion/gameserver/model/gameobjects/player/Equipment.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/gameobjects/player/PlayerSettings.h"
#include "aion/gameserver/network/aion/AionConnection.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/network/aion/serverpackets/SM_VIEW_PLAYER_DETAILS.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/world/knownlist/KnownList.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets {

using model::gameobjects::player::DeniedStatus;
using model::gameobjects::player::Player;

CM_VIEW_PLAYER_DETAILS::CM_VIEW_PLAYER_DETAILS(int32_t opcode, const StateSet& validStates) : AionClientPacket(opcode, validStates) {
}

// Java CM_VIEW_PLAYER_DETAILS.java:25-27
void CM_VIEW_PLAYER_DETAILS::readImpl() {
	targetObjectId = readD();
}

// Java CM_VIEW_PLAYER_DETAILS.java:30-40
void CM_VIEW_PLAYER_DETAILS::runImpl() {
	const runtime::Ptr<Player> player = getConnection()->getActivePlayer();
	const runtime::Ptr<Player> target = player->getKnownList().getPlayer(targetObjectId);
	if (target == nullptr)
		return;
	if (!target->getPlayerSettings()->isInDeniedStatus(DeniedStatus::VIEW_DETAILS) ||
		player->hasAccess(configs::administration::AdminConfig::VIEW_PLAYER_DETAILS.load()))
		sendPacket(serverpackets::SM_VIEW_PLAYER_DETAILS(target->getEquipment().getEquippedItemsWithoutStigma(), *target));
	else
		sendPacket(serverpackets::SM_SYSTEM_MESSAGE::STR_MSG_REJECTED_WATCH(target->getName()));
}

AION_CLIENT_PACKET(CM_VIEW_PLAYER_DETAILS);

} // namespace aion::gameserver::network::aion::clientpackets
