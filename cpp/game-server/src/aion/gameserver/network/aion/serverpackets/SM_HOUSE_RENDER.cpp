#include "aion/gameserver/network/aion/serverpackets/SM_HOUSE_RENDER.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"
#include "aion/gameserver/model/house/House.h"
#include "aion/gameserver/network/aion/AionServerPacket.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_HOUSE_RENDER::SM_HOUSE_RENDER(model::house::House& house) : AbstractHouseInfoPacket(opcodeOf<SM_HOUSE_RENDER>, house) {
}
void SM_HOUSE_RENDER::writeImpl(AionConnection* con) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::network::aion::serverpackets
