#include "aion/gameserver/network/aion/serverpackets/SM_LEGION_MEMBERLIST.h"

#include "aion/gameserver/configs/network/NetworkConfig.h"
#include "aion/gameserver/model/PlayerClassInfo.h"
#include "aion/gameserver/model/house/House.h"
#include "aion/gameserver/model/team/legion/LegionMember.h"
#include "aion/gameserver/model/templates/housing/HouseAddress.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"
#include "aion/gameserver/network/aion/serverpackets/detail/PacketLookups.h"
#include "aion/gameserver/network/aion/serverpackets/detail/PacketSupport.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_LEGION_MEMBERLIST::SM_LEGION_MEMBERLIST(const std::vector<runtime::Ptr<model::team::legion::LegionMember>>& legionMembersValue, bool isFirstValue,
	bool isLastValue)
	: SM_LEGION_MEMBERLIST(opcodeOf<SM_LEGION_MEMBERLIST>, legionMembersValue, isFirstValue, isLastValue) {
}

SM_LEGION_MEMBERLIST::SM_LEGION_MEMBERLIST(int32_t opCode, const std::vector<runtime::Ptr<model::team::legion::LegionMember>>& legionMembersValue,
	bool isFirstValue, bool isLastValue)
	: AionServerPacket(opCode), isFirst(isFirstValue), isLast(isLastValue), legionMembers(legionMembersValue.begin(), legionMembersValue.end()) {
}

SM_LEGION_MEMBERLIST::~SM_LEGION_MEMBERLIST() = default;

void SM_LEGION_MEMBERLIST::writeImpl(AionConnection* con) {
	int32_t size = static_cast<int32_t>(legionMembers.size());
	writeC(isFirst ? 1 : 0);
	writeH(isLast ? -size : size);
	for (const runtime::Ref<model::team::legion::LegionMember>& legionMember : legionMembers)
		writeLegionMember(*legionMember);
}

void SM_LEGION_MEMBERLIST::writeLegionMember(model::team::legion::LegionMember& legionMember) {
	writeD(legionMember.getObjectId());
	writeS(legionMember.getName());
	writeC(model::getClassId(legionMember.getPlayerClass()));
	writeD(legionMember.getLevel());
	writeC(detail::legionRankId(legionMember.getRank()));
	writeD(legionMember.getWorldId());
	writeC(legionMember.isOnline() ? 1 : 0);
	writeS(legionMember.getSelfIntro());
	writeS(legionMember.getNickname());
	writeD(legionMember.isOnline() ? 0 : legionMember.getLastOnlineEpochSeconds());
	runtime::Ptr<model::house::House> house = detail::findActiveHouse(legionMember.getObjectId());
	writeD(house == nullptr ? 0 : house->getAddress()->getId());
	writeD(house == nullptr ? 0 : detail::houseDoorStateId(house->getDoorState()));
	writeD(configs::network::NetworkConfig::GAMESERVER_ID.load()); // displays server number for each away player in region field
}

} // namespace aion::gameserver::network::aion::serverpackets
