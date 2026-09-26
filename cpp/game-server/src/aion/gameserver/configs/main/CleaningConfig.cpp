#include "aion/gameserver/configs/main/CleaningConfig.h"

#include "aion/gameserver/configs/detail/Bind.h"

namespace aion::gameserver::configs::main {

void CleaningConfig::bind(commons::configuration::ConfigurableProcessor& p) {
	AION_BIND(p, "gameserver.cleaning.enable", CLEANING_ENABLE, "false");
	AION_BIND(p, "gameserver.cleaning.min_account_inactivity", MIN_ACCOUNT_INACTIVITY_DAYS, "365");
	AION_BIND(p, "gameserver.cleaning.max_level", MAX_DELETABLE_CHAR_LEVEL, "25");
}

} // namespace aion::gameserver::configs::main
