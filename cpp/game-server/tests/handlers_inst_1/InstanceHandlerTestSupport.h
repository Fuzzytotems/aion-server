#pragma once

// Chunk I1's instance maps, npc ids and handler factories for the shared fixture (InstanceFixture.h).

#include "InstanceFixture.h"

namespace aion::gameserver::handlers::instance {

// the marker factories of the handlers under test (AION_INSTANCE_HANDLER defines them in the handler library)
::aion::gameserver::handlers::InstanceFactory AdmaStrongholdInstance_instanceFactory;
::aion::gameserver::handlers::InstanceFactory DraupnirCaveInstance_instanceFactory;
::aion::gameserver::handlers::InstanceFactory FireTempleInstance_instanceFactory;
::aion::gameserver::handlers::InstanceFactory HaramelInstance_instanceFactory;
::aion::gameserver::handlers::InstanceFactory LowerUdasTempleInstance_instanceFactory;

} // namespace aion::gameserver::handlers::instance

namespace aion::gameserver::handlers::instance::dredgion {

::aion::gameserver::handlers::InstanceFactory BaranathDredgionInstance_instanceFactory;
::aion::gameserver::handlers::InstanceFactory ChantraDredgionInstance_instanceFactory;
::aion::gameserver::handlers::InstanceFactory TerathDredgionInstance_instanceFactory;

} // namespace aion::gameserver::handlers::instance::dredgion

namespace aion::gameserver::handlers::instance::abyss {

::aion::gameserver::handlers::InstanceFactory KrotanBarracks_instanceFactory;
::aion::gameserver::handlers::InstanceFactory KysisBarracks_instanceFactory;
::aion::gameserver::handlers::InstanceFactory MirenBarracks_instanceFactory;
::aion::gameserver::handlers::InstanceFactory KrotanChamber_instanceFactory;
::aion::gameserver::handlers::InstanceFactory KysisChamber_instanceFactory;
::aion::gameserver::handlers::InstanceFactory MirenChamber_instanceFactory;
::aion::gameserver::handlers::InstanceFactory LegionsKrotanBarracks_instanceFactory;
::aion::gameserver::handlers::InstanceFactory LegionsKysisBarracks_instanceFactory;
::aion::gameserver::handlers::InstanceFactory LegionsMirenBarracks_instanceFactory;

::aion::gameserver::handlers::InstanceFactory AsteriaInstance_instanceFactory;
::aion::gameserver::handlers::InstanceFactory RoahInstance_instanceFactory;
::aion::gameserver::handlers::InstanceFactory RightWingChamberInstance_instanceFactory;
::aion::gameserver::handlers::InstanceFactory LeftWingChamberInstance_instanceFactory;
::aion::gameserver::handlers::InstanceFactory SulfurTreeNestInstance_instanceFactory;
::aion::gameserver::handlers::InstanceFactory EsoterraceInstance_instanceFactory;
::aion::gameserver::handlers::InstanceFactory AbyssalSplinterInstance_instanceFactory;
::aion::gameserver::handlers::InstanceFactory UnstableSplinterpathInstance_instanceFactory;
::aion::gameserver::handlers::InstanceFactory TheHexwayInstance_instanceFactory;

} // namespace aion::gameserver::handlers::instance::abyss

namespace aion::gameserver::handlers::instance::test {

inline constexpr int32_t BARANATH_DREDGION = 300110000;
inline constexpr int32_t CHANTRA_DREDGION = 300210000;
inline constexpr int32_t TERATH_DREDGION = 300440000;
inline constexpr int32_t LOWER_UDAS_TEMPLE = 300160000;
inline constexpr int32_t HARAMEL = 300200000;
inline constexpr int32_t DRAUPNIR_CAVE = 320080000;
inline constexpr int32_t FIRE_TEMPLE = 320100000;
inline constexpr int32_t ADMA_STRONGHOLD = 320130000;

/** world_maps.xml's rows of the I1 maps under test (the Dredgions' child elements left out) */
inline constexpr const char* INSTANCE_MAPS_XML = R"x(<world_maps>
	<map id="300220000" cName="IDAbRe_Core" name="Abyssal Splinter" name_id="401676" instance="true" water_level="70" death_level="0" world_type="ABYSS" world_size="1024" drop_type="ABYSS_INSTANCE" flags="GLIDE PVP DUEL_SAME_RACE NO_RETURN_BATTLE" pve_attack_ratio="150" pve_defend_ratio="50"/>
	<map id="300600000" cName="IDAbRe_Core_02" name="Unstable Abyssal Splinter" name_id="403651" instance="true" water_level="70" death_level="0" world_type="ABYSS" world_size="1024" drop_type="ABYSS_INSTANCE" flags="GLIDE PVP DUEL_SAME_RACE NO_RETURN_BATTLE" pve_attack_ratio="150" pve_defend_ratio="50"/>
	<map id="300700000" cName="IDUnderpassRe" name="The Hexway" name_id="403660" instance="true" water_level="70" death_level="0" world_size="1024" drop_type="BALAUREA_INSTANCE" flags="RECALL PVP DUEL_SAME_RACE NO_RETURN_BATTLE" pve_attack_ratio="150" pve_defend_ratio="50"/>
	<map id="300250000" cName="IDF4Re_Drana" name="Esoterrace" name_id="401750" instance="true" water_level="40" death_level="0" world_size="1536" drop_type="BALAUREA_INSTANCE" flags="RECALL GLIDE PVP DUEL_SAME_RACE NO_RETURN_BATTLE" pve_attack_ratio="150" pve_defend_ratio="50"/>
	<map id="300050000" cName="IDAbRe_Up_Asteria" name="Asteria Chamber" name_id="401201" instance="true" water_level="16" death_level="0" world_type="ABYSS" world_size="1024" drop_type="ABYSS_INSTANCE" flags="RECALL PVP DUEL_SAME_RACE NO_RETURN_BATTLE" pve_attack_ratio="150" pve_defend_ratio="50"/>
	<map id="300060000" cName="IDAbRe_Low_Divine" name="Sulfur Tree Nest" name_id="401198" instance="true" water_level="16" death_level="0" world_type="ABYSS" world_size="1024" drop_type="ABYSS_INSTANCE" flags="RECALL PVP DUEL_SAME_RACE NO_RETURN_BATTLE" pve_attack_ratio="150" pve_defend_ratio="50"/>
	<map id="300070000" cName="IDAbRe_Up_Rhoo" name="Chamber of Roah" name_id="401202" instance="true" water_level="16" death_level="0" world_type="ABYSS" world_size="1024" drop_type="ABYSS_INSTANCE" flags="RECALL PVP DUEL_SAME_RACE NO_RETURN_BATTLE" pve_attack_ratio="150" pve_defend_ratio="50"/>
	<map id="300080000" cName="IDAbRe_Low_Wciel" name="Left Wing Chamber" name_id="401200" instance="true" water_level="70" death_level="0" world_type="ABYSS" world_size="1024" drop_type="ABYSS_INSTANCE" flags="RECALL PVP DUEL_SAME_RACE NO_RETURN_BATTLE" pve_attack_ratio="150" pve_defend_ratio="50"/>
	<map id="300090000" cName="IDAbRe_Low_Eciel" name="Right Wing Chamber" name_id="401199" instance="true" water_level="16" death_level="0" world_type="ABYSS" world_size="512" drop_type="ABYSS_INSTANCE" flags="RECALL PVP DUEL_SAME_RACE NO_RETURN_BATTLE" pve_attack_ratio="150" pve_defend_ratio="50"/>
	<map id="301300000" cName="IDAbRe_Up3_Crotan_02_N" name="Krotan Barracks" name_id="404100" instance="true" water_level="16" death_level="0" world_type="ABYSS" world_size="1024" drop_type="ABYSS_INSTANCE" flags="RECALL PVP DUEL_SAME_RACE NO_RETURN_BATTLE"/>
	<map id="301280000" cName="IDAbRe_Up3_Dkisas_02_N" name="Kysis Barracks" name_id="404101" instance="true" water_level="16" death_level="0" world_type="ABYSS" world_size="1024" drop_type="ABYSS_INSTANCE" flags="RECALL PVP DUEL_SAME_RACE NO_RETURN_BATTLE"/>
	<map id="301290000" cName="IDAbRe_Up3_Lamiren_02_N" name="Miren Barracks" name_id="404102" instance="true" water_level="16" death_level="0" world_type="ABYSS" world_size="1024" drop_type="ABYSS_INSTANCE" flags="RECALL PVP DUEL_SAME_RACE NO_RETURN_BATTLE"/>
	<map id="300140000" cName="IDAbRe_Up3_Crotan" name="Krotan Chamber" name_id="401203" instance="true" water_level="16" death_level="0" world_type="ABYSS" world_size="1024" drop_type="ABYSS_INSTANCE" flags="RECALL PVP DUEL_SAME_RACE NO_RETURN_BATTLE" pve_attack_ratio="150" pve_defend_ratio="50"/>
	<map id="300120000" cName="IDAbRe_Up3_Dkisas" name="Kysis Chamber" name_id="401204" instance="true" water_level="16" death_level="0" world_type="ABYSS" world_size="1024" drop_type="ABYSS_INSTANCE" flags="RECALL PVP DUEL_SAME_RACE NO_RETURN_BATTLE" pve_attack_ratio="150" pve_defend_ratio="50"/>
	<map id="300130000" cName="IDAbRe_Up3_Lamiren" name="Miren Chamber" name_id="401205" instance="true" water_level="16" death_level="0" world_type="ABYSS" world_size="1024" drop_type="ABYSS_INSTANCE" flags="RECALL PVP DUEL_SAME_RACE NO_RETURN_BATTLE" pve_attack_ratio="150" pve_defend_ratio="50"/>
	<map id="301260000" cName="IDAbRe_Up3_Crotan_02" name="Legion's Krotan Barracks" name_id="404061" instance="true" water_level="16" death_level="0" world_type="ABYSS" world_size="1024" drop_type="ABYSS_INSTANCE" flags="RECALL PVP DUEL_SAME_RACE NO_RETURN_BATTLE"/>
	<map id="301240000" cName="IDAbRe_Up3_Dkisas_02" name="Legion's Kysis Barracks" name_id="404062" instance="true" water_level="16" death_level="0" world_type="ABYSS" world_size="1024" drop_type="ABYSS_INSTANCE" flags="RECALL PVP DUEL_SAME_RACE NO_RETURN_BATTLE"/>
	<map id="301250000" cName="IDAbRe_Up3_Lamiren_02" name="Legion's Miren Barracks" name_id="404063" instance="true" water_level="16" death_level="0" world_type="ABYSS" world_size="1024" drop_type="ABYSS_INSTANCE" flags="RECALL PVP DUEL_SAME_RACE NO_RETURN_BATTLE"/>
	<map id="300110000" cName="IDAb1_Dreadgion" name="Dredgion" name_id="401193" instance="true" water_level="16" death_level="0" world_size="1024" drop_type="DREDGION_INSTANCE" except_buff="true" flags="GLIDE PVP DUEL_SAME_RACE NO_RETURN_BATTLE" pve_attack_ratio="150" pve_defend_ratio="50"/>
	<map id="300210000" cName="IDDreadgion_02" name="Chantra Dredgion" name_id="401675" instance="true" water_level="16" death_level="0" world_type="BALAUREA" world_size="1024" drop_type="DREDGION_INSTANCE" except_buff="true" flags="GLIDE PVP DUEL_SAME_RACE NO_RETURN_BATTLE" pve_attack_ratio="150" pve_defend_ratio="50"/>
	<map id="300440000" cName="IDDreadgion_03" name="Terath Dredgion" name_id="401836" instance="true" water_level="16" death_level="0" world_type="BALAUREA" world_size="1024" drop_type="DREDGION_INSTANCE" except_buff="true" flags="GLIDE PVP DUEL_SAME_RACE NO_RETURN_BATTLE" pve_attack_ratio="150" pve_defend_ratio="50"/>
	<map id="300160000" cName="IDTemple_Low" name="Lower Udas Temple" name_id="401325" instance="true" water_level="16" death_level="0" world_size="2048" drop_type="BALAUREA_INSTANCE" flags="GLIDE PVP DUEL_SAME_RACE NO_RETURN_BATTLE" pve_attack_ratio="150" pve_defend_ratio="50"/>
	<map id="300200000" cName="IDNovice" name="Haramel" name_id="401728" instance="true" water_level="16" death_level="0" world_size="512" drop_type="OTHER_INSTANCES" flags="GLIDE PVP DUEL_SAME_RACE NO_RETURN_BATTLE" pve_attack_ratio="150" pve_defend_ratio="50"/>
	<map id="320080000" cName="IDDF3_Dragon" name="Draupnir Cave" name_id="401022" instance="true" water_level="16" death_level="0" world_size="1024" drop_type="OTHER_INSTANCES" flags="RECALL GLIDE PVP DUEL_SAME_RACE NO_RETURN_BATTLE" pve_attack_ratio="150" pve_defend_ratio="50"/>
	<map id="320100000" cName="IDDF2_Dflame" name="Fire Temple" name_id="400836" instance="true" water_level="16" death_level="0" world_size="512" drop_type="OTHER_INSTANCES" flags="RECALL GLIDE PVP DUEL_SAME_RACE NO_RETURN_BATTLE" pve_attack_ratio="150" pve_defend_ratio="50"/>
	<map id="320130000" cName="IDDf2a_Adma" name="Adma Stronghold" name_id="401152" instance="true" water_level="16" death_level="0" world_size="1024" drop_type="OTHER_INSTANCES" flags="RECALL GLIDE PVP DUEL_SAME_RACE NO_RETURN_BATTLE" pve_attack_ratio="150" pve_defend_ratio="50"/>
</world_maps>)x";

inline const char* instanceMapsXml() {
	return INSTANCE_MAPS_XML;
}

/** The npc ids the five handlers spawn or kill */
inline const std::vector<int32_t>& handlerNpcIds() {
	static const std::vector<int32_t> ids{205224, 212839, 212790, 212842, 212799, 212840, 212794, 212843, 212803, 212841, 212845, 214094, 214621, 212846,
		216922, 216920, 216921, 217067, 700950, 700829, 700830, 700831, 700832, 700852, 213587, 213588, 213771, 213773, 213776, 213778, 213779,
		213802, 213780, 216150, 216645, 216531, 216530,
		// the Dredgions
		215093, 214823, 700485, 700486, 700487, 700488, 700489, 700490, 700491, 700492, 700493, 700495, 700496, 700497, 700498, 700508, 700502,
		700506, 730214, 216886, 216889, 216890, 730349, 730314, 701441, 701442, 701455, 233377, 233371,
		// the I1 AIs' npcs: Icaronix the Betrayer, the duke's summons, an illusion gate (test id 900100) and the ids it adds, two plain npcs
		214599, 284978, 284979, 284980, 284981, 900100, 900101, 900102, 900103, 900200, 900300,
		// Krotan Barracks: the key masters, the bosses, the chests (and the legion version's), the timer npc, the doors, a door guard, the
		// artifact, the statues
		233627, 233628, 233629, 233630, 233632, 233633, 702288, 702289, 702290, 702291, 206095, 700545, 233599, 233600, 215413, 235536,
		// the five chambers with a start ring: the treasures the timer takes, the gold chests, Treasurer Nabatma, the exit
		700475, 700476, 700477, 701483, 701488, 700472, 700473, 700474, 701489, 701484, 700471, 701482, 701487, 700466, 701481, 701486, 214804,
		700463, 700462, 700464, 701485, 701480, 215424, 700455,
		// Esoterrace
		799580, 217649, 282295, 282291, 217204, 217205, 217289, 217281, 217195, 701027, 217185, 701023, 205437, 217206, 701025, 217282, 217283,
		217284,
		// Warden Surama's geysers
		282171, 282172, 282173, 282174, 282425, 282426, 282427, 282428,
		// Dalia Charlands's greenfingers
		282176, 282177, 282178,
		// Abyssal Splinter
		216951, 216950, 216948, 216949, 281907, 281908, 216960, 216952, 700955, 281909, 282014, 282015, 282131, 281903, 281904, 700934, 700860,
		700861, 700935, 700936, 700937, 700938, 282107, 730317, 700856, 701593, 700857, 700858, 700862, 700865, 700864,
		// Unstable Splinterpath
		219554, 219553, 219551, 219552, 283204, 283205, 219555, 219563, 701588, 283206, 219567, 219579, 219580, 219565, 219566, 701575, 701576,
		701577, 701578, 701579, 701580, 219586, 700957, 701589,
		// the splinter AIs: Kaluva's eggs and their hatchlings
		281902, 281911, 281912, 282057, 219971, 219952, 219970, 219969, 219572, 219573, 219584,
		// The Hexway
		219609, 219617, 219611, 286933, 219612, 219613, 219610, 219614, 219635, 219636, 701662, 701663, 701664,
		// the Hexway AIs: the smoke, Galamat's adds
		282465, 219616};
	return ids;
}

inline std::string handlerNpcAi(int32_t npcId) {
	// the greenfingers walk (DaliaCharlandsAI casts their AI to NpcAI)
	return npcId >= 282176 && npcId <= 282178 ? "greenfingers" : "";
}

} // namespace aion::gameserver::handlers::instance::test
