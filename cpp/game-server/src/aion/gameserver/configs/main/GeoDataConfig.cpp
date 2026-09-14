#include "aion/gameserver/configs/main/GeoDataConfig.h"

#include "aion/gameserver/configs/detail/Bind.h"

namespace aion::gameserver::configs::main {

void GeoDataConfig::bind(commons::configuration::ConfigurableProcessor& p) {
	AION_BIND(p, "gameserver.geodata.enable", GEO_ENABLE, "true");
	AION_BIND(p, "gameserver.geodata.cansee.enable", CANSEE_ENABLE, "true");
	AION_BIND(p, "gameserver.geodata.fear.enable", FEAR_ENABLE, "true");
	AION_BIND(p, "gameserver.geodata.npc.move", GEO_NPC_MOVE, "true");
	AION_BIND(p, "gameserver.geodata.materials.enable", GEO_MATERIALS_ENABLE, "true");
	AION_BIND(p, "gameserver.geodata.materials.showdetails", GEO_MATERIALS_SHOWDETAILS, "false");
	AION_BIND(p, "gameserver.geodata.shields.enable", GEO_SHIELDS_ENABLE, "true");
}

} // namespace aion::gameserver::configs::main
