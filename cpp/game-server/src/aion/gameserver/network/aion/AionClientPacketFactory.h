#pragma once

#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "aion/commons/utils/ByteBuffer.h"
#include "aion/gameserver/network/aion/StateSet.h"
#include "aion/gameserver/network/aion/fwd.h"

namespace aion::gameserver::handlers {
struct ClientPacketEntry;
}

namespace aion::gameserver::network::aion {

/**
 * Creates the client packet for the opcode at the start of a decrypted client packet body.
 * <p>
 * C++: Java's static PacketInfo table (250 slots, filled by reflection) is built from two generated tables: ClientPacketInfo.gen.inc (opcode,
 * class name and valid states of the 186 packets, the Java static initializer) and the AION_CLIENT_PACKET registry of HandlerRegistry.h (class
 * name -> factory). The table is built on first use from handlers::clientPacketEntries(). A packet class of the opcode table without a registry
 * entry (its C++ port does not exist yet) is logged once and ignored like an invalid state (C++ only, porting state).
 * <p>
 * Thread-safety: tryCreatePacket runs on IO threads; the table is immutable once published (setEntries is for startup and tests).
 *
 * @author Neon
 */
class AionClientPacketFactory {
public:
	AionClientPacketFactory() = delete;

	/** Java: private static class PacketInfo - one slot of the opcode table */
	// fieldmap.toml (K5): the reflective Constructor is the registry's factory function; entries are immutable once the table is published
	class PacketInfo {
	public:
		/** Java: packetConstructor.getDeclaringClass().getSimpleName() */
		std::string_view packetClassName; // points to string literals of the generated tables; immutable once the table is published
		/** Java: packetConstructor (nullptr: the class has no AION_CLIENT_PACKET registration yet) */
		std::unique_ptr<AionClientPacket> (*packetFactory)(int32_t opcode, const StateSet& validStates) = nullptr;
		/** Java: Set<State> validStates */
		StateSet validStates; // fieldmap.toml: immutable EnumSet as the StateSet value type (HandlerRegistry.h); written before the table is published

		bool isValid(AionConnection_State state) const noexcept { return validStates.contains(state); }

		/** Java: newPacket(opCode, buffer, con) - creates the packet and attaches the buffer and the connection */
		std::unique_ptr<AionClientPacket> newPacket(int32_t opCode, const commons::utils::ByteBuffer& buffer, AionConnection* con) const;

		std::string_view getPacketClassName() const noexcept { return packetClassName; }
	};

	/**
	 * Reads the (obfuscated) opcode, skips the static code and the secondary opcode and creates the packet for the client's current state.
	 *
	 * @return the packet with buffer and connection attached, or nullptr if the opcode is unknown or invalid in the connection's state
	 */
	static std::unique_ptr<AionClientPacket> tryCreatePacket(commons::utils::ByteBuffer& data, AionConnection* client);

	/** C++ only: the table slot of the opcode, nullptr if Java's table has no packet there (tests) */
	static const PacketInfo* getPacketInfo(int32_t opcode);

	/**
	 * C++ only: rebuilds the table with the given registry entries (every entry must outlive the process; default: handlers::clientPacketEntries()).
	 * Startup and tests only: the previous table stays allocated, so concurrent readers remain valid.
	 */
	static void setEntries(std::span<const handlers::ClientPacketEntry> entries);

	/**
	 * C++ only (header request 5a-pre-7): the packet classes a client sent that had no registry entry yet (the "which is not ported yet" warning of
	 * tryCreatePacket), sorted by name, since process start. For the m5a_summary.txt of the check-output mode (docs/design/m5a-plan.md D7).
	 */
	static std::vector<std::string> unportedPacketClassesSeen();
};

} // namespace aion::gameserver::network::aion
