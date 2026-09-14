#include "aion/gameserver/network/aion/serverpackets/SM_PLASTIC_SURGERY.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_PLASTIC_SURGERY::SM_PLASTIC_SURGERY(model::gameobjects::player::Player& player, bool isGenderSwitchValue)
	: AionServerPacket(opcodeOf<SM_PLASTIC_SURGERY>), isGenderSwitch(isGenderSwitchValue) {
	AION_UNPORTED();
}

void SM_PLASTIC_SURGERY::writeImpl(AionConnection* con) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::network::aion::serverpackets
