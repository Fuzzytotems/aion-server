#include "aion/gameserver/network/aion/serverpackets/SM_HOUSE_UPDATE.h"

#include "aion/gameserver/model/house/House.h"
#include "aion/gameserver/network/aion/AionServerPacket.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_HOUSE_UPDATE::SM_HOUSE_UPDATE(model::house::House& house) : AbstractHouseInfoPacket(opcodeOf<SM_HOUSE_UPDATE>, house) {
}
void SM_HOUSE_UPDATE::writeImpl(AionConnection* con) {
	writeH(1); // unk
	writeH(0);
	writeH(1); // unk (if this is 0 any changed house settings are ignored on client side)
	writeCommonInfo();
}

} // namespace aion::gameserver::network::aion::serverpackets
