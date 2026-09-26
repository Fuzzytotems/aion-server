#include "aion/gameserver/configs/main/FallDamageConfig.h"

#include "aion/gameserver/configs/detail/Bind.h"

namespace aion::gameserver::configs::main {

void FallDamageConfig::bind(commons::configuration::ConfigurableProcessor& p) {
	AION_BIND(p, "gameserver.falldamage.percentage", FALL_DAMAGE_PERCENTAGE, "1.0");
	AION_BIND(p, "gameserver.falldamage.distance.minimum", MINIMUM_DISTANCE_DAMAGE, "10");
	AION_BIND(p, "gameserver.falldamage.distance.maximum", MAXIMUM_DISTANCE_DAMAGE, "50");
	AION_BIND(p, "gameserver.falldamage.distance.midair", MAXIMUM_DISTANCE_MIDAIR, "200");
}

} // namespace aion::gameserver::configs::main
