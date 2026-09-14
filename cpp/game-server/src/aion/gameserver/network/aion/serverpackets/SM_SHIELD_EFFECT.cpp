#include "aion/gameserver/network/aion/serverpackets/SM_SHIELD_EFFECT.h"

#include "aion/gameserver/model/siege/SiegeLocation.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"
#include "aion/gameserver/runtime/base/Unported.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_SHIELD_EFFECT::SM_SHIELD_EFFECT(const std::vector<runtime::Ptr<model::siege::SiegeLocation>>& locationsValue)
	: AionServerPacket(opcodeOf<SM_SHIELD_EFFECT>), locations(locationsValue.begin(), locationsValue.end()) {
}

SM_SHIELD_EFFECT::SM_SHIELD_EFFECT(int32_t location)
	: AionServerPacket(opcodeOf<SM_SHIELD_EFFECT>) {
	AION_UNPORTED();
}

SM_SHIELD_EFFECT::~SM_SHIELD_EFFECT() = default;

void SM_SHIELD_EFFECT::writeImpl(AionConnection* con) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::network::aion::serverpackets
