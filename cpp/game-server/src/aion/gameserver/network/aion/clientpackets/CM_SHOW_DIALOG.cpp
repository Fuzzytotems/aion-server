#include "aion/gameserver/network/aion/clientpackets/CM_SHOW_DIALOG.h"

#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/controllers/PlayerController.h"
#include "aion/gameserver/controllers/effect/PlayerEffectController.h"
#include "aion/gameserver/handlers/HandlerRegistry.h"
#include "aion/gameserver/model/gameobjects/Npc.h"
#include "aion/gameserver/model/gameobjects/VisibleObject.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/templates/npc/NpcTemplate.h"
#include "aion/gameserver/model/templates/npc/TalkInfo.h"
#include "aion/gameserver/network/aion/AionConnection.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/world/knownlist/KnownList.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets {

using model::gameobjects::Npc;
using model::gameobjects::player::Player;

CM_SHOW_DIALOG::CM_SHOW_DIALOG(int32_t opcode, const StateSet& validStates) : AionClientPacket(opcode, validStates) {
}

// Java CM_SHOW_DIALOG.java:23-25
void CM_SHOW_DIALOG::readImpl() {
	targetObjectId = readD();
}

// Java CM_SHOW_DIALOG.java:28-42
void CM_SHOW_DIALOG::runImpl() {
	const runtime::Ptr<Player> player = getConnection()->getActivePlayer();
	if (player->isProtectionActive())
		player->getController().stopProtectionActiveTask();

	if (player->isTrading())
		return;

	if (const runtime::Ptr<Npc> target = runtime::as<Npc>(player->getKnownList().getObject(targetObjectId))) {
		const model::templates::npc::TalkInfo* talkInfo = target->getObjectTemplate()->getTalkInfo();
		if (talkInfo != nullptr && !talkInfo->isCanTalkInvisible() && player->isInAnyHide())
			player->getEffectController()->removeHideEffects();
		target->getController().onDialogRequest(*player);
	}
}

AION_CLIENT_PACKET(CM_SHOW_DIALOG);

} // namespace aion::gameserver::network::aion::clientpackets
