#include "aion/gameserver/network/aion/serverpackets/SM_HOUSE_OWNER_INFO.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"
#include "aion/gameserver/model/house/House.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_HOUSE_OWNER_INFO::SM_HOUSE_OWNER_INFO(model::gameobjects::player::Player& player) : AionServerPacket(opcodeOf<SM_HOUSE_OWNER_INFO>) {
	AION_UNPORTED();
}

SM_HOUSE_OWNER_INFO::~SM_HOUSE_OWNER_INFO() = default;

void SM_HOUSE_OWNER_INFO::writeImpl(AionConnection* con) {
	AION_UNPORTED();
}

int32_t SM_HOUSE_OWNER_INFO::calculateWeeksUntilNextPay() {
	AION_UNPORTED();
}

} // namespace aion::gameserver::network::aion::serverpackets
