#include "aion/gameserver/network/aion/serverpackets/SM_SHIELD_EFFECT.h"

#include "aion/gameserver/model/siege/SiegeLocation.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"
#include "aion/gameserver/network/aion/serverpackets/detail/PacketLookups.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_SHIELD_EFFECT::SM_SHIELD_EFFECT(const std::vector<runtime::Ptr<model::siege::SiegeLocation>>& locationsValue)
	: AionServerPacket(opcodeOf<SM_SHIELD_EFFECT>), locations(locationsValue.begin(), locationsValue.end()) {
}

SM_SHIELD_EFFECT::SM_SHIELD_EFFECT(int32_t location)
	: AionServerPacket(opcodeOf<SM_SHIELD_EFFECT>) {
	locations.emplace_back(detail::getSiegeLocation(location)); // Java adds null for an unknown location (writeImpl throws)
}

SM_SHIELD_EFFECT::~SM_SHIELD_EFFECT() = default;

void SM_SHIELD_EFFECT::writeImpl(AionConnection* con) {
	writeH(static_cast<int32_t>(locations.size()));
	for (const runtime::Ref<model::siege::SiegeLocation>& loc : locations) {
		writeD(loc->getLocationId());
		writeC(loc->isUnderShield() ? 1 : 0);
	}
}

} // namespace aion::gameserver::network::aion::serverpackets
