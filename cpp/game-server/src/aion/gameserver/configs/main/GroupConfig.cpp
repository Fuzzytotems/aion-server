#include "aion/gameserver/configs/main/GroupConfig.h"

#include "aion/gameserver/configs/detail/Bind.h"

namespace aion::gameserver::configs::main {

void GroupConfig::bind(commons::configuration::ConfigurableProcessor& p) {
	AION_BIND(p, "gameserver.playergroup.removetime", GROUP_REMOVE_TIME, "600");
	AION_BIND(p, "gameserver.playergroup.maxdistance", GROUP_MAX_DISTANCE, "100");
	AION_BIND(p, "gameserver.group.inviteotherfaction", GROUP_INVITEOTHERFACTION, "false");
	AION_BIND(p, "gameserver.playeralliance.removetime", ALLIANCE_REMOVE_TIME, "600");
	AION_BIND(p, "gameserver.playeralliance.inviteotherfaction", ALLIANCE_INVITEOTHERFACTION, "false");
	AION_BIND(p, "gameserver.instance_group.form_anywhere", FORM_INSTANCE_GROUP_ANYWHERE, "false");
}

} // namespace aion::gameserver::configs::main
