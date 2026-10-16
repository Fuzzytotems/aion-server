#pragma once

#include <memory>

#include "aion/chatserver/network/gameserver/GsClientPacket.h"
#include "aion/commons/utils/ByteBuffer.h"

/**
 * Creates the client packet for the opcode at the start of a game server packet, depending on the connection state.
 * <p>
 * Java: com.aionemu.chatserver.network.factories.GsPacketHandlerFactory
 *
 * @author -Nemesiss-
 */
namespace aion::chatserver::network::factories::GsPacketHandlerFactory {

/**
 * Consumes the opcode and returns the packet (not read yet) with connection and buffer set, or nullptr for an opcode unknown in the
 * connection's state, which is logged ("Unknown packet received from Game Server: 0x.. state ...").
 */
std::unique_ptr<gameserver::GsClientPacket> handle(commons::utils::ByteBuffer& data, std::shared_ptr<gameserver::GsConnection> client);

} // namespace aion::chatserver::network::factories::GsPacketHandlerFactory
