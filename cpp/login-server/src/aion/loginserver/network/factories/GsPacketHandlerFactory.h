#pragma once

#include <memory>

#include "aion/commons/utils/ByteBuffer.h"
#include "aion/loginserver/network/gameserver/GsClientPacket.h"

/**
 * Creates the client packet for the opcode at the start of a game server packet, depending on the connection state.
 * <p>
 * Java: com.aionemu.loginserver.network.factories.GsPacketHandlerFactory
 *
 * @author -Nemesiss-
 */
namespace aion::loginserver::network::factories::GsPacketHandlerFactory {

/**
 * Reads one packet from given ByteBuffer: consumes the opcode and returns the packet (not read yet) with connection and buffer set, or nullptr
 * for unknown opcodes, which are logged ("Unknown packet received from Game Server: 0x.. state=...").
 *
 * @throws commons::utils::BufferUnderflowException if the buffer is empty
 */
std::unique_ptr<gameserver::GsClientPacket> handle(commons::utils::ByteBuffer& data, std::shared_ptr<gameserver::GsConnection> client);

} // namespace aion::loginserver::network::factories::GsPacketHandlerFactory
