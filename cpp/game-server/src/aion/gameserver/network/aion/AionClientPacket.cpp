#include "aion/gameserver/network/aion/AionClientPacket.h"

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/gameserver/network/aion/AionConnection.h"
#include "aion/gameserver/runtime/base/Unported.h"

namespace aion::gameserver::network::aion {

static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.network.aion.AionClientPacket");

AionClientPacket::AionClientPacket(int32_t opcode, const StateSet& validStatesValue) : BaseClientPacket(opcode), validStates(validStatesValue) {
}

void AionClientPacket::run() {
	AION_UNPORTED();
}

void AionClientPacket::sendPacket(AionServerPacket& msg) {
	AION_UNPORTED();
}

std::string AionClientPacket::readS(int32_t characterCount) {
	AION_UNPORTED();
}

bool AionClientPacket::isValid() {
	AION_UNPORTED();
}

} // namespace aion::gameserver::network::aion
