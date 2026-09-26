#include "aion/gameserver/network/aion/AionClientPacket.h"

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/commons/utils/StringUtils.h"
#include "aion/gameserver/network/aion/AionConnection.h"
#include "aion/gameserver/network/aion/AionServerPacket.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion {

static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.network.aion.AionClientPacket");

AionClientPacket::AionClientPacket(int32_t opcode, const StateSet& validStatesValue) : BaseClientPacket(opcode), validStates(validStatesValue) {
}

void AionClientPacket::run() {
	try {
		if (isValid()) // run only if packet is still valid (connection state didn't change, for example due to logout)
			runImpl();
	} catch (...) {
		log.errorCurrentException("Error handling client packet from " + connectionToString() + ": " + toString());
	}
}

void AionClientPacket::sendPacket(AionServerPacket& msg) {
	getConnection()->sendPacket(msg);
}

std::string AionClientPacket::readS(int32_t characterCount) {
	std::string string = readS(); // read byte length = characters * 2 + 2
	const int32_t length = commons::utils::StringUtils::utf16Length(string);
	if (length < characterCount)
		readB((characterCount - length) * 2);
	return string;
}

bool AionClientPacket::isValid() {
	return validStates.contains(getConnection()->getState());
}

} // namespace aion::gameserver::network::aion
