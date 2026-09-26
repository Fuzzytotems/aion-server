#include "aion/gameserver/configs/main/EventsConfig.h"

#include "aion/gameserver/configs/detail/Bind.h"

namespace aion::gameserver::configs::main {

void EventsConfig::bind(commons::configuration::ConfigurableProcessor& p) {
	AION_BIND(p, "gameserver.event.service.disabled_events", DISABLED_EVENTS);
	AION_BIND(p, "gameserver.event.arcade.enable", ENABLE_EVENT_ARCADE, "false");
	AION_BIND(p, "gameserver.event.arcade.resume_token", ARCADE_RESUME_TOKEN, "3");
	AION_BIND(p, "gameserver.worldraid.enable", ENABLE_WORLDRAID, "true");
	AION_BIND(p, "gameserver.worldraid.use_spawn_msg", WORLDRAID_ENABLE_SPAWNMSG, "true");
	AION_BIND(p, "gameserver.event.headhunting.enable", ENABLE_HEADHUNTING, "false");
	AION_BIND(p, "gameserver.event.headhunting.maps", HEADHUNTING_MAPS, "");
	AION_BIND(p, "gameserver.event.headhunting.consolation_prize_kills", HEADHUNTING_CONSOLATION_PRIZE_KILLS, "50");
	AION_BIND(p, "gameserver.event.advent_calendar.enable", ENABLE_ADVENT_CALENDAR, "false");
}

} // namespace aion::gameserver::configs::main
