#include "aion/gameserver/network/aion/clientpackets/CM_TARGET_SELECT.h"

#include <string>

#include "aion/gameserver/handlers/HandlerRegistry.h"
#include "aion/gameserver/model/gameobjects/VisibleObject.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/team/GeneralTeam.h"
#include "aion/gameserver/model/team/TeamMember.h"
#include "aion/gameserver/model/team/TemporaryPlayerTeam.h"
#include "aion/gameserver/network/aion/AionConnection.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/utils/audit/AuditLogger.h"
#include "aion/gameserver/world/knownlist/KnownList.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets {

using model::gameobjects::VisibleObject;
using model::gameobjects::player::Player;
using serverpackets::SM_SYSTEM_MESSAGE;

CM_TARGET_SELECT::CM_TARGET_SELECT(int32_t opcode, const StateSet& validStates) : AionClientPacket(opcode, validStates) {
}

void CM_TARGET_SELECT::readImpl() {
	targetObjectId = readD();
	selectTargetOfTarget = readC() == 1;
}

void CM_TARGET_SELECT::runImpl() {
	runtime::Ptr<Player> player = getConnection()->getActivePlayer();

	runtime::Ptr<VisibleObject> newTarget;
	if (selectTargetOfTarget) {
		// Java reads player.getTarget() twice (CM_TARGET_SELECT.java:41 and :45); C++ reads the Field once and holds the Ptr, because the second
		// read has to keep the object alive while its own target is read. Only a concurrent setTarget between the two reads could tell the two
		// apart, and there Java would throw a NullPointerException on line 45 (see the wave B report's deviations note).
		runtime::Ptr<VisibleObject> currentTarget = player->getTarget();
		if (!currentTarget) {
			sendPacket(SM_SYSTEM_MESSAGE::STR_ASSISTKEY_THIS_IS_ASSISTKEY());
			return;
		}
		newTarget = currentTarget->getTarget();
		if (!newTarget) {
			sendPacket(SM_SYSTEM_MESSAGE::STR_ASSISTKEY_NO_USER());
			return;
		}
		if (!newTarget->equals(*player) && !player->getKnownList().sees(*newTarget)) {
			sendPacket(player->getKnownList().knows(*newTarget) ? SM_SYSTEM_MESSAGE::STR_ASSISTKEY_NO_USER()
															    : SM_SYSTEM_MESSAGE::STR_ASSISTKEY_TOO_FAR());
			return;
		}
	} else if (targetObjectId == 0) {
		newTarget = nullptr;
	} else if (targetObjectId == player->getObjectId()) {
		newTarget = player;
	} else {
		newTarget = player->getKnownList().getObject(targetObjectId);
		// Java: player.getCurrentTeam().getMember(id).getObject() is a Player (TemporaryPlayerTeam binds M = Player). GeneralTeam::hasMember and
		// getMember are P5-10 bodies and still AION_UNPORTED; nothing in the tree puts a player into a group or an alliance at M5a (no caller of
		// setPlayerGroup/setPlayerAllianceGroup exists), so isInTeam() is false and this arm is unreachable, exactly as at
		// PlayerLifeStats.cpp:67-70. Written as Java has it, so it reports the missing bodies by name once teams land. The erasure spells
		// GeneralTeam's M as AionObject (hub-headers.md §8.1), so the narrowing is explicit; `cast` and not `as`, because Java's generic
		// assignment is a checkcast, as TemporaryPlayerTeam::getLeaderObject has it (TemporaryPlayerTeam.cpp:44-46).
		if (!newTarget && player->isInTeam() && player->getCurrentTeam()->hasMember(targetObjectId)) {
			newTarget = runtime::cast<Player>(player->getCurrentTeam()->getMember(targetObjectId)->getObject());
		} else if (newTarget && !player->equals(*newTarget) && !player->getKnownList().sees(*newTarget)) {
			utils::audit::AuditLogger::log(*player, "possibly used radar hack: trying to target invisible " + newTarget->toString());
			newTarget = nullptr;
		}
	}
	player->setTarget(newTarget);
}

AION_CLIENT_PACKET(CM_TARGET_SELECT);

} // namespace aion::gameserver::network::aion::clientpackets
