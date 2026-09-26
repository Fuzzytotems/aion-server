#include "aion/gameserver/network/aion/serverpackets/SM_LEGION_UPDATE_EMBLEM.h"

#include "aion/gameserver/model/team/legion/LegionEmblem.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"
#include "aion/gameserver/network/aion/serverpackets/detail/PacketSupport.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_LEGION_UPDATE_EMBLEM::SM_LEGION_UPDATE_EMBLEM(int32_t legionIdValue, model::team::legion::LegionEmblem& emblem)
	: AionServerPacket(opcodeOf<SM_LEGION_UPDATE_EMBLEM>), legionId(legionIdValue) {
	emblemId = emblem.getEmblemId();
	color_a = emblem.getColor_a();
	color_r = emblem.getColor_r();
	color_g = emblem.getColor_g();
	color_b = emblem.getColor_b();
	emblemType = detail::legionEmblemTypeValue(emblem.getEmblemType());
}

void SM_LEGION_UPDATE_EMBLEM::writeImpl(AionConnection* con) {
	writeD(legionId);
	writeC(emblemId);
	writeC(emblemType);
	writeC(color_a);
	writeC(color_r);
	writeC(color_g);
	writeC(color_b);
}

} // namespace aion::gameserver::network::aion::serverpackets
