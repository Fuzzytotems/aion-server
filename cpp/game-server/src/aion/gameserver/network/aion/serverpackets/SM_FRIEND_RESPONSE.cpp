#include "aion/gameserver/network/aion/serverpackets/SM_FRIEND_RESPONSE.h"

#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"

namespace aion::gameserver::network::aion::serverpackets {

const std::shared_ptr<SM_FRIEND_RESPONSE> SM_FRIEND_RESPONSE::TARGET_OFFLINE = std::make_shared<SM_FRIEND_RESPONSE>(0x1);
const std::shared_ptr<SM_FRIEND_RESPONSE> SM_FRIEND_RESPONSE::TARGET_ALREADY_FRIEND = std::make_shared<SM_FRIEND_RESPONSE>(0x02);
const std::shared_ptr<SM_FRIEND_RESPONSE> SM_FRIEND_RESPONSE::TARGET_NOT_FOUND = std::make_shared<SM_FRIEND_RESPONSE>(0x03);
const std::shared_ptr<SM_FRIEND_RESPONSE> SM_FRIEND_RESPONSE::LIST_FULL = std::make_shared<SM_FRIEND_RESPONSE>(0x05);
const std::shared_ptr<SM_FRIEND_RESPONSE> SM_FRIEND_RESPONSE::TARGET_BLOCKED_YOU = std::make_shared<SM_FRIEND_RESPONSE>(0x08);
const std::shared_ptr<SM_FRIEND_RESPONSE> SM_FRIEND_RESPONSE::TARGET_DEAD = std::make_shared<SM_FRIEND_RESPONSE>(0x09);
const std::shared_ptr<SM_FRIEND_RESPONSE> SM_FRIEND_RESPONSE::TOO_MANY_REQUESTS = std::make_shared<SM_FRIEND_RESPONSE>(0x0D);
const std::shared_ptr<SM_FRIEND_RESPONSE> SM_FRIEND_RESPONSE::CLOSE_SEND_REQUEST_WINDOW = std::make_shared<SM_FRIEND_RESPONSE>(0x11);

SM_FRIEND_RESPONSE SM_FRIEND_RESPONSE::TARGET_ADDED(std::string_view targetName) {
	return SM_FRIEND_RESPONSE(targetName, 0x0);
}

SM_FRIEND_RESPONSE SM_FRIEND_RESPONSE::TARGET_DENIED(std::string_view targetName) {
	return SM_FRIEND_RESPONSE(targetName, 0x04);
}

SM_FRIEND_RESPONSE SM_FRIEND_RESPONSE::TARGET_REMOVED(std::string_view targetName) {
	return SM_FRIEND_RESPONSE(targetName, 0x06);
}

SM_FRIEND_RESPONSE SM_FRIEND_RESPONSE::TARGET_LIST_FULL(std::string_view targetName) {
	return SM_FRIEND_RESPONSE(targetName, 0x0A);
}

SM_FRIEND_RESPONSE SM_FRIEND_RESPONSE::TARGET_OFFLINE_SENT_REQUEST(std::string_view targetName) {
	return SM_FRIEND_RESPONSE(targetName, 0x0B);
}

SM_FRIEND_RESPONSE SM_FRIEND_RESPONSE::TARGET_REQUESTED_ALREADY(std::string_view targetName) {
	return SM_FRIEND_RESPONSE(targetName, 0x0C);
}

SM_FRIEND_RESPONSE SM_FRIEND_RESPONSE::REQUESTER_LIST_FULL_CANT_ACCEPT(std::string_view targetName) {
	return SM_FRIEND_RESPONSE(targetName, 0x0E);
}

SM_FRIEND_RESPONSE SM_FRIEND_RESPONSE::REQUEST_DENIED(std::string_view requesterName) {
	return SM_FRIEND_RESPONSE(requesterName, 0x12);
}

SM_FRIEND_RESPONSE SM_FRIEND_RESPONSE::REQUEST_ALREADY_RECEIVED(std::string_view targetName) {
	return SM_FRIEND_RESPONSE(targetName, 0x13);
}

SM_FRIEND_RESPONSE::SM_FRIEND_RESPONSE(int32_t messageType) : SM_FRIEND_RESPONSE("", messageType) {
}

SM_FRIEND_RESPONSE::SM_FRIEND_RESPONSE(std::string_view playerNameValue, int32_t messageType)
	: AionServerPacket(opcodeOf<SM_FRIEND_RESPONSE>), playerName(playerNameValue), code(messageType) {
}

void SM_FRIEND_RESPONSE::writeImpl(AionConnection* con) {
	writeS(playerName);
	writeC(code);
}

} // namespace aion::gameserver::network::aion::serverpackets
