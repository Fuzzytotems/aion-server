#pragma once

#include <memory>

#include "aion/commons/utils/ByteBuffer.h"
#include "aion/loginserver/network/aion/AionClientPacket.h"

/**
 * Creates the client packet for the opcode at the start of a decrypted Aion client packet, depending on the connection state.
 * <p>
 * retail (KOR 8.2.22) client packet names for opcodes:
 * 0 AQ_LOGIN, 1 AQ_SERVER_LIST, 2 AQ_ABOUT_TO_PLAY, 3 AQ_LOGOUT, 4 AQ_LOGIN_MD5, 5 AQ_SERVER_LIST_EX, 6 AQ_SCCHECK, 7 AQ_GAMEGUARD,
 * 8 AQ_UPDATE_SESSION_REQ, 9 AQ_WEBSESSION_LOGIN, 10 AQ_OTPCHECK, 11 AQ_EXTERNAL_TOKEN_LOGIN, 12 AQ_AUX_AUTHENTICATION_ACK, 16 AQ_IOVATION_CHECK,
 * 18 AQ_LOGIN_TOKEN
 * <p>
 * retail (KOR 8.2.22) server packet names for opcodes:
 * 0 AC_PROTOCOL_VER, 1 AC_LOGIN_FAIL, 2 AC_BLOCKED_ACCOUNT, 3 AC_LOGIN_OK, 4 AC_SEND_SERVER_LIST, 5 AC_SEND_SERVER_FAIL, 6 AC_PLAY_FAIL,
 * 7 AC_PLAY_OK, 8 AC_ACCOUNT_KICKED, 9 AC_BLOCKED_ACCOUNT_WITH_MSG, 10 AC_SCCHECK_REQ, 11 AC_GAMEGUARD, 12 AC_UPDATE_SESSION_ACK,
 * 13 AC_OTPCHECK_REQ, 14 AC_AUX_AUTHENTICATION_REQ, 15 AC_TELEPHONEAUTH_STARTED, 19 AC_IOVATION_CHECK_REQ
 * <p>
 * Java: com.aionemu.loginserver.network.factories.AionPacketHandlerFactory
 *
 * @author -Nemesiss-
 */
namespace aion::loginserver::network::factories::AionPacketHandlerFactory {

/**
 * Reads one packet from given ByteBuffer: consumes the opcode and returns the packet (not read yet) sharing the buffer, or nullptr for unknown
 * opcodes, which are logged ("Unknown packet received from client: opCode=0x.. state=... length=... data=[..]").
 *
 * @throws commons::utils::BufferUnderflowException if the buffer is empty
 */
std::unique_ptr<aion::AionClientPacket> handle(commons::utils::ByteBuffer& data, std::shared_ptr<aion::LoginConnection> client);

} // namespace aion::loginserver::network::factories::AionPacketHandlerFactory
