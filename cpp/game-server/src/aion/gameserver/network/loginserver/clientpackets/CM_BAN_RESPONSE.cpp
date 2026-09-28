#include "aion/gameserver/network/loginserver/clientpackets/CM_BAN_RESPONSE.h"

#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/utils/PacketSendUtility.h"
#include "aion/gameserver/world/World.h"

namespace aion::gameserver::network::loginserver::clientpackets {

CM_BAN_RESPONSE::CM_BAN_RESPONSE(int32_t opCode) : LsClientPacket(opCode) {
}

void CM_BAN_RESPONSE::readImpl() {
	this->type = readC();
	this->accountId = readD();
	this->ip = readS();
	this->time = readD();
	this->adminObjId = readD();
	this->result = readUC() == 1;
}

void CM_BAN_RESPONSE::runImpl() {
	runtime::Ptr<model::gameobjects::player::Player> admin = world::World::getInstance().getPlayer(adminObjId);

	if (!admin) {
		return;
	}

	// Some messages stuff
	std::string message;
	if (type == 1 || type == 3) {
		if (result) {
			if (time < 0)
				message = "Account ID " + std::to_string(accountId) + " was successfully unbanned";
			else if (time == 0)
				message = "Account ID " + std::to_string(accountId) + " was successfully banned";
			else
				message = "Account ID " + std::to_string(accountId) + " was successfully banned for " + std::to_string(time) + " minutes";
		} else
			message = "Error occurred while banning player's account";
		utils::PacketSendUtility::sendMessage(*admin, message);
	}
	if (type == 2 || type == 3) {
		if (result) {
			if (time < 0)
				message = "IP mask " + ip + " was successfully removed from block list";
			else if (time == 0)
				message = "IP mask " + ip + " was successfully added to block list";
			else
				message = "IP mask " + ip + " was successfully added to block list for " + std::to_string(time) + " minutes";
		} else
			message = "Error occurred while adding IP mask " + ip;
		utils::PacketSendUtility::sendMessage(*admin, message);
	}
}

} // namespace aion::gameserver::network::loginserver::clientpackets
