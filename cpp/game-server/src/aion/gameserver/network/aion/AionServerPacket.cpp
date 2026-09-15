#include "aion/gameserver/network/aion/AionServerPacket.h"

#include <atomic>
#include <cstddef>
#include <deque>
#include <string>
#include <vector>

#include "aion/commons/utils/Exception.h"
#include "aion/commons/utils/StringUtils.h"
#include "aion/gameserver/network/Crypt.h"
#include "aion/gameserver/runtime/base/Finally.h"

namespace aion::gameserver::network::aion {

namespace {

/**
 * Capacity of a scratch buffer: the AionConnection write buffer (8192 * 4 bytes, Java's AConnection write buffer) minus the 2-byte frame length
 * that AionConnection::writeData prepends. A packet that does not fit throws BufferOverflowException while it is written, like Java's write
 * into the connection's write buffer.
 */
constexpr int32_t SCRATCH_CAPACITY = 8192 * 4 - 2;

/**
 * The serializations running on this thread (C++ only, runtime-architecture.md §8.2). A writeImpl may serialize another packet (a nested
 * serialization gets its own buffer), so the buffers form a stack; std::deque keeps references to lower buffers stable.
 */
struct SerializationStack {
	std::deque<commons::utils::ByteBuffer> buffers;
	size_t depth = 0;
};

// confined: per-thread scratch buffers of the running serializations, never shared (no Ref or Ptr inside)
thread_local SerializationStack serializationStack;

/** Serialization sequence numbers (runtime-architecture.md §8.4); the first packet gets 1 */
// lint: L14 unsigned sequence counter of the packet kernel (runtime-architecture.md §8.4), no game state
std::atomic<uint64_t> serializationSequence{0};

} // namespace

int32_t AionServerPacket::byteLengthForString(std::string_view text) {
	if (text.empty())
		return 2;
	return (commons::utils::StringUtils::utf16Length(text) + 1) * 2;
}

int32_t AionServerPacket::byteLengthForFixedString(int32_t fixedLength) {
	return (fixedLength + 1) * 2;
}

AionServerPacket::AionServerPacket(int32_t opCode) : BaseServerPacket(opCode) {
}

AionServerPacket::~AionServerPacket() = default;

void AionServerPacket::writeOP() {
	commons::utils::ByteBuffer& buf = getBuf();
	// obfuscate packet id
	int32_t op = Crypt::encodeServerPacketOpcode(getOpCode());
	buf.putShort(static_cast<int16_t>(op));
	// put static server packet code
	buf.put(Crypt::staticServerPacketCode);
	// for checksum?
	buf.putShort(static_cast<int16_t>(~op));
}

SerializedBody AionServerPacket::serialize(AionConnection* con) {
	SerializedBody body;
	body.seq = serializationSequence.fetch_add(1, std::memory_order_acq_rel) + 1;
	body.opCode = getOpCode();

	SerializationStack& stack = serializationStack;
	const size_t index = stack.depth;
	if (stack.buffers.size() <= index)
		stack.buffers.push_back(commons::utils::ByteBuffer::allocate(SCRATCH_CAPACITY));
	stack.buffers[index].clear();
	stack.depth++;
	auto popBuffer = runtime::finally([&stack]() noexcept { stack.depth--; });

	// Java write(con, buffer): the length placeholder is written by AionConnection::writeData
	writeOP();
	writeImpl(con);

	const commons::utils::ByteBuffer& buf = stack.buffers[index];
	body.bytes = std::make_shared<const std::vector<uint8_t>>(buf.data(), buf.data() + buf.position());
	return body;
}

commons::utils::ByteBuffer& AionServerPacket::getBuf() {
	SerializationStack& stack = serializationStack;
	if (stack.depth == 0)
		throw commons::utils::IllegalStateException("AionServerPacket::getBuf() called outside AionServerPacket::serialize");
	return stack.buffers[stack.depth - 1];
}

void AionServerPacket::writeD(int32_t value) {
	BaseServerPacket::writeD(getBuf(), value);
}

void AionServerPacket::writeH(int32_t value) {
	BaseServerPacket::writeH(getBuf(), value);
}

void AionServerPacket::writeC(int32_t value) {
	BaseServerPacket::writeC(getBuf(), value);
}

void AionServerPacket::writeDF(double value) {
	BaseServerPacket::writeDF(getBuf(), value);
}

void AionServerPacket::writeF(float value) {
	BaseServerPacket::writeF(getBuf(), value);
}

void AionServerPacket::writeQ(int64_t value) {
	BaseServerPacket::writeQ(getBuf(), value);
}

void AionServerPacket::writeS(std::string_view text) {
	BaseServerPacket::writeS(getBuf(), text);
}

void AionServerPacket::writeB(std::span<const uint8_t> data) {
	BaseServerPacket::writeB(getBuf(), data);
}

void AionServerPacket::writeS(std::string_view text, int32_t fixedLength) {
	commons::utils::ByteBuffer& buf = getBuf();
	if (text.empty()) {
		const std::vector<uint8_t> zeros(static_cast<size_t>(byteLengthForFixedString(fixedLength)));
		buf.put(zeros);
	} else {
		const std::u16string utf16 = commons::utils::StringUtils::toUtf16(text);
		for (int32_t i = 0; i < fixedLength; i++)
			buf.putChar(static_cast<size_t>(i) < utf16.size() ? utf16[static_cast<size_t>(i)] : u'\0');
		buf.putChar(u'\0');
	}
}

void AionServerPacket::writeDyeInfo(std::optional<int32_t> rgb) {
	if (!rgb) {
		const uint8_t zeros[4]{};
		writeB(zeros);
	} else {
		writeC(1); // dye status (1 = dyed, 0 = not dyed)
		writeC((*rgb & 0xFF0000) >> 16); // r
		writeC((*rgb & 0xFF00) >> 8); // g
		writeC(*rgb & 0xFF); // b
	}
}

} // namespace aion::gameserver::network::aion
