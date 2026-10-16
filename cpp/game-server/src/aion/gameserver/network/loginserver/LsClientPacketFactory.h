#pragma once

#include <cstdint>
#include <memory>
#include <string_view>

#include "aion/commons/utils/ByteBuffer.h"
#include "aion/gameserver/network/loginserver/LoginServerConnection_State.h"
#include "aion/gameserver/network/loginserver/fwd.h"

namespace aion::gameserver::network::loginserver {

/**
 * Creates the packets the login server sends to the game server.
 * <p>
 * C++: Java's reflective PacketInfo map is a constant table in the .cpp (opcode, class, valid states).
 *
 * @author Neon
 */
class LsClientPacketFactory {
public:
	LsClientPacketFactory() = delete;

	/** Java: private static class PacketInfo<T extends LsClientPacket> (erased: a factory function instead of the reflective constructor) */
	class PacketInfo { // fieldmap.toml (K5): the reflective Constructor is a factory function; entries of a constant table
	public:
		std::string_view packetClassName; // points to string literals; entries of a constant table
		std::unique_ptr<LsClientPacket> (*packetFactory)(int32_t opcode) = nullptr;
		/** Java: Set<State> validStates (bit per LoginServerConnection_State ordinal) */
		uint32_t validStates = 0; // fieldmap.toml: immutable EnumSet as a bit set over the two states; entries of a constant table

		bool isValid(LoginServerConnection_State state) const noexcept { return (validStates & (uint32_t{1} << static_cast<uint32_t>(state))) != 0; }

		/** Java: newPacket(opCode, buffer, con) */
		std::unique_ptr<LsClientPacket> newPacket(int32_t opCode, const commons::utils::ByteBuffer& buffer, LoginServerConnection* con) const;

		std::string_view getPacketClassName() const noexcept { return packetClassName; }
	};

	/** @return the packet for the opcode (first byte of data) with buffer and connection attached, or nullptr if unknown or invalid in the state */
	static std::unique_ptr<LsClientPacket> tryCreatePacket(commons::utils::ByteBuffer& data, LoginServerConnection* client);
};

} // namespace aion::gameserver::network::loginserver
