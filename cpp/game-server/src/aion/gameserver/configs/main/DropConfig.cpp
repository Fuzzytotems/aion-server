#include "aion/gameserver/configs/main/DropConfig.h"

#include "aion/gameserver/configs/detail/Bind.h"

namespace aion::gameserver::configs::main {

void DropConfig::bind(commons::configuration::ConfigurableProcessor& p) {
	AION_BIND(p, "gameserver.drop.announce_quality", MIN_ANNOUNCE_QUALITY);
	AION_BIND(p, "gameserver.drop.disable_range_check_maps", DISABLE_RANGE_CHECK_MAPS);
}

} // namespace aion::gameserver::configs::main
