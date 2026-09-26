#include "aion/gameserver/network/aion/serverpackets/SM_TOWNS_LIST.h"

#include "aion/gameserver/model/town/Town.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"
#include "aion/gameserver/network/aion/serverpackets/detail/PacketSupport.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_TOWNS_LIST::SM_TOWNS_LIST(const std::unordered_map<int32_t, runtime::Ptr<model::town::Town>>& townsValue)
	: AionServerPacket(opcodeOf<SM_TOWNS_LIST>), towns(townsValue.begin(), townsValue.end()) {
}

SM_TOWNS_LIST::~SM_TOWNS_LIST() = default;

void SM_TOWNS_LIST::writeImpl(AionConnection* con) {
	writeH(static_cast<int32_t>(towns.size()));
	for (const auto& [id, town] : detail::javaHashMapOrder(towns)) { // Java iterates the HashMap values (TownDAO.load, Town.broadcastUpdate)
		writeD(town->getId());
		writeD(town->getLevel());
		// Java: (int) (town.getLevelUpDate().getTime() / 1000)
		writeD(static_cast<int32_t>(detail::unbox(town->getLevelUpDate(), "Town.getLevelUpDate()").time_since_epoch().count() / 1000));
	}
}

} // namespace aion::gameserver::network::aion::serverpackets
