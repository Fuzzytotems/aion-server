#include "aion/gameserver/network/PacketWriteHelper.h"

#include <string>
#include <vector>

#include "aion/commons/utils/Exception.h"
#include "aion/commons/utils/StringUtils.h"

namespace aion::gameserver::network {

void PacketWriteHelper::writeD(commons::utils::ByteBuffer& buf, int32_t value) {
	buf.putInt(value);
}

void PacketWriteHelper::writeH(commons::utils::ByteBuffer& buf, int32_t value) {
	buf.putShort(static_cast<int16_t>(value));
}

void PacketWriteHelper::writeC(commons::utils::ByteBuffer& buf, int32_t value) {
	buf.put(static_cast<int8_t>(value));
}

void PacketWriteHelper::writeDF(commons::utils::ByteBuffer& buf, double value) {
	buf.putDouble(value);
}

void PacketWriteHelper::writeF(commons::utils::ByteBuffer& buf, float value) {
	buf.putFloat(value);
}

void PacketWriteHelper::writeQ(commons::utils::ByteBuffer& buf, int64_t value) {
	buf.putLong(value);
}

void PacketWriteHelper::writeS(commons::utils::ByteBuffer& buf, std::string_view text) {
	if (!text.empty()) {
		const std::u16string utf16 = commons::utils::StringUtils::toUtf16(text);
		for (char16_t c : utf16)
			buf.putChar(c);
	}
	buf.putChar(u'\0');
}

void PacketWriteHelper::writeS(commons::utils::ByteBuffer& buf, std::string_view text, int32_t size) {
	if (text.empty()) { // Java: null writes size zero bytes, "" writes its (zero) characters and size zero bytes: the same bytes
		skip(buf, size);
	} else {
		const std::u16string utf16 = commons::utils::StringUtils::toUtf16(text);
		for (char16_t c : utf16)
			buf.putChar(c);
		skip(buf, size - static_cast<int32_t>(utf16.size() * 2)); // Java: new byte[negative] throws NegativeArraySizeException
	}
}

void PacketWriteHelper::writeB(commons::utils::ByteBuffer& buf, std::span<const uint8_t> data) {
	buf.put(data);
}

void PacketWriteHelper::skip(commons::utils::ByteBuffer& buf, int32_t bytes) {
	if (bytes < 0)
		throw commons::utils::IllegalArgumentException("Negative array size: " + std::to_string(bytes)); // Java: NegativeArraySizeException
	const std::vector<uint8_t> zeros(static_cast<size_t>(bytes));
	buf.put(zeros);
}

void PacketWriteHelper::writeDyeInfo(commons::utils::ByteBuffer& buf, std::optional<int32_t> rgb) {
	if (!rgb) {
		skip(buf, 4);
	} else {
		writeC(buf, 1); // dye status (1 = dyed, 0 = not dyed)
		writeC(buf, (*rgb & 0xFF0000) >> 16); // r
		writeC(buf, (*rgb & 0xFF00) >> 8); // g
		writeC(buf, *rgb & 0xFF); // b
	}
}

PacketWriteHelper::~PacketWriteHelper() = default;

} // namespace aion::gameserver::network
