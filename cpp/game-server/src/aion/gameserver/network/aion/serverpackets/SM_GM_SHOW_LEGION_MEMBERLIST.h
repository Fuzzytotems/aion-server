#pragma once

#include <vector>

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/model/team/legion/fwd.h"
#include "aion/gameserver/network/aion/serverpackets/SM_LEGION_MEMBERLIST.h"
#include "aion/gameserver/network/aion/serverpackets/fwd.h"

namespace aion::gameserver::network::aion::serverpackets {

/**
 * @author Yeats
 */
class SM_GM_SHOW_LEGION_MEMBERLIST : public SM_LEGION_MEMBERLIST {
public:
	SM_GM_SHOW_LEGION_MEMBERLIST(const std::vector<runtime::Ptr<model::team::legion::LegionMember>>& legionMembers, bool isFirst, bool isLast);

protected:
	void writeLegionMember(model::team::legion::LegionMember& legionMember) override;
};

} // namespace aion::gameserver::network::aion::serverpackets
