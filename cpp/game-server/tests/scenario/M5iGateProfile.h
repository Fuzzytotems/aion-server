#pragma once

// M5i I-6 (m5i-plan.md 10.1, 15.5): the keys of the siege gate gs.scenario.m5i, merged into its gateKeys over the scenario profile
// (ScenarioServers::m5aProfile keeps sieges, rifts, world raids, vortexes and CP off):
// - sieges on (SiegeConfig.SIEGE_ENABLED), the Balaur assault off (D4), the server time zone UTC (D13: the siege schedule is in server time);
// - rifts, world raids and the vortex on for I-11's cases (C10, C11, C16); CP stays off;
// - the Ahserion raid and the Moltenus spawn stay at the profile's far future (D6).
// The siege schedule (config/schedule/siege_schedule.xml) is armed as shipped: its evening slots fall outside every gate run the overnight lane
// starts (the earliest preparation of a Wednesday is 20:55 UTC; the gate prints the time it ran).

#include <map>
#include <string>

namespace aion::gameserver::scenario {

inline std::map<std::string, std::string> m5iSiegeKeys() {
	return {
		{"gameserver.siege.enable", "true"},
		{"gameserver.siege.assault.enable", "false"},
		{"gameserver.timezone", "UTC"},
		// I-11, the world-event half: rifts (their hourly cron opens rifts at hh:00 - the gate starts at least 20 minutes before an hh:00), world
		// raids (the shipped schedule's days 3, 6, 9, ... 18, 21, 24 at 17:30/19:30), and the vortex (the shipped schedule: Saturday and Sunday 16:00)
		{"gameserver.rift.enable", "true"},
		{"gameserver.worldraid.enable", "true"},
		{"gameserver.vortex.enable", "true"},
	};
}

} // namespace aion::gameserver::scenario
