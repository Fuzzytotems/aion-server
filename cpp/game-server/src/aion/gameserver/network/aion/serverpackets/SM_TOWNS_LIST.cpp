#include "aion/gameserver/network/aion/serverpackets/SM_TOWNS_LIST.h"

#include "aion/gameserver/model/town/Town.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"
#include "aion/gameserver/runtime/base/Unported.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_TOWNS_LIST::SM_TOWNS_LIST(const std::unordered_map<int32_t, runtime::Ptr<model::town::Town>>& townsValue)
	: AionServerPacket(opcodeOf<SM_TOWNS_LIST>), towns(townsValue.begin(), townsValue.end()) {
}

SM_TOWNS_LIST::~SM_TOWNS_LIST() = default;

void SM_TOWNS_LIST::writeImpl(AionConnection* con) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::network::aion::serverpackets
