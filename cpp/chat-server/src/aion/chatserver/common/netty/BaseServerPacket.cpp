#include "aion/chatserver/common/netty/BaseServerPacket.h"

#include <string>

#include "aion/commons/utils/StringUtils.h"

namespace aion::chatserver::common::netty {

void BaseServerPacket::writeS(ChannelBuffer& buf, std::string_view text) {
	if (!text.empty()) {
		for (char16_t c : commons::utils::StringUtils::toUtf16(text))
			buf.putChar(c);
	}
	buf.putChar(u'\0');
}

} // namespace aion::chatserver::common::netty
