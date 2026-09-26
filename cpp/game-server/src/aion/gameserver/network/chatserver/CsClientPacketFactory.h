#pragma once

#include <cstdint>
#include <memory>
#include <string_view>

#include "aion/commons/utils/ByteBuffer.h"
#include "aion/gameserver/network/chatserver/ChatServerConnection_State.h"
#include "aion/gameserver/network/chatserver/fwd.h"

namespace aion::gameserver::network::chatserver {

/**
 * Creates the packets the chat server sends to the game server. C++: the reflective PacketInfo map is a constant table in the .cpp.
 *
 * @author Neon
 */
class CsClientPacketFactory {
public:
	CsClientPacketFactory() = delete;

	/** Java: private static class PacketInfo<T extends CsClientPacket> (erased) */
	class PacketInfo { // fieldmap.toml (K5): the reflective Constructor is a factory function; entries of a constant table
	public:
		std::string_view packetClassName; // points to string literals; entries of a constant table
		std::unique_ptr<CsClientPacket> (*packetFactory)(int32_t opcode) = nullptr;
		/** Java: Set<State> validStates (bit per ChatServerConnection_State ordinal) */
		uint32_t validStates = 0; // fieldmap.toml: immutable EnumSet as a bit set over the two states; entries of a constant table

		bool isValid(ChatServerConnection_State state) const noexcept { return (validStates & (uint32_t{1} << static_cast<uint32_t>(state))) != 0; }

		std::unique_ptr<CsClientPacket> newPacket(int32_t opCode, const commons::utils::ByteBuffer& buffer, ChatServerConnection* con) const;

		std::string_view getPacketClassName() const noexcept { return packetClassName; }
	};

	static std::unique_ptr<CsClientPacket> tryCreatePacket(commons::utils::ByteBuffer& data, ChatServerConnection* client);
};

} // namespace aion::gameserver::network::chatserver
