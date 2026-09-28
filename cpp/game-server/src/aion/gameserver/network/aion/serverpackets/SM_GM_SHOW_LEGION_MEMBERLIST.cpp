#include "aion/gameserver/network/aion/serverpackets/SM_GM_SHOW_LEGION_MEMBERLIST.h"

#include "aion/gameserver/model/GenderInfo.h"
#include "aion/gameserver/model/PlayerClassInfo.h"
#include "aion/gameserver/model/gameobjects/player/PlayerCommonData.h"
#include "aion/gameserver/model/team/legion/LegionMember.h"
#include "aion/gameserver/network/aion/AionServerPacket.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"
#include "aion/gameserver/network/aion/serverpackets/detail/PacketSupport.h"
#include "aion/gameserver/services/player/PlayerService.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_GM_SHOW_LEGION_MEMBERLIST::SM_GM_SHOW_LEGION_MEMBERLIST(const std::vector<runtime::Ptr<model::team::legion::LegionMember>>& legionMembers,
	bool isFirst, bool isLast)
	: SM_LEGION_MEMBERLIST(opcodeOf<SM_GM_SHOW_LEGION_MEMBERLIST>, legionMembers, isFirst, isLast) {
}
void SM_GM_SHOW_LEGION_MEMBERLIST::writeLegionMember(model::team::legion::LegionMember& legionMember) {
	writeD(legionMember.getObjectId());
	writeS(legionMember.getName());
	writeC(model::getClassId(legionMember.getPlayerClass()));
	writeC(model::getGenderId(services::player::PlayerService::getOrLoadPlayerCommonData(legionMember.getObjectId())->getGender()));
	writeD(legionMember.getLevel());
	writeC(detail::legionRankId(legionMember.getRank()));
	writeD(legionMember.getWorldId());
	writeC(legionMember.isOnline() ? 1 : 0);
	writeS(legionMember.getSelfIntro());
	writeS(legionMember.getNickname());
	writeD(legionMember.isOnline() ? 0 : legionMember.getLastOnlineEpochSeconds());
}

} // namespace aion::gameserver::network::aion::serverpackets
