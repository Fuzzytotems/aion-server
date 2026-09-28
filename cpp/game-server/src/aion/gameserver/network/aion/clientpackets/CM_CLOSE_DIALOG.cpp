#include "aion/gameserver/network/aion/clientpackets/CM_CLOSE_DIALOG.h"

#include "aion/gameserver/handlers/HandlerRegistry.h"
#include "aion/gameserver/model/gameobjects/VisibleObject.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/network/aion/AionConnection.h"
#include "aion/gameserver/network/aion/serverpackets/SM_LOOKATOBJECT.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/services/DialogService.h"
#include "aion/gameserver/world/knownlist/KnownList.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets {

using model::gameobjects::VisibleObject;
using model::gameobjects::player::Player;

CM_CLOSE_DIALOG::CM_CLOSE_DIALOG(int32_t opcode, const StateSet& validStates) : AionClientPacket(opcode, validStates) {
}

// Java CM_CLOSE_DIALOG.java:24-26
void CM_CLOSE_DIALOG::readImpl() {
	targetObjectId = readD();
}

// Java CM_CLOSE_DIALOG.java:29-36
void CM_CLOSE_DIALOG::runImpl() {
	const runtime::Ptr<Player> player = getConnection()->getActivePlayer();
	const runtime::Ptr<VisibleObject> target = player->getKnownList().getObject(targetObjectId);
	if (!target)
		return;
	services::DialogService::onCloseDialog(*player, target);
	sendPacket(serverpackets::SM_LOOKATOBJECT(*target));
}

AION_CLIENT_PACKET(CM_CLOSE_DIALOG);

} // namespace aion::gameserver::network::aion::clientpackets
