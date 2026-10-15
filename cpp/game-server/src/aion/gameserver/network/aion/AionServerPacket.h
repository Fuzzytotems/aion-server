#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <string_view>

#include "aion/commons/network/packet/BaseServerPacket.h"
#include "aion/commons/utils/ByteBuffer.h"
#include "aion/gameserver/network/aion/SerializedBody.h"
#include "aion/gameserver/network/aion/fwd.h"

namespace aion::gameserver::network::aion {

/**
 * Base class for every GS -> Aion Server Packet.
 * <p>
 * Hub header (docs/design/hub-headers.md §12). Server packets are stack temporaries (fieldmap K2, runtime-architecture.md §8): object members
 * are `Ref<X>`, and senders pass them by reference (`PacketSendUtility::sendPacket(player, SM_X(...))`). A packet is serialized eagerly on the
 * sending thread (serialize, replacing Java's lazy `write(con, buffer)` on the IO thread): writeImpl writes through the static write helpers
 * into a thread-local buffer (getBuf()), and the resulting SerializedBody is queued to the connection. A SHARED packet is serialized once per
 * broadcast; the packets whose writeImpl reads the connection override recipients() to return PER_RECIPIENT and are serialized for each
 * recipient (§8.3).
 * <p>
 * Java's no-argument constructor sets the opcode from `ServerPacketsOpcodes.getOpcode(getClass())`; in C++ the dynamic type is not known in the
 * base constructor, so every subclass passes `opcodeOf<SM_X>` (ServerPacketsOpcodes.gen.h) to AionServerPacket(int32_t).
 *
 * @author -Nemesiss-
 */
class AionServerPacket : public commons::network::packet::BaseServerPacket {
public:
	static constexpr int32_t MAX_CLIENT_SUPPORTED_PACKET_SIZE = 8192;
	/** 8192 - 2 (body length) - 2 (opCode) - 1 (staticServerPacketCode) - 2 (opCode flipped bits) */
	static constexpr int32_t MAX_USABLE_PACKET_BODY_SIZE = MAX_CLIENT_SUPPORTED_PACKET_SIZE - 7;

	/** C++ only (runtime-architecture.md §8.2): whether one serialization can be shared by all recipients of a broadcast */
	enum class Recipients : uint8_t { SHARED, PER_RECIPIENT };

	static int32_t byteLengthForString(std::string_view text);

	static int32_t byteLengthForFixedString(int32_t fixedLength);

	~AionServerPacket() override;

protected:
	/** Constructs new server packet with the given opcode (subclasses with a fixed opcode pass `opcodeOf<SM_X>`). */
	explicit AionServerPacket(int32_t opCode);

private:
	/** Write packet opCode and two additional bytes (into the current thread-local buffer) */
	void writeOP();

public:
	/**
	 * C++ only: SHARED by default; overridden (generated list, lint L10) by the packets whose writeImpl reads the connection.
	 */
	virtual Recipients recipients() const noexcept { return Recipients::SHARED; }

	/**
	 * Replaces Java's `write(AionConnection con, ByteBuffer buffer)`: samples the sequence number, writes the opcode header and writeImpl(con)
	 * into the thread-local scratch buffer and copies the body out. Encryption and the length prefix are done by AionConnection::writeData on
	 * the IO strand. Logs a warning if the frame exceeds MAX_CLIENT_SUPPORTED_PACKET_SIZE.
	 *
	 * @param con
	 *          the recipient's connection for PER_RECIPIENT packets, nullptr for a SHARED serialization (dereferencing it then throws
	 *          NullPointerException naming the packet, §8.7)
	 */
	SerializedBody serialize(AionConnection* con);

protected:
	/** Write data that this packet represents to the current buffer. */
	virtual void writeImpl(AionConnection* con) {}

public:
	/** The buffer of the serialization running on this thread (Java: the packet's buf). @throws IllegalStateException outside serialize */
	static commons::utils::ByteBuffer& getBuf();

protected:
	/** Java BaseServerPacket.writeD: writes an int to the current buffer. */
	static void writeD(int32_t value);
	/** Java BaseServerPacket.writeH */
	static void writeH(int32_t value);
	/** Java BaseServerPacket.writeC(int) */
	static void writeC(int32_t value);
	/** Java BaseServerPacket.writeDF */
	static void writeDF(double value);
	/** Java BaseServerPacket.writeF */
	static void writeF(float value);
	/** Java BaseServerPacket.writeQ */
	static void writeQ(int64_t value);
	/** Java BaseServerPacket.writeS(String): UTF-16LE plus the terminating char (Java null: empty) */
	static void writeS(std::string_view text);
	/** Java BaseServerPacket.writeB */
	static void writeB(std::span<const uint8_t> data);

	/**
	 * Write string to buffer with a fixed length. Characters exceeding fixedLength will be truncated. Missing ones will be zero-padded.
	 * The number of written bytes to the buffer is fixedLength * 2 + 2. The additional two bytes are the terminating (zero) char which the client
	 * always requires. One could actually populate that last char normally and it would be displayed on client side, but then following data may get
	 * corrupted.
	 */
	static void writeS(std::string_view text, int32_t fixedLength);

	/**
	 * Writes dye information (dye status + 3 byte RGB value) to the buffer.
	 *
	 * @param rgb
	 *          - may be absent (Java null)
	 */
	static void writeDyeInfo(std::optional<int32_t> rgb);

	int32_t getOpCodeZeroPadding() const override { return 3; }
};

} // namespace aion::gameserver::network::aion
