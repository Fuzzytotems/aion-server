#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "aion/commons/network/packet/BasePacket.h"
#include "aion/commons/utils/ByteBuffer.h"

namespace aion::commons::network::packet {

/**
 * Non-template part of BaseClientPacket: the packet data buffer, read() and the typed read helpers. Use BaseClientPacket&lt;Connection&gt; as
 * the base class of concrete packets; this class exists so that the logic is compiled once and so that PacketProcessor can handle packets of
 * any connection type.
 */
class ClientPacketBase : public BasePacket {
public:
	/** Attaches the packet data. The buffer's [position, limit) is the packet data still to be read. */
	void setBuffer(utils::ByteBuffer buffer) noexcept { buf = std::move(buffer); }

	/**
	 * Reads data from the packet buffer by calling readImpl(). Exceptions thrown by readImpl are logged together with a hex dump of the packet,
	 * and false is returned (the packet should not be executed). If readImpl leaves bytes unread, a warning is logged once per opcode.
	 * <p>
	 * The buffer usually refers to the connection's read buffer, which is only valid during AConnection::processData, so read() must be
	 * called from processData (like all consumers of the Java server do).
	 *
	 * @return true if reading was successful, otherwise false
	 */
	bool read();

	/** @return number of bytes remaining in this packet buffer */
	int32_t getRemainingBytes() const noexcept { return buf.remaining(); }

	/**
	 * Executes this packet (Java: Runnable.run). The default implementation calls runImpl(). Server specific base classes override it to catch
	 * and log exceptions (Java: AionClientPacket.run), but PacketProcessor guards against escaping exceptions as well.
	 */
	virtual void run() { runImpl(); }

protected:
	/** Constructs a new client packet with the given opcode. The buffer must be set later with setBuffer. */
	explicit ClientPacketBase(int32_t opcode) noexcept : BasePacket(opcode) {}
	/** Constructs a new client packet with the given data buffer and opcode. */
	ClientPacketBase(utils::ByteBuffer buffer, int32_t opcode) noexcept : BasePacket(opcode), buf(std::move(buffer)) {}

	/** Data reading implementation. */
	virtual void readImpl() = 0;

	/** Execution of this packet's action. */
	virtual void runImpl() = 0;

	/** @return the owning connection's toString(), or "null", for log messages */
	virtual std::string connectionToString() const = 0;

	// All read helpers log "Missing X for: <packet> (sent from <connection>)" on buffer underflow and return 0 (or the data read so far for
	// readS, or zeroes for readB), like Java.

	/** Reads an int. */
	int32_t readD();
	/** Reads a byte. */
	int8_t readC();
	/** Reads an unsigned byte. */
	int32_t readUC();
	/** Reads a short. */
	int16_t readH();
	/** Reads an unsigned short. */
	int32_t readUH();
	/** Reads a double. */
	double readDF();
	/** Reads a float. */
	float readF();
	/** Reads a long. */
	int64_t readQ();
	/** Reads a UTF-16LE string up to (and consuming) the terminating 0 char and returns it as UTF-8. */
	std::string readS();
	/**
	 * Reads length bytes. On underflow nothing is consumed and a zero-filled vector of the requested length is returned (Java: get(byte[])).
	 *
	 * @throws utils::IllegalArgumentException if length is negative (Java: NegativeArraySizeException)
	 */
	std::vector<uint8_t> readB(int32_t length);

	/** Direct access to the packet data buffer. */
	utils::ByteBuffer& getBuffer() noexcept { return buf; }

private:
	void logMissing(const char* type) const;

	/** ByteBuffer that contains this packet data */
	utils::ByteBuffer buf;
};

/**
 * Base class for every client packet (a packet this process receives), owned by a connection of type TConnection.
 * <p>
 * A packet is created and read in AConnection::processData and then usually handed to a PacketProcessor (as std::unique_ptr) or another
 * executor, which calls run(). The packet keeps its connection alive through a shared_ptr.
 * <p>
 * TConnection must provide {@code std::string toString() const} (every AConnection does). Only setConnection() needs the complete connection
 * type: it stores a function that calls toString(), so the virtual connectionToString() (which MSVC instantiates together with the class)
 * compiles against a forward declaration, and server packet base headers need not include their connection (Asio, windows.h).
 * <p>
 * Java: com.aionemu.commons.network.packet.BaseClientPacket
 *
 * @author -Nemesiss-
 */
template <typename TConnection>
class BaseClientPacket : public ClientPacketBase {
public:
	using Connection = TConnection;

	/** Attaches the client connection to this packet. Needs the complete TConnection. */
	void setConnection(std::shared_ptr<TConnection> connection) noexcept {
		client = std::move(connection);
		clientToString = [](const TConnection& c) { return c.toString(); };
	}

	/** @return connection that is the owner of this packet */
	const std::shared_ptr<TConnection>& getConnection() const noexcept { return client; }

protected:
	explicit BaseClientPacket(int32_t opcode) noexcept : ClientPacketBase(opcode) {}
	BaseClientPacket(utils::ByteBuffer buffer, int32_t opcode) noexcept : ClientPacketBase(std::move(buffer), opcode) {}

	std::string connectionToString() const override { return client ? clientToString(*client) : std::string("null"); }

private:
	/** Owner of this packet. */
	std::shared_ptr<TConnection> client;
	/** TConnection::toString of client, bound by setConnection (the only member that needs the complete connection type) */
	std::string (*clientToString)(const TConnection&) = nullptr;
};

} // namespace aion::commons::network::packet
