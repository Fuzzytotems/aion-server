#include "aion/chatserver/network/aion/clientpackets/CM_CHANNEL_REQUEST.h"

#include "aion/chatserver/service/ChatService.h"
#include "aion/chatserver/utils/Utf16Le.h"

namespace aion::chatserver::network::aion::clientpackets {

void CM_CHANNEL_REQUEST::readImpl() {
	readC(); // 0x40 = @
	readH(); // 0
	channelRequestId = readD(); // client increases this by 1 for each request (e.g. after teleport)
	readB(16); // 0
	int32_t length = (readH() * 2);
	channelIdentifier = readB(length);
	readD(); // 0
}

void CM_CHANNEL_REQUEST::runImpl() {
	service::ChatService::getInstance().registerPlayerWithChannel(clientChannelHandler, channelRequestId, utils::Utf16Le::newString(channelIdentifier));
}

} // namespace aion::chatserver::network::aion::clientpackets
