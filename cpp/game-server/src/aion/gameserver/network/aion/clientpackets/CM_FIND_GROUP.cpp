#include "aion/gameserver/network/aion/clientpackets/CM_FIND_GROUP.h"

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/gameserver/handlers/HandlerRegistry.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/network/aion/AionConnection.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/services/findgroup/FindGroupService.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets {

CM_FIND_GROUP::CM_FIND_GROUP(int32_t opcode, const StateSet& validStates) : AionClientPacket(opcode, validStates) {
}

using model::gameobjects::player::Player;
using services::findgroup::FindGroupService;

// Java CM_FIND_GROUP.java:39-118
void CM_FIND_GROUP::readImpl() {
	action = readUC();
	switch (action) {
		case 0: // recruit list
			break;
		case 1: // offer delete
			playerOrTeamId = readD();
			serverId = readC();
			unk1 = readC();
			unk2 = readC();
			unk3 = readC();
			break;
		case 2: // send offer
			playerOrTeamId = readD();
			message = readS();
			groupType = readUC();
			break;
		case 3: // recruit update
			playerOrTeamId = readD();
			serverId = readC();
			unk1 = readC();
			unk2 = readC();
			unk3 = readC();
			message = readS();
			groupType = readUC();
			break;
		case 4: // apply list
			break;
		case 5: // post delete
			playerOrTeamId = readD();
			break;
		case 6: // apply create
		case 7: // apply update
			playerOrTeamId = readD();
			message = readS();
			groupType = readUC();
			classId = readUC();
			level = readUC();
			break;
		case 8: // register InstanceGroup
			instanceMaskId = readD();
			readUC(); // unk 0
			message = readS(); // text
			minMembers = readUC(); // minMembers chosen by writer
			break;
		case 9: // remove instance group
			playerOrTeamId = readD();
			instanceMaskId = readD();
			break;
		case 10: // show instance groups
			break;
		case 11: // apply for instance group
			playerOrTeamId = readD();
			instanceMaskId = readD();
			break;
		case 12: // accept/deny instance group applicant
			playerOrTeamId = readD();
			instanceApplicationReply = readC(); // 1: accept, 0: deny
			break;
		case 13: // triggered every 50s when instance group tab is open or option "Automatic search when the window is closed" is checked
			break;
		case 15: // show instance group member info
			playerOrTeamId = readD();
			instanceMaskId = readD();
			break;
		case 17:
			playerOrTeamId = readD();
			instanceMaskId = readD();
			message = readS();
			break;
		case 20: // clicked Enter button in Prepare for entry window
			break;
		case 25: // ban from instance group
			playerOrTeamId = readD();
			instanceMaskId = readD();
			bannedPlayerId = readD();
			break;
		default:
			commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.network.aion.clientpackets.CM_FIND_GROUP")
				.warn("Unknown find group action " + std::to_string(action));
			break;
	}
}

// Java CM_FIND_GROUP.java:121-142
void CM_FIND_GROUP::runImpl() {
	const runtime::Ptr<Player> player = getConnection()->getActivePlayer();
	FindGroupService& service = FindGroupService::getInstance();
	switch (action) {
		case 0:
			service.showRecruitments(*player);
			break;
		case 1:
			service.removeRecruitment(*player, serverId, unk1, unk2, unk3);
			break;
		case 2:
			service.addRecruitment(*player, message, groupType);
			break;
		case 3:
			service.updateRecruitment(*player, message, groupType);
			break;
		case 4:
			service.showApplications(*player);
			break;
		case 5:
			service.removeApplication(*player);
			break;
		case 6:
			service.addApplication(*player, message, groupType, classId, level);
			break;
		case 7:
			service.updateApplication(*player, message, groupType, classId, level);
			break;
		case 8:
			service.registerInstanceGroup(*player, instanceMaskId, message, minMembers);
			break;
		case 9:
			service.removeInstanceGroup(*player);
			break;
		case 10:
			service.showInstanceGroups(*player, false);
			break;
		case 11:
			service.sendInstanceApplication(*player, playerOrTeamId);
			break;
		case 12:
			service.sendInstanceApplicationResult(*player, playerOrTeamId, instanceApplicationReply);
			break;
		case 13:
			service.showInstanceGroups(*player, true);
			break;
		case 15:
			service.showInstanceGroupMembersInfo(*player, playerOrTeamId);
			break;
		case 17:
			service.updateInstanceGroup(*player, message);
			break;
		default:
			break;
	}
}

AION_CLIENT_PACKET(CM_FIND_GROUP);

} // namespace aion::gameserver::network::aion::clientpackets
