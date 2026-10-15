#include "aion/gameserver/model/team/legion/LegionRankInfo.h"
#include "aion/gameserver/network/aion/serverpackets/SM_LEGION_UPDATE_TITLE.h"

#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"
#include "aion/gameserver/network/aion/serverpackets/detail/PacketSupport.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_LEGION_UPDATE_TITLE::SM_LEGION_UPDATE_TITLE(int32_t playerObjectIdValue, int32_t legionIdValue, std::string_view legionNameValue,
	model::team::legion::LegionRank rankValue)
	: AionServerPacket(opcodeOf<SM_LEGION_UPDATE_TITLE>), playerObjectId(playerObjectIdValue), legionId(legionIdValue), legionName(legionNameValue),
	  rank(rankValue) {
}

void SM_LEGION_UPDATE_TITLE::writeImpl(AionConnection* con) {
	writeD(playerObjectId);
	writeD(legionId);
	writeS(legionName);
	writeC(model::team::legion::getRankId(rank));
}

} // namespace aion::gameserver::network::aion::serverpackets
