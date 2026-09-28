#pragma once

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "aion/chatserver/common/netty/AbstractPacket.h"
#include "aion/chatserver/common/netty/ChannelBuffer.h"

namespace aion::chatserver::common::netty {

/**
 * Base class of the packets received from Aion clients: the frame buffer and the little endian read helpers.
 * <p>
 * Unlike commons' BaseClientPacket, readC and readH return unsigned values, the "Missing X" errors do not name the connection, and the
 * "not fully read" warning is logged for every such packet, not once per opcode.
 * <p>
 * Java: com.aionemu.chatserver.common.netty.BaseClientPacket
 */
class BaseClientPacket : public AbstractPacket {
public:
	/** @return number of bytes left to read */
	int32_t getRemainingBytes() const noexcept { return buf.remaining(); }

	/**
	 * Perform packet read: calls readImpl() and logs a warning with a hex dump if bytes were left unread. An exception of readImpl is logged with
	 * a hex dump ("Reading failed for packet ...").
	 *
	 * @return false if readImpl threw
	 */
	bool read();

	/** Perform packet action: calls runImpl(), logging any exception ("Running failed for packet ..."). */
	void run();

protected:
	/** @param channelBuffer the frame, positioned after the opcode */
	BaseClientPacket(ChannelBuffer channelBuffer, int8_t opCode) noexcept : AbstractPacket(opCode), buf(std::move(channelBuffer)) {}

	virtual void readImpl() = 0;

	virtual void runImpl() = 0;

	// On a buffer underflow the read helpers log "Missing X for: <packet>", consume nothing and return 0 (readS: the chars read so far, readB: a
	// zero-filled array of the requested length), like Java.

	/** Read int from this packet buffer. */
	int32_t readD();

	/** Read byte from this packet buffer (unsigned, 0-255). */
	int32_t readC();

	/** Read short from this packet buffer (unsigned, 0-65535). */
	int32_t readH();

	/** Read double from this packet buffer. */
	double readDF();

	/** Read float from this packet buffer. */
	float readF();

	/** Read long from this packet buffer. */
	int64_t readQ();

	/** Read String from this packet buffer: UTF-16LE chars up to (and consuming) a 0 char, returned as UTF-8. */
	std::string readS();

	/**
	 * Read n bytes from this packet buffer, n = length.
	 * @throws commons::utils::IllegalArgumentException if length is negative (Java: NegativeArraySizeException)
	 */
	std::vector<uint8_t> readB(int32_t length);

private:
	void logMissing(const char* type) const;

	ChannelBuffer buf;
};

} // namespace aion::chatserver::common::netty
