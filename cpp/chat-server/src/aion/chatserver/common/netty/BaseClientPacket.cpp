#include "aion/chatserver/common/netty/BaseClientPacket.h"

#include <fmt/format.h>

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/commons/utils/Exception.h"
#include "aion/commons/utils/NetworkUtils.h"
#include "aion/commons/utils/StringUtils.h"

namespace aion::chatserver::common::netty {

namespace {

const commons::logging::Logger& log() {
	static const auto* logger = new commons::logging::Logger(commons::logging::LoggerFactory::getLogger("com.aionemu.chatserver.common.netty.BaseClientPacket"));
	return *logger;
}

/** Java: NetworkUtils.toHex(buf.toByteBuffer(startPos, buf.writerIndex() - startPos)) */
std::string dump(const ChannelBuffer& buf, int32_t startPos) {
	return commons::utils::NetworkUtils::toHex(buf.slice(startPos, buf.limit() - startPos));
}

} // namespace

bool BaseClientPacket::read() {
	int32_t startPos = buf.position();
	try {
		readImpl();
		if (getRemainingBytes() > 0)
			log().warn("{} was not fully read! Last {} bytes were not read from buffer: \n{}", toString(), getRemainingBytes(), dump(buf, startPos));
		return true;
	} catch (...) {
		try {
			std::string msg = "Reading failed for packet " + toString() + ". Buffer Info";
			if (getRemainingBytes() > 0)
				msg += " (last " + std::to_string(getRemainingBytes()) + " bytes were not read)";
			msg += ":\n" + dump(buf, startPos);
			log().errorCurrentException(msg);
		} catch (...) {
			// logging must not turn a read failure into an exception
		}
		return false;
	}
}

void BaseClientPacket::run() {
	try {
		runImpl();
	} catch (...) {
		try {
			log().errorCurrentException("Running failed for packet " + toString());
		} catch (...) {
		}
	}
}

void BaseClientPacket::logMissing(const char* type) const {
	log().error("Missing {} for: {}", type, toString());
}

int32_t BaseClientPacket::readD() {
	try {
		return buf.getInt();
	} catch (const commons::utils::BufferUnderflowException&) {
		logMissing("D");
	}
	return 0;
}

int32_t BaseClientPacket::readC() {
	try {
		return static_cast<uint8_t>(buf.get());
	} catch (const commons::utils::BufferUnderflowException&) {
		logMissing("C");
	}
	return 0;
}

int32_t BaseClientPacket::readH() {
	try {
		return static_cast<uint16_t>(buf.getShort());
	} catch (const commons::utils::BufferUnderflowException&) {
		logMissing("H");
	}
	return 0;
}

double BaseClientPacket::readDF() {
	try {
		return buf.getDouble();
	} catch (const commons::utils::BufferUnderflowException&) {
		logMissing("DF");
	}
	return 0;
}

float BaseClientPacket::readF() {
	try {
		return buf.getFloat();
	} catch (const commons::utils::BufferUnderflowException&) {
		logMissing("F");
	}
	return 0;
}

int64_t BaseClientPacket::readQ() {
	try {
		return buf.getLong();
	} catch (const commons::utils::BufferUnderflowException&) {
		logMissing("Q");
	}
	return 0;
}

std::string BaseClientPacket::readS() {
	std::u16string sb;
	try {
		char16_t ch;
		while ((ch = buf.getChar()) != 0)
			sb += ch;
	} catch (const commons::utils::BufferUnderflowException&) {
		logMissing("S");
	}
	return commons::utils::StringUtils::toUtf8(sb);
}

std::vector<uint8_t> BaseClientPacket::readB(int32_t length) {
	if (length < 0)
		throw commons::utils::IllegalArgumentException(fmt::format("Negative array size: {}", length));
	std::vector<uint8_t> result(static_cast<size_t>(length));
	try {
		buf.get(result);
	} catch (const commons::utils::BufferUnderflowException&) {
		logMissing("byte[]");
	}
	return result;
}

} // namespace aion::chatserver::common::netty
