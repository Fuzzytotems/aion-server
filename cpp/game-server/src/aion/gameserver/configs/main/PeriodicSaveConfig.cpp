#include "aion/gameserver/configs/main/PeriodicSaveConfig.h"

#include "aion/gameserver/configs/detail/Bind.h"

namespace aion::gameserver::configs::main {

void PeriodicSaveConfig::bind(commons::configuration::ConfigurableProcessor& p) {
	AION_BIND(p, "gameserver.periodicsave.player.general", PLAYER_GENERAL, "900");
	AION_BIND(p, "gameserver.periodicsave.player.items", PLAYER_ITEMS, "900");
	AION_BIND(p, "gameserver.periodicsave.legion.items", LEGION_ITEMS, "1200");
	AION_BIND(p, "gameserver.periodicsave.player.pets", PLAYER_PETS, "10");
}

} // namespace aion::gameserver::configs::main
