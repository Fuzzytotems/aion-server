#pragma once

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/model/team/legion/fwd.h"
#include "aion/gameserver/network/aion/serverpackets/SM_LEGION_INFO.h"
#include "aion/gameserver/network/aion/serverpackets/fwd.h"

namespace aion::gameserver::network::aion::serverpackets {

/**
 * @author Yeats.
 */
class SM_GM_SHOW_LEGION_INFO : public SM_LEGION_INFO {
public:
	explicit SM_GM_SHOW_LEGION_INFO(model::team::legion::Legion& legion);
};

} // namespace aion::gameserver::network::aion::serverpackets
