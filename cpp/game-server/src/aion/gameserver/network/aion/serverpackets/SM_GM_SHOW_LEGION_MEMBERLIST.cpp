#include "aion/gameserver/network/aion/serverpackets/SM_GM_SHOW_LEGION_MEMBERLIST.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"
#include "aion/gameserver/model/team/legion/LegionMember.h"
#include "aion/gameserver/network/aion/AionServerPacket.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_GM_SHOW_LEGION_MEMBERLIST::SM_GM_SHOW_LEGION_MEMBERLIST(const std::vector<runtime::Ptr<model::team::legion::LegionMember>>& legionMembers,
	bool isFirst, bool isLast)
	: SM_LEGION_MEMBERLIST(opcodeOf<SM_GM_SHOW_LEGION_MEMBERLIST>, legionMembers, isFirst, isLast) {
}
void SM_GM_SHOW_LEGION_MEMBERLIST::writeLegionMember(model::team::legion::LegionMember& legionMember) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::network::aion::serverpackets
