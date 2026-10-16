#include "aion/gameserver/configs/main/WorldConfig.h"

#include "aion/gameserver/configs/detail/Bind.h"

namespace aion::gameserver::configs::main {

void WorldConfig::bind(commons::configuration::ConfigurableProcessor& p) {
	AION_BIND(p, "gameserver.world.region.size", WORLD_REGION_SIZE, "128");
	AION_BIND(p, "gameserver.world.max.twincount.usual", WORLD_MAX_TWINS_USUAL, "1");
	AION_BIND(p, "gameserver.world.max.twincount.beginner", WORLD_MAX_TWINS_BEGINNER, "-1");
	AION_BIND(p, "gameserver.world.emulate.fasttrack", WORLD_EMULATE_FASTTRACK, "true");
	AION_BIND(p, "gameserver.world.zone_handler_directory", ZONE_HANDLER_DIRECTORY, "./data/handlers/zone");
}

} // namespace aion::gameserver::configs::main
