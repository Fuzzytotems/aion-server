#include "aion/chatserver/network/aion/clientpackets/CM_CHANNEL_CREATE.h"

namespace aion::chatserver::network::aion::clientpackets {

void CM_CHANNEL_CREATE::readImpl() {
	readC(); // 0x40 = @
	readH(); // 0
	channelRequestId = readD();
	readB(16); // 0
	int32_t identifierLength = readH() * 2;
	channelIdentifier = readB(identifierLength); // encoded in UTF_16LE
	readB(7); // 0
	int32_t passwordLength = readH() * 2;
	password = readB(passwordLength); // encoded in UTF_16LE
	readH(); // -1
}

void CM_CHANNEL_CREATE::runImpl() {
	// TODO differentiate between language and "normal" user channels (both have "User" as the type identifier),
	// TODO otherwise users can create password protected language channels

	// TODO rework broadcasting + channels, so each channel has its own user list,
	// TODO to remove unused private channels if the last one leaves it / logs out

	// TODO support for private channel password protection
}

} // namespace aion::chatserver::network::aion::clientpackets
