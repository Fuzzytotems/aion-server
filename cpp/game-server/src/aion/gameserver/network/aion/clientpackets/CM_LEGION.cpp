#include "aion/gameserver/network/aion/clientpackets/CM_LEGION.h"

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/team/legion/Legion.h"
#include "aion/gameserver/network/aion/serverpackets/SM_LEGION_INFO.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/services/LegionService.h"
#include "aion/gameserver/utils/Util.h"
#include "aion/gameserver/handlers/HandlerRegistry.h"
#include "aion/gameserver/network/aion/AionConnection.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets {

using model::gameobjects::player::Player;
using serverpackets::SM_SYSTEM_MESSAGE;
using services::LegionService;


CM_LEGION::CM_LEGION(int32_t opcode, const StateSet& validStates) : AionClientPacket(opcode, validStates) {
}

// Java CM_LEGION.java:38-110
void CM_LEGION::readImpl() {
	exOpcode = readUC();
	switch (exOpcode) {
		// Create a legion
		case 0x00:
			readD(); // 00 78 19 00 40
			legionName = readS();
			break;
		// Invite to legion
		case 0x01:
			readD(); // empty
			charName = readS();
			break;
		// Leave legion
		case 0x02:
			readD(); // empty
			readH(); // empty
			break;
		// Kick member from legion
		case 0x04:
			readD(); // empty
			charName = readS();
			break;
		// Appoint a new Brigade General
		case 0x05:
			readD();
			charName = readS();
			break;
		// Change rank
		case 0x06:
			rank = readD();
			charName = readS();
			break;
		// Show current announcement (via /gnotice)
		case 0x07:
		// Refresh legion info
		case 0x08:
			readD(); // 0
			readH(); // empty
			break;
		// Edit current announcement (from legion window or via /gnotice New text)
		case 0x09:
			readD(); // empty or char id?
			announcement = readS();
			break;
		// Change self introduction
		case 0x0A:
			readD(); // empty char id?
			newSelfIntro = readS();
			break;
		// Edit permissions
		case 0x0D:
			deputyPermission = readH();
			centurionPermission = readH();
			legionarPermission = readH();
			volunteerPermission = readH();
			break;
		// Level legion up
		case 0x0E:
			readD(); // empty
			readH(); // empty
			break;
		case 0x0F:
			charName = readS();
			newNickname = readS();
			break;
		case 0x10: // selected legion dominion
			legionDominionId = readD();
			break;
		default:
			// Java: "Unknown Legion exOpcode 0x" + Integer.toHexString(exOpcode).toUpperCase()
			commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.network.aion.clientpackets.CM_LEGION").warn("Unknown Legion exOpcode 0x{:X}", exOpcode);
			break;
	}
}

// Java CM_LEGION.java:113-158. A string the exOpcode did not read is Java's null: only the arms that read it pass it on
void CM_LEGION::runImpl() {
	const runtime::Ptr<Player> activePlayer = getConnection()->getActivePlayer();
	if (activePlayer->isLegionMember()) {
		const runtime::Ptr<model::team::legion::Legion> legion = activePlayer->getLegion();
		if (charName)
			charName = utils::Util::convertName(*charName);
		switch (exOpcode) {
			// invite to legion
			case 0x01:
				LegionService::getInstance().invitePlayerToLegion(*activePlayer, *charName);
				break;
			// leave legion
			case 0x02:
				LegionService::getInstance().leaveLegion(*activePlayer, false);
				break;
			// kick member
			case 0x04:
				LegionService::getInstance().kickMember(*activePlayer, *charName);
				break;
			// appoint a new Brigade General
			case 0x05:
				LegionService::getInstance().startBrigadeGeneralChangeProcess(*activePlayer, *charName);
				break;
			// change rank
			case 0x06:
				LegionService::getInstance().appointRank(*activePlayer, *charName, rank);
				break;
			// show legion notice (from /gnotice chat command)
			case 0x07: {
				const runtime::Ptr<model::team::legion::Legion::Announcement> currentAnnouncement = legion->getAnnouncement();
				if (!currentAnnouncement)
					sendPacket(SM_SYSTEM_MESSAGE::STR_MSG_NOSET_GUILD_NOTICE());
				else
					sendPacket(SM_SYSTEM_MESSAGE::STR_GUILD_NOTICE(currentAnnouncement->message(), currentAnnouncement->time().time_since_epoch().count() / 1000));
				break;
			}
			// refresh legion info
			case 0x08:
				sendPacket(serverpackets::SM_LEGION_INFO(*legion));
				break;
			// edit announcements
			case 0x09:
				LegionService::getInstance().changeAnnouncement(*activePlayer, *announcement);
				break;
			// change self introduction
			case 0x0A:
				LegionService::getInstance().changeSelfIntro(*activePlayer, *newSelfIntro);
				break;
			// edit permissions
			case 0x0D:
				LegionService::getInstance().changePermissions(*activePlayer, deputyPermission, centurionPermission, legionarPermission, volunteerPermission);
				break;
			// level up legion
			case 0x0E:
				LegionService::getInstance().requestChangeLevel(*activePlayer);
				break;
			// change nickname
			case 0x0F:
				LegionService::getInstance().changeNickname(*activePlayer, *charName, *newNickname);
				break;
			// select Legion Dominion to participate
			case 0x10:
				LegionService::getInstance().joinLegionDominion(*activePlayer, legionDominionId);
				break;
			default:
				break;
		}
	} else {
		switch (exOpcode) {
			case 0x00: // create a legion
				LegionService::getInstance().createLegion(*activePlayer, *legionName);
				break;
			default:
				break;
		}
	}
}

AION_CLIENT_PACKET(CM_LEGION);

} // namespace aion::gameserver::network::aion::clientpackets
