#include "aion/chatserver/network/aion/clientpackets/CM_PLAYER_INFO.h"

namespace aion::chatserver::network::aion::clientpackets {

void CM_PLAYER_INFO::readImpl() {
	readC(); // 0
	readH(); // 0
	classId = readC();
	readD(); // 0
	level = readD();
	unk = readB(135);
}

} // namespace aion::chatserver::network::aion::clientpackets
