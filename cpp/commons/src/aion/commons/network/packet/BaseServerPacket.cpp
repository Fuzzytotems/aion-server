#include "aion/commons/network/packet/BaseServerPacket.h"

#include <string>

#include "aion/commons/utils/StringUtils.h"

namespace aion::commons::network::packet {

void BaseServerPacket::writeS(utils::ByteBuffer& buf, std::string_view text) {
	if (!text.empty()) {
		std::u16string utf16 = utils::StringUtils::toUtf16(text);
		for (char16_t c : utf16)
			buf.putChar(c);
	}
	buf.putChar(u'\0');
}

} // namespace aion::commons::network::packet
