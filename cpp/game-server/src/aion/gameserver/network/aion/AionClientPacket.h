#pragma once

#include <concepts>
#include <cstdint>
#include <string>

#include "aion/commons/network/packet/BaseClientPacket.h"
#include "aion/gameserver/network/aion/AionConnection.h"
#include "aion/gameserver/network/aion/StateSet.h"
#include "aion/gameserver/network/aion/fwd.h"
#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion {

/**
 * Base class for every Aion -> GS Client Packet
 * <p>
 * Hub header (docs/design/hub-headers.md §12). Created by AionClientPacketFactory through the AION_CLIENT_PACKET factories of HandlerRegistry.h
 * (`std::make_unique<CM_X>(opcode, validStates)`), read on the IO strand and executed on the PacketProcessor (runtime-architecture.md §1.1).
 * The connection is held as std::shared_ptr (BaseClientPacket). The header includes AionConnection.h (and so Asio): the direct base class
 * BaseClientPacket<AionConnection> instantiates connectionToString() with its template argument, which needs the complete connection.
 *
 * @author -Nemesiss-
 */
class AionClientPacket : public commons::network::packet::BaseClientPacket<AionConnection> {
private:
	const StateSet validStates; // fieldmap: Java Set<State> is the StateSet value type that HandlerRegistry.h's ClientPacketFactory passes

protected:
	/**
	 * Constructs new client packet instance. ByteBuffer and ClientConnection should be later set manually, after using this constructor.
	 *
	 * @param opcode
	 *          packet id
	 * @param validStates
	 *          connection valid states
	 */
	AionClientPacket(int32_t opcode, const StateSet& validStates);

public:
	/** Runs runImpl() if the packet is still valid, logging exceptions ("Error handling client packet from <connection>: <packet>"). */
	void run() override final;

protected:
	/**
	 * Send new AionServerPacket to connection that is owner of this packet. This method is equivalent to: getConnection().sendPacket(msg);
	 */
	void sendPacket(AionServerPacket& msg);

	/** C++ only: sendPacket for packet temporaries (`sendPacket(SM_X(...))`, hub-headers.md §12) */
	template <std::derived_from<AionServerPacket> P>
	void sendPacket(P&& msg) {
		sendPacket(static_cast<AionServerPacket&>(msg));
	}

	using commons::network::packet::ClientPacketBase::readS;

	/** Reads a fixed size string from the buffer, omitting not present trailing characters for the return value. */
	std::string readS(int32_t characterCount);

public:
	/**
	 * Checks if the packet is still valid for its connection. Java final.
	 *
	 * @return True if packet is still valid and should be processed.
	 */
	bool isValid();

protected:
	int32_t getOpCodeZeroPadding() const override { return 3; }
};

} // namespace aion::gameserver::network::aion
