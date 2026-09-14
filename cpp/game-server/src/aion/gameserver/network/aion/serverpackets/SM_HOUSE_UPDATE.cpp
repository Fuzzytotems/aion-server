#include "aion/gameserver/network/aion/serverpackets/SM_HOUSE_UPDATE.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"
#include "aion/gameserver/model/house/House.h"
#include "aion/gameserver/network/aion/AionServerPacket.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_HOUSE_UPDATE::SM_HOUSE_UPDATE(model::house::House& house) : AbstractHouseInfoPacket(opcodeOf<SM_HOUSE_UPDATE>, house) {
}
void SM_HOUSE_UPDATE::writeImpl(AionConnection* con) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::network::aion::serverpackets
