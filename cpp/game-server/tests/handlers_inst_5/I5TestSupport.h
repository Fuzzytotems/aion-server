#pragma once

// Chunk I5's instance maps, npc ids and handler factories for the shared fixture (tests/handlers_inst_1/InstanceFixture.h).

#include "../handlers_inst_1/InstanceFixture.h"

namespace aion::gameserver::handlers::instance {

// the marker factories of the handlers under test (AION_INSTANCE_HANDLER defines them in the handler library)
::aion::gameserver::handlers::InstanceFactory DanuarMysticariumInstance_instanceFactory;
::aion::gameserver::handlers::InstanceFactory SauroSupplyBaseInstance_instanceFactory;
::aion::gameserver::handlers::InstanceFactory TalocsHollowInstance_instanceFactory;
::aion::gameserver::handlers::InstanceFactory DanuarReliquaryInstance_instanceFactory;
::aion::gameserver::handlers::InstanceFactory DanuarReliquaryInstance_L_instanceFactory;
::aion::gameserver::handlers::InstanceFactory InfernalDanuarReliquaryInstance_instanceFactory;
::aion::gameserver::handlers::InstanceFactory NightmareCircus_instanceFactory;
::aion::gameserver::handlers::InstanceFactory RaksangRuinsInstance_instanceFactory;

} // namespace aion::gameserver::handlers::instance

namespace aion::gameserver::handlers::instance::test {

inline constexpr int32_t TALOCS_HOLLOW = 300190000;
inline constexpr int32_t DANUAR_MYSTICARIUM = 300480000;
inline constexpr int32_t DANUAR_RELIQUARY = 301110000;
inline constexpr int32_t SAURO_SUPPLY_BASE = 301130000;
inline constexpr int32_t LUCKY_DANUAR_RELIQUARY = 301330000;
inline constexpr int32_t INFERNAL_DANUAR_RELIQUARY = 301360000;
inline constexpr int32_t NIGHTMARE_CIRCUS = 301200000;
inline constexpr int32_t RAKSANG_RUINS = 300610000;

/** world_maps.xml's rows of the I5 maps under test */
inline constexpr const char* I5_MAPS_XML = R"x(<world_maps>
	<map id="300190000" cName="IDElim" name="Taloc's Hollow" name_id="401295" instance="true" water_level="16" death_level="0" world_type="BALAUREA" world_size="2048" drop_type="BALAUREA_INSTANCE" flags="GLIDE PVP DUEL_SAME_RACE NO_RETURN_BATTLE" pve_attack_ratio="150" pve_defend_ratio="50"/>
	<map id="300480000" cName="IDLDF5RE_Solo" name="Sealed Danuar Mysticarium" name_id="403519" instance="true" water_level="16" death_level="90" world_size="768" drop_type="BALAUREA_HIGH_INSTANCE" flags="GLIDE PVP DUEL_SAME_RACE NO_RETURN_BATTLE"/>
	<map id="301110000" cName="IDLDF5_Under_Rune" name="Danuar Reliquary" name_id="403695" instance="true" water_level="16" death_level="0" world_size="512" drop_type="BALAUREA_HIGH_INSTANCE" flags="PVP DUEL_SAME_RACE NO_RETURN_BATTLE"/>
	<map id="301130000" cName="IDVritra_Base" name="Sauro Supply Base" name_id="403759" instance="true" water_level="3" death_level="0" world_size="1024" drop_type="BALAUREA_HIGH_INSTANCE" flags="GLIDE PVP DUEL_SAME_RACE NO_RETURN_BATTLE"/>
	<map id="301330000" cName="IDLDF5_Under_Rune_PC" name="Lucky Danuar Reliquary" name_id="404164" instance="true" water_level="16" death_level="0" world_size="512" drop_type="BALAUREA_HIGH_INSTANCE" flags="PVP DUEL_SAME_RACE NO_RETURN_BATTLE" pve_attack_ratio="1000" pve_defend_ratio="200"/>
	<map id="301360000" cName="IDLDF5_Under_Rune_H" name="Infernal Danuar Reliquary" name_id="404306" instance="true" water_level="16" death_level="0" world_size="512" drop_type="BALAUREA_HIGH_INSTANCE" flags="PVP DUEL_SAME_RACE NO_RETURN_BATTLE"/>
	<map id="301200000" cName="IDAsteria_IU_World" name="The Nightmare Circus" name_id="404049" instance="true" water_level="20" death_level="0" world_size="1024" drop_type="NONE" flags="GLIDE PVP DUEL_SAME_RACE NO_RETURN_BATTLE"/>
	<map id="300610000" cName="IDRaksha_solo" name="Raksang Ruins" name_id="404645" instance="true" water_level="16" death_level="0" world_size="1536" drop_type="BALAUREA_HIGH_INSTANCE" flags="GLIDE PVP DUEL_SAME_RACE NO_RETURN_BATTLE"/>
</world_maps>)x";

inline const char* instanceMapsXml() {
	return I5_MAPS_XML;
}

/** The npc ids the I5 handlers spawn or kill */
inline const std::vector<int32_t>& handlerNpcIds() {
	static const std::vector<int32_t> ids{
		// Danuar Mysticarium
		219964, 219963, 219965, 219969, 731583, 702715, 702717, 219958, 219959, 702700, 702701,
		// Sauro Supply Base
		230846, 230852, 233316, 233317, 230847, 230848, 230837, 230849, 230850, 230851, 233255, 230838, 230853, 730872, 230854, 230855, 230856,
		230857, 230858, 801967, 284437, 284445, 284446, 730876, 730877, 233256, 233257, 233258,
		// Taloc's Hollow
		215480, 215467, 215457, 700633, 700738, 700739, 281817, 215488, 700740, 700741, 799503, 700940, 700941,
		// the Danuar Reliquaries
		284377, 284378, 284379, 231304, 231305, 284383, 284384, 855244, 855245, 234690, 234691, 701795, 802183, 730843, 284386,
		// the sauroBase AIs: Sheba's henchmen and channelers
		284435, 284436,
		// the tallocsHollow AIs: the Supraklaw, Kinquid's destroyers, Shishir and Gellmar, the cocoons
		217132, 282008, 282009, 799500, 799501, 730232, 730233,
		// the danuarReliquary AIs: the vengeful orb, Modor's marker, ice storm and adds, the weakened and the infernal adds
		284387, 284385, 284528, 284380, 284381, 284382, 284659, 284660, 284661, 284662, 284663, 284664, 856493, 856494, 856495, 856496, 856497,
		856498,
		// the Nightmare Circus and its AIs
		233455, 233450, 233459, 233453, 233467, 831747, 831740, 831627, 831741, 831718, 831551, 831552, 831553, 831598, 831744, 831745, 831575,
		831576, 831560, 831559, 831561, 831562, 233462, 233463, 233457, 233162, 831348, 831349,
		// Raksang Ruins and its AIs
		206378, 206379, 206380, 206395, 206396, 206397, 236010, 236011, 236012, 236013, 236014, 236074, 236075, 236076, 236020, 236021, 236096,
		236097, 236305, 236084, 217469, 236303, 236304, 236306, 730445, 730438, 702673, 702674, 702675, 702690, 702691, 702692, 206197, 206198,
		206199, 282325, 730446, 730447, 217425, 217451, 217456, 217475, 217647, 282451, 282452, 282453, 282454, 701062, 701063, 701064, 701065,
		282455, 282456, 282457, 282458, 217471, 217472, 217760, 217761, 217763, 217764,
		// two plain npcs
		900200, 900300};
	return ids;
}

inline std::string handlerNpcAi(int32_t) {
	return "";
}

} // namespace aion::gameserver::handlers::instance::test
