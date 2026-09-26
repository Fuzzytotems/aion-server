#include "aion/chatserver/network/aion/clientpackets/CM_PING.h"

namespace aion::chatserver::network::aion::clientpackets {

void CM_PING::readImpl() {
	readC(); // 0
	readH(); // 0
	readB(16); // 0
}

} // namespace aion::chatserver::network::aion::clientpackets
