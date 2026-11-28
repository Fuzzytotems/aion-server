#include "aion/chatserver/network/aion/ClientPacketHandler.h"

#include "aion/chatserver/network/aion/clientpackets/CM_CHANNEL_CREATE.h"
#include "aion/chatserver/network/aion/clientpackets/CM_CHANNEL_JOIN.h"
#include "aion/chatserver/network/aion/clientpackets/CM_CHANNEL_LEAVE.h"
#include "aion/chatserver/network/aion/clientpackets/CM_CHANNEL_MESSAGE.h"
#include "aion/chatserver/network/aion/clientpackets/CM_CHANNEL_REQUEST.h"
#include "aion/chatserver/network/aion/clientpackets/CM_CHAT_INI.h"
#include "aion/chatserver/network/aion/clientpackets/CM_PING.h"
#include "aion/chatserver/network/aion/clientpackets/CM_PLAYER_AUTH.h"
#include "aion/chatserver/network/aion/clientpackets/CM_PLAYER_INFO.h"
#include <fmt/format.h>

#include "aion/chatserver/network/aion/AionConnection.h"
#include "aion/commons/logging/LoggerFactory.h"
#include "aion/commons/utils/NetworkUtils.h"

namespace aion::chatserver::network::aion {

using namespace clientpackets;
using State = AionConnection::State;

namespace {

const commons::logging::Logger& log() {
	static const auto* logger = new commons::logging::Logger(commons::logging::LoggerFactory::getLogger("com.aionemu.chatserver.network.aion.ClientPacketHandler"));
	return *logger;
}

void logUnknownPacket(int8_t opCode, State state, const commons::utils::ByteBuffer& buf) {
	log().warn("Unknown packet received from client: opCode=0x{} state={} length={} data=[{}]", fmt::format("{:02X}", static_cast<uint8_t>(opCode)),
		toString(state), buf.remaining(), commons::utils::NetworkUtils::toHex(buf));
}

} // namespace

std::unique_ptr<AbstractClientPacket> ClientPacketHandler::handle(commons::utils::ByteBuffer& buf, const std::shared_ptr<AionConnection>& connection) {
	/*
		retail (KOR 8.2.22) client packet names for opcodes:
		0x0012  (Leave channel)
		0x0018 C2S_CHANNEL_CHATTING
		0x002c C2S_SET_USER_GAME_INFO
		0x002f C2S_CHANNEL_CHATTING_EX
		0x4003 (Get Chat ID)
		0x4005 C2S_SIGNATURE_LOGIN
		0x4006 (NP login)
		0x4007 (Web logout)
		0x400b (Create channel)
		0x400d (Join channel)
		0x4010 (Join open channel)
		0x4014 (Get channel member)
		0x4016 (Get channel banned member)
		0x401b (Ban from channel)
		0x4020 (Unban channel)
		0x4022 (Search channel)
		0x4024 (Change channel operator)
		0x4027 (Change channel property)
		0x402a (Get User Game Info)
		0x402d (Get Chat Server Address)
		0x4032 (Create and join channel)
		0x4901 C2S_ACTIVATE_VOICECHAT
		0x4904 C2S_DEACTIVATE_VOICECHAT
		0x4907 C2S_START_VOICECHAT
		0x4910 C2S_STOP_VOICECHAT
		0x4913 C2S_MUTE_USER
		0x4916 C2S_UNMUTE_USER   (H: pktsz(0x16)   H: opcode  H: ?(zero?)  D: taskId   D: channelId  Q: userID )
		0x4919 C2S_CHANGE_VOICECHAT_PROPERTY
		---------------------------
		retail (KOR 8.2.22) server packet names for opcodes:
		0x0006 (connect related) reconnect?
		0x000f S2C_JOINED_CHANNEL
		0x0013 S2C_LEFT_CHANNEL
		0x0019 S2C_CHANNEL_CHATTING_FAILED
		0x001a S2C_CHANNEL_CHATTING
		0x001d S2C_BANNED_FROM_CHANNEL
		0x0026 S2C_CHANNEL_OPERATOR_CHANGED
		0x0029 S2C_CHANNEL_PROPERTY_CHANGED
		0x0903 ?
		0x0906 ?
		0x0909 ?
		0x0912 ?
		0x0915 ?
		0x0918 ?
		0x0921 ?
		0x4002 S2C_LOGIN_RESULT
		0x4004 S2C_GET_CHAT_ID_FROM_GLOBAL_RESULT
		0x400c S2C_CREATE_CHANNEL_RESULT
		0x400e S2C_JOIN_CHANNEL_RESULT
		0x4011 S2C_JOIN_OPEN_CHANNEL_RESULT
		0x401c S2C_BAN_FROM_CHANNEL_RESULT
		0x4021 S2C_UNBAN_FROM_CHANNEL_RESULT
		0x4023 S2C_SEARCH_CHANNEL_RESULT
		0x4025 S2C_CHANGE_CHANNEL_OPERATOR_RESULT
		0x4028 S2C_CHANGE_CHANNEL_PROPERTY_RESULT
		0x402b S2C_GET_USER_GAME_INFO_RESULT
		0x402e S2C_GET_CHAT_SERVER_ADDR_RESULT
		0x4031 S2C_VERSION_INFO_RESULT
		0x4033 S2C_JOIN_CHANNEL_RESULT  (only in latest client)
		0x4035 S2C_SPAMMER_REGIST_RESULT (only in latest client)
		0x4905 ?
		0x4908 ?
		0x4911 ?
		0x4914 ?
		0x4917 ?   (H: pktsz(0x18)   H: opcode  D: taskID?(search in queue list for task with opcode 0x4916)   D: channelId  Q: userID D: ? )
		0x4920 ?
	 */
	int8_t opCode = buf.get();
	State state = connection->getState();
	std::unique_ptr<AbstractClientPacket> clientPacket;

	switch (state) {
		case State::CONNECTED:
			switch (opCode) {
				case 0x30:
					clientPacket = std::make_unique<CM_CHAT_INI>(buf, connection, opCode);
					break;
				case 0x05:
					clientPacket = std::make_unique<CM_PLAYER_AUTH>(buf, connection, opCode);
					break;
				default:
					logUnknownPacket(opCode, state, buf);
			}
			break;
		case State::AUTHED:
			switch (opCode) {
				case 0x0B:
					clientPacket = std::make_unique<CM_CHANNEL_CREATE>(buf, connection, opCode);
					break;
				case 0x0D:
					clientPacket = std::make_unique<CM_CHANNEL_JOIN>(buf, connection, opCode);
					break;
				case 0x10:
					clientPacket = std::make_unique<CM_CHANNEL_REQUEST>(buf, connection, opCode);
					break;
				case 0x12:
					clientPacket = std::make_unique<CM_CHANNEL_LEAVE>(buf, connection, opCode);
					break;
				case 0x18:
					clientPacket = std::make_unique<CM_CHANNEL_MESSAGE>(buf, connection, opCode);
					break;
				case 0x2C:
					clientPacket = std::make_unique<CM_PLAYER_INFO>(buf, connection, opCode);
					break;
				case static_cast<int8_t>(0xFF):
					clientPacket = std::make_unique<CM_PING>(buf, connection, opCode);
					break;
				default:
					logUnknownPacket(opCode, state, buf);
			}
			break;
	}
	return clientPacket;
}

} // namespace aion::chatserver::network::aion
