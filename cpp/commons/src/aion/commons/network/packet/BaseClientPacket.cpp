#include "aion/commons/network/packet/BaseClientPacket.h"

#include <mutex>
#include <unordered_set>

#include <fmt/format.h>

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/commons/utils/NetworkUtils.h"
#include "aion/commons/utils/StringUtils.h"

namespace aion::commons::network::packet {

namespace {

// Intentionally leaked and created on first use, so the network classes can be constructed and destroyed as statics (Java: static fields) without
// depending on the initialization or destruction order of namespace scope statics.
const logging::Logger& log() {
	static const auto* logger = new logging::Logger(logging::LoggerFactory::getLogger("com.aionemu.commons.network.packet.BaseClientPacket"));
	return *logger;
}

/**
 * Opcodes of packets for which the "was not fully read" warning was already logged (Java: ConcurrentHashMap.newKeySet()).
 * @return true if the opcode was not in the set yet
 */
bool addPartiallyReadPacket(int32_t opcode) {
	static std::mutex mutex;
	static std::unordered_set<int32_t> partiallyReadPackets;
	std::lock_guard lock(mutex);
	return partiallyReadPackets.insert(opcode).second;
}

/**
 * Opcodes of packets for which the "is shorter than expected" warning was already logged (Java: tooShortPackets, ConcurrentHashMap.newKeySet()).
 * @return true if the opcode was not in the set yet
 */
bool addTooShortPacket(int32_t opcode) {
	static std::mutex mutex;
	static std::unordered_set<int32_t> tooShortPackets;
	std::lock_guard lock(mutex);
	return tooShortPackets.insert(opcode).second;
}

} // namespace

bool ClientPacketBase::read() {
	int32_t startPos = buf.position();
	try {
		readImpl();

		if (getRemainingBytes() > 0 && addPartiallyReadPacket(getOpCode()))
			log().warn("{} was not fully read! Last {} bytes were not read from buffer:\n{}", toString(), getRemainingBytes(),
				utils::NetworkUtils::toHex(buf, startPos, buf.limit()));

		return true;
	} catch (const utils::BufferUnderflowException&) {
		try {
			if (addTooShortPacket(getOpCode()))
				log().warn(fmt::format("{} from {} is shorter than expected:\n{}", toString(), connectionToString(),
					utils::NetworkUtils::toHex(buf, startPos, buf.limit())));
		} catch (...) {
			// logging must not turn a read failure into an exception
		}
		return false;
	} catch (...) {
		try {
			std::string msg = "Reading failed for packet " + toString() + ". Buffer Info";
			if (getRemainingBytes() > 0)
				msg += " (last " + std::to_string(getRemainingBytes()) + " bytes were not read)";
			msg += ":\n" + utils::NetworkUtils::toHex(buf, startPos, buf.limit());
			log().errorCurrentException(msg);
		} catch (...) {
			// logging must not turn a read failure into an exception
		}
		return false;
	}
}

int32_t ClientPacketBase::readD() {
	return buf.getInt();
}

int8_t ClientPacketBase::readC() {
	return buf.get();
}

int32_t ClientPacketBase::readUC() {
	return static_cast<uint8_t>(buf.get());
}

int16_t ClientPacketBase::readH() {
	return buf.getShort();
}

int32_t ClientPacketBase::readUH() {
	return static_cast<uint16_t>(buf.getShort());
}

double ClientPacketBase::readDF() {
	return buf.getDouble();
}

float ClientPacketBase::readF() {
	return buf.getFloat();
}

int64_t ClientPacketBase::readQ() {
	return buf.getLong();
}

std::string ClientPacketBase::readS() {
	std::u16string sb;
	char16_t ch;
	while ((ch = buf.getChar()) != 0)
		sb += ch;
	return utils::StringUtils::toUtf8(sb);
}

std::vector<uint8_t> ClientPacketBase::readB(int32_t length) {
	if (length < 0 || length > buf.remaining())
		throw utils::BufferUnderflowException();
	std::vector<uint8_t> result(static_cast<size_t>(length));
	buf.get(result);
	return result;
}

} // namespace aion::commons::network::packet
