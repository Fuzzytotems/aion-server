#include "aion/gameserver/network/aion/serverpackets/SM_LEGION_UPDATE_TITLE.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_LEGION_UPDATE_TITLE::SM_LEGION_UPDATE_TITLE(int32_t playerObjectIdValue, int32_t legionIdValue, std::string_view legionNameValue,
	model::team::legion::LegionRank rankValue)
	: AionServerPacket(opcodeOf<SM_LEGION_UPDATE_TITLE>), playerObjectId(playerObjectIdValue), legionId(legionIdValue), legionName(legionNameValue),
	  rank(rankValue) {
}

void SM_LEGION_UPDATE_TITLE::writeImpl(AionConnection* con) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::network::aion::serverpackets
