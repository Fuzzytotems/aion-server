#pragma once

// Chunk I6's instance maps, npc ids and handler factories for the shared fixture (tests/handlers_inst_1/InstanceFixture.h).

#include "../handlers_inst_1/InstanceFixture.h"

namespace aion::gameserver::handlers::instance::rakes {

// the marker factories of the handlers under test (AION_INSTANCE_HANDLER defines them in the handler library)
::aion::gameserver::handlers::InstanceFactory SteelRakeInstance_instanceFactory;
::aion::gameserver::handlers::InstanceFactory SteelRakeCabineInstance_instanceFactory;

} // namespace aion::gameserver::handlers::instance::rakes

namespace aion::gameserver::handlers::instance {

::aion::gameserver::handlers::InstanceFactory TheobomosLabInstance_instanceFactory;
::aion::gameserver::handlers::InstanceFactory InfinityShardInstance_instanceFactory;
::aion::gameserver::handlers::InstanceFactory TheShugoEmperorsVault_instanceFactory;

} // namespace aion::gameserver::handlers::instance

namespace aion::gameserver::handlers::instance::test {

inline constexpr int32_t STEEL_RAKE = 300100000;
inline constexpr int32_t STEEL_RAKE_CABIN = 300460000;
inline constexpr int32_t TIAMAT_STRONGHOLD = 300510000;
inline constexpr int32_t SHUGO_IMPERIAL_TOMB = 300560000;
inline constexpr int32_t INFINITY_SHARD = 300800000;
inline constexpr int32_t SHUGO_EMPERORS_VAULT = 301400000;
inline constexpr int32_t THEOBOMOS_LAB = 310110000;

/** world_maps.xml's rows of the I6 maps */
inline constexpr const char* I6_MAPS_XML = R"x(<world_maps>
	<map id="300100000" cName="IDshulackShip" name="Steel Rake" name_id="401255" instance="true" water_level="16" death_level="0" world_size="1024" drop_type="STEEL_RAKE_INSTANCE" flags="RECALL GLIDE PVP DUEL_SAME_RACE NO_RETURN_BATTLE" pve_attack_ratio="150" pve_defend_ratio="50"/>
	<map id="300460000" cName="IDshulackShip_Solo" name="Steel Rake Cabin" name_id="401843" instance="true" water_level="16" death_level="0" world_size="1024" drop_type="STEEL_RAKE_INSTANCE" flags="GLIDE PVP DUEL_SAME_RACE NO_RETURN_BATTLE" pve_attack_ratio="150" pve_defend_ratio="50"/>
	<map id="300510000" cName="IDTiamat_1" name="Tiamat Stronghold" name_id="403522" instance="true" water_level="16" death_level="0" world_size="2048" drop_type="BALAUREA_HIGH_INSTANCE" flags="GLIDE PVP DUEL_SAME_RACE NO_RETURN_BATTLE" pve_attack_ratio="150" pve_defend_ratio="50"/>
	<map id="300560000" cName="IDDF2Flying_Event01" name="Shugo Imperial Tomb" name_id="403566" instance="true" water_level="16" death_level="0" world_size="512" drop_type="OTHER_INSTANCES" except_buff="true" flags="GLIDE PVP DUEL_SAME_RACE NO_RETURN_BATTLE" pve_attack_ratio="150" pve_defend_ratio="50"/>
	<map id="300800000" cName="IDRuneweapon" name="Infinity Shard" name_id="403690" instance="true" water_level="16" death_level="0" world_size="256" drop_type="BALAUREA_HIGH_INSTANCE" flags="GLIDE PVP DUEL_SAME_RACE NO_RETURN_BATTLE"/>
	<map id="301400000" cName="IDSweep" name="The Shugo Emperor's Vault" name_id="404461" instance="true" water_level="16" death_level="0" world_size="1024" drop_type="NONE" except_buff="true" flags="GLIDE PVP DUEL_SAME_RACE NO_RETURN_BATTLE"/>
	<map id="310110000" cName="IDLF2a_Lab" name="Theobomos Lab" name_id="401151" instance="true" water_level="16" death_level="0" world_size="1024" drop_type="OTHER_INSTANCES" flags="RECALL GLIDE PVP DUEL_SAME_RACE NO_RETURN_BATTLE"/>
</world_maps>)x";

inline const char* instanceMapsXml() {
	return I6_MAPS_XML;
}

/** The npc ids I6's handlers and AIs spawn, find or kill, and two plain npcs the AIs under test are installed on */
inline const std::vector<int32_t>& handlerNpcIds() {
	static const std::vector<int32_t> ids{900200, 900300,
		// Steel Rake: Pegureronerk, the Kaneron Agent, the six special deliveries
		798376, 215041, 215074, 215075, 215076, 215077, 215054, 215055,
		// Steel Rake Cabin: Sweeper Nunukin, Madame Bovariki, the Steel Rake Shaman
		219026, 219032, 219003,
		// the rakes AIs: the deck crew and Chief Mate Menekiki, Golden Eye Mantutu with his devices and supplies, 219037, Tamer Anikiki and the
		// key boxes, Brass-Eye Grogget's and Chief Gunner Koakoa's helpers, Engineer Lahulahu's devices, the badabooms, the cannons
		215402, 215403, 215404, 215405, 230727, 231460, 214968, 215069, 219033, 281128, 281129, 701386, 701387, 219037, 219040, 700553, 281181,
		281182, 281183, 281184, 281187, 281212, 281213, 281108, 281109, 281110, 281111, 281112, 281113, 281114, 281322, 281323, 281324, 281325,
		281326, 231016, 231017, 730769,
		// Theobomos Lab: the exit, Silikor's guards, orb and memories, Silikor and Triroan, the researcher, Triroan's element summons
		730178, 280971, 280972, 280973, 281053, 281054, 214668, 214669, 798223, 280975, 280976, 280977, 280978,
		// Infinity Shard: Hyperion, the generators and their elites, the resonators, the turrets, the barrier, the wipers, the exit, the guards
		// and the ancient tyrhunds
		231073, 231074, 231078, 231082, 231086, 231083, 231087, 231079, 231075, 231092, 231093, 231094, 231095, 231102, 730741, 231104, 730842,
		231096, 231097, 231098, 231099, 231100, 231101, 231103,
		// The Shugo Emperor's Vault: the intruders and bosses, the doors, springs, portals and butler, the Shugos, the chests
		235629, 235630, 235660, 235631, 235652, 235653, 235641, 235644, 235634, 235649, 236083, 235633, 235650, 235635, 235636, 235637, 235638,
		235651, 235681, 235680, 235640, 235685, 235684, 235683, 235647, 235632, 832925, 832919, 832918, 832922, 832924, 832932, 832927, 833491,
		833492, 833493, 832931, 832930, 832950, 833461, 833462, 832929};
	return ids;
}

inline std::string handlerNpcAi(int32_t) {
	return "";
}

} // namespace aion::gameserver::handlers::instance::test
