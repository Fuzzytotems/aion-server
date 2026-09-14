#include "aion/gameserver/network/aion/serverpackets/AbstractHouseInfoPacket.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/model/house/House.h"

namespace aion::gameserver::network::aion::serverpackets {

AbstractHouseInfoPacket::AbstractHouseInfoPacket(int32_t opCode, model::house::House& houseValue) : AionServerPacket(opCode), house(houseValue) {
}

AbstractHouseInfoPacket::~AbstractHouseInfoPacket() = default;

void AbstractHouseInfoPacket::writeCommonInfo() {
	AION_UNPORTED();
}

} // namespace aion::gameserver::network::aion::serverpackets
