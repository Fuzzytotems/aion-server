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

} // namespace

bool ClientPacketBase::read() {
	int32_t startPos = buf.position();
	try {
		readImpl();

		if (getRemainingBytes() > 0 && addPartiallyReadPacket(getOpCode()))
			log().warn(toString() + " was not fully read! Last " + std::to_string(getRemainingBytes()) + " bytes were not read from buffer:\n" +
							 utils::NetworkUtils::toHex(buf, startPos, buf.limit()));

		return true;
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

void ClientPacketBase::logMissing(const char* type) const {
	log().error(fmt::format("Missing {} for: {} (sent from {})", type, toString(), connectionToString()));
}

int32_t ClientPacketBase::readD() {
	try {
		return buf.getInt();
	} catch (const utils::BufferUnderflowException&) {
		logMissing("D");
	}
	return 0;
}

int8_t ClientPacketBase::readC() {
	try {
		return buf.get();
	} catch (const utils::BufferUnderflowException&) {
		logMissing("C");
	}
	return 0;
}

int32_t ClientPacketBase::readUC() {
	try {
		return static_cast<uint8_t>(buf.get());
	} catch (const utils::BufferUnderflowException&) {
		logMissing("C");
	}
	return 0;
}

int16_t ClientPacketBase::readH() {
	try {
		return buf.getShort();
	} catch (const utils::BufferUnderflowException&) {
		logMissing("H");
	}
	return 0;
}

int32_t ClientPacketBase::readUH() {
	try {
		return static_cast<uint16_t>(buf.getShort());
	} catch (const utils::BufferUnderflowException&) {
		logMissing("H");
	}
	return 0;
}

double ClientPacketBase::readDF() {
	try {
		return buf.getDouble();
	} catch (const utils::BufferUnderflowException&) {
		logMissing("DF");
	}
	return 0;
}

float ClientPacketBase::readF() {
	try {
		return buf.getFloat();
	} catch (const utils::BufferUnderflowException&) {
		logMissing("F");
	}
	return 0;
}

int64_t ClientPacketBase::readQ() {
	try {
		return buf.getLong();
	} catch (const utils::BufferUnderflowException&) {
		logMissing("Q");
	}
	return 0;
}

std::string ClientPacketBase::readS() {
	std::u16string sb;
	try {
		char16_t ch;
		while ((ch = buf.getChar()) != 0)
			sb += ch;
	} catch (const utils::BufferUnderflowException&) {
		logMissing("S");
	}
	return utils::StringUtils::toUtf8(sb);
}

std::vector<uint8_t> ClientPacketBase::readB(int32_t length) {
	if (length < 0)
		throw utils::IllegalArgumentException(fmt::format("Negative array size: {}", length));
	std::vector<uint8_t> result(static_cast<size_t>(length));
	try {
		buf.get(result);
	} catch (const utils::BufferUnderflowException&) {
		logMissing("byte[]");
	}
	return result;
}

} // namespace aion::commons::network::packet
