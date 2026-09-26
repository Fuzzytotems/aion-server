#include "aion/gameserver/network/aion/serverpackets/SM_GAMEGUARD.h"

#include <vector>

#include "aion/commons/utils/Exception.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_GAMEGUARD::SM_GAMEGUARD(int32_t sizeValue) : AionServerPacket(opcodeOf<SM_GAMEGUARD>), size(sizeValue) {
}

void SM_GAMEGUARD::writeImpl(AionConnection* con) {
	writeD(size);
	if (size < 0)
		throw commons::utils::IllegalArgumentException("Negative array size: " + std::to_string(size)); // Java: new byte[size] throws NegativeArraySizeException
	writeB(std::vector<uint8_t>(static_cast<size_t>(size)));
}

} // namespace aion::gameserver::network::aion::serverpackets
