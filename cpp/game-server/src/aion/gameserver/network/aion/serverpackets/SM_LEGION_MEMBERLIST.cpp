#include "aion/gameserver/network/aion/serverpackets/SM_LEGION_MEMBERLIST.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/model/team/legion/LegionMember.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"

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
	AION_UNPORTED();
}

void SM_LEGION_MEMBERLIST::writeLegionMember(model::team::legion::LegionMember& legionMember) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::network::aion::serverpackets
