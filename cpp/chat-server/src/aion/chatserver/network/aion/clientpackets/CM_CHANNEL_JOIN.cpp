#include "aion/chatserver/network/aion/clientpackets/CM_CHANNEL_JOIN.h"

namespace aion::chatserver::network::aion::clientpackets {

void CM_CHANNEL_JOIN::readImpl() {
	readC(); // 0x40 = @
	readH(); // 0
	channelRequestId = readD(); // client increases this by 1 for each request (e.g. after teleport)
	readB(16); // 0
	int32_t identifierLength = readH() * 2;
	channelIdentifier = readB(identifierLength); // encoded in UTF_16LE
	int32_t passwordLength = readH() * 2;
	password = readB(passwordLength); // encoded in UTF_16LE
}

void CM_CHANNEL_JOIN::runImpl() {
	// TODO see comments in CM_CHANNEL_CREATE
}

} // namespace aion::chatserver::network::aion::clientpackets
