#include "aion/gameserver/configs/main/PunishmentConfig.h"

#include "aion/gameserver/configs/detail/Bind.h"

namespace aion::gameserver::configs::main {

void PunishmentConfig::bind(commons::configuration::ConfigurableProcessor& p) {
	AION_BIND(p, "gameserver.punishment.enable", PUNISHMENT_ENABLE, "false");
	AION_BIND(p, "gameserver.punishment.type", PUNISHMENT_TYPE, "1");
	AION_BIND(p, "gameserver.punishment.time", PUNISHMENT_TIME, "1440");
}

} // namespace aion::gameserver::configs::main
