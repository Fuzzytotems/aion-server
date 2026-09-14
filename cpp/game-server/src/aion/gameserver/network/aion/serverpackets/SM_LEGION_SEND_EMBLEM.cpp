#include "aion/gameserver/network/aion/serverpackets/SM_LEGION_SEND_EMBLEM.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_LEGION_SEND_EMBLEM::SM_LEGION_SEND_EMBLEM(int32_t legionIdValue, model::team::legion::LegionEmblem& emblem, int32_t emblemDataSizeValue,
	std::string_view legionNameValue)
	: AionServerPacket(opcodeOf<SM_LEGION_SEND_EMBLEM>), legionId(legionIdValue), emblemDataSize(emblemDataSizeValue), legionName(legionNameValue) {
	AION_UNPORTED();
}

void SM_LEGION_SEND_EMBLEM::writeImpl(AionConnection* con) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::network::aion::serverpackets
