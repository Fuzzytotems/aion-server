#include "aion/gameserver/network/aion/serverpackets/SM_LEGION_SEND_EMBLEM.h"

#include "aion/gameserver/model/team/legion/LegionEmblem.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"
#include "aion/gameserver/network/aion/serverpackets/detail/PacketSupport.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_LEGION_SEND_EMBLEM::SM_LEGION_SEND_EMBLEM(int32_t legionIdValue, model::team::legion::LegionEmblem& emblem, int32_t emblemDataSizeValue,
	std::string_view legionNameValue)
	: AionServerPacket(opcodeOf<SM_LEGION_SEND_EMBLEM>), legionId(legionIdValue), emblemDataSize(emblemDataSizeValue), legionName(legionNameValue) {
	emblemId = emblem.getEmblemId();
	emblemType = detail::legionEmblemTypeValue(emblem.getEmblemType());
	color_a = emblem.getColor_a();
	color_r = emblem.getColor_r();
	color_g = emblem.getColor_g();
	color_b = emblem.getColor_b();
}

void SM_LEGION_SEND_EMBLEM::writeImpl(AionConnection* con) {
	writeD(legionId);
	writeC(emblemId);
	writeC(emblemType);
	writeD(emblemDataSize);
	writeC(color_a);
	writeC(color_r);
	writeC(color_g);
	writeC(color_b);
	writeS(legionName);
	writeC(0x01);
}

} // namespace aion::gameserver::network::aion::serverpackets
