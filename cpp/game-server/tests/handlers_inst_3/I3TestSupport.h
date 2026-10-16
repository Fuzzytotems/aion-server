#pragma once

// Chunk I3's instance maps, npc ids and handler factories for the shared fixture (tests/handlers_inst_1/InstanceFixture.h).

#include "../handlers_inst_1/InstanceFixture.h"

namespace aion::gameserver::handlers::instance {

// the marker factories of the handlers under test (AION_INSTANCE_HANDLER defines them in the handler library)
::aion::gameserver::handlers::InstanceFactory OphidanBridgeInstance_instanceFactory;
::aion::gameserver::handlers::InstanceFactory OphidanBridgeInstance_L_instanceFactory;
::aion::gameserver::handlers::InstanceFactory LinkgateFoundryInstance_instanceFactory;
::aion::gameserver::handlers::InstanceFactory KromedesTrialInstance_instanceFactory;
::aion::gameserver::handlers::InstanceFactory EternalBastionInstance_instanceFactory;
namespace pvp {
::aion::gameserver::handlers::InstanceFactory IronWallWarfrontInstance_instanceFactory;
::aion::gameserver::handlers::InstanceFactory IdgelDomeInstance_instanceFactory;
::aion::gameserver::handlers::InstanceFactory KamarBattlefieldInstance_instanceFactory;
::aion::gameserver::handlers::InstanceFactory EngulfedOphidanBridgeInstance_instanceFactory;
} // namespace pvp
namespace pvparenas {
::aion::gameserver::handlers::InstanceFactory DisciplineTrainingGroundsInstance_instanceFactory;
::aion::gameserver::handlers::InstanceFactory ArenaOfDisciplineInstance_instanceFactory;
::aion::gameserver::handlers::InstanceFactory ChaosTrainingGroundsInstance_instanceFactory;
::aion::gameserver::handlers::InstanceFactory ArenaOfChaosInstance_instanceFactory;
::aion::gameserver::handlers::InstanceFactory ArenaOfGloryInstance_instanceFactory;
::aion::gameserver::handlers::InstanceFactory HarmonyTrainingGroundsInstance_instanceFactory;
::aion::gameserver::handlers::InstanceFactory ArenaOfHarmonyInstance_instanceFactory;
} // namespace pvparenas

} // namespace aion::gameserver::handlers::instance

namespace aion::gameserver::handlers::instance::test {

inline constexpr int32_t KROMEDES_TRIAL = 300230000;
inline constexpr int32_t OPHIDAN_BRIDGE = 300590000;
inline constexpr int32_t LINKGATE_FOUNDRY = 301270000;
inline constexpr int32_t LUCKY_OPHIDAN_BRIDGE = 301320000;
inline constexpr int32_t ETERNAL_BASTION = 300540000;
inline constexpr int32_t KAMAR_BATTLEFIELD = 301120000;
inline constexpr int32_t ENGULFED_OPHIDAN_BRIDGE = 301210000;
inline constexpr int32_t IRON_WALL_WARFRONT = 301220000;
inline constexpr int32_t IDGEL_DOME = 301310000;
inline constexpr int32_t ARENA_OF_CHAOS = 300350000;
inline constexpr int32_t ARENA_OF_DISCIPLINE = 300360000;
inline constexpr int32_t CHAOS_TRAINING_GROUNDS = 300420000;
inline constexpr int32_t DISCIPLINE_TRAINING_GROUNDS = 300430000;
inline constexpr int32_t ARENA_OF_GLORY = 300550000;
inline constexpr int32_t ARENA_OF_HARMONY = 300450000;
inline constexpr int32_t HARMONY_TRAINING_GROUNDS = 300570000;

/** world_maps.xml's rows of the I3 maps under test */
inline constexpr const char* I3_MAPS_XML = R"x(<world_maps>
	<map id="300230000" cName="IDCromede" name="Kromede's Trial" name_id="401727" instance="true" water_level="16" death_level="0" world_size="1024" drop_type="OTHER_INSTANCES" flags="GLIDE PVP DUEL_SAME_RACE NO_RETURN_BATTLE" pve_attack_ratio="150" pve_defend_ratio="50"/>
	<map id="300590000" cName="IDLDF5_Under_01" name="Ophidan Bridge" name_id="403621" instance="true" water_level="0" death_level="0" world_size="1024" drop_type="BALAUREA_HIGH_INSTANCE" flags="GLIDE PVP DUEL_SAME_RACE NO_RETURN_BATTLE"/>
	<map id="301270000" cName="IDLDF4Re_01" name="Linkgate Foundry" name_id="404099" instance="true" water_level="16" death_level="0" world_size="512" drop_type="BALAUREA_HIGH_INSTANCE" flags="GLIDE PVP DUEL_SAME_RACE NO_RETURN_BATTLE"/>
	<map id="301320000" cName="IDLDF5_Under_01_PC" name="Lucky Ophidan Bridge" name_id="404163" instance="true" water_level="0" death_level="0" world_size="1024" drop_type="BALAUREA_HIGH_INSTANCE" flags="GLIDE PVP DUEL_SAME_RACE NO_RETURN_BATTLE"/>
	<map id="301120000" cName="IDKamar" name="Kamar Battlefield" name_id="403696" instance="true" water_level="200" death_level="0" world_size="3072" drop_type="BATTLEFIELD_INSTANCE" except_buff="true" flags="RECALL GLIDE PVP DUEL_SAME_RACE NO_RETURN_BATTLE"/>
	<map id="301210000" cName="IDLDF5_Under_01_War" name="Engulfed Ophidan Bridge" name_id="404051" instance="true" water_level="0" death_level="560" world_size="1024" drop_type="BALAUREA_HIGH_INSTANCE" except_buff="true" flags="GLIDE PVP DUEL_SAME_RACE NO_RETURN_BATTLE"/>
	<map id="301220000" cName="IDF5_TD_War" name="Iron Wall Warfront" name_id="404056" instance="true" water_level="60" death_level="0" world_size="1024" drop_type="BALAUREA_HIGH_INSTANCE" except_buff="true" flags="GLIDE PVP DUEL_SAME_RACE NO_RETURN_BATTLE"/>
	<map id="301310000" cName="IDLDF5_Fortress_Re" name="Idgel Dome" name_id="404081" instance="true" water_level="16" death_level="0" world_size="512" drop_type="BATTLEFIELD_INSTANCE" except_buff="true" flags="GLIDE PVP DUEL_SAME_RACE NO_RETURN_BATTLE"/>
	<map id="300350000" cName="IDArena_pvp01" name="Arena of Chaos" name_id="402045" instance="true" water_level="16" death_level="0" world_size="2048" drop_type="NONE" except_buff="true" flags="GLIDE PVP DUEL_SAME_RACE NO_RETURN_BATTLE" pve_attack_ratio="150" pve_defend_ratio="50"/>
	<map id="300360000" cName="IDArena_pvp02" name="Arena of Discipline" name_id="402046" instance="true" water_level="0" death_level="0" world_size="2048" drop_type="NONE" except_buff="true" flags="GLIDE PVP DUEL_SAME_RACE NO_RETURN_BATTLE" pve_attack_ratio="150" pve_defend_ratio="50"/>
	<map id="300420000" cName="IDArena_pvp01_T" name="Chaos Training Grounds" name_id="403259" instance="true" water_level="16" death_level="0" world_size="2048" drop_type="NONE" except_buff="true" flags="GLIDE PVP DUEL_SAME_RACE NO_RETURN_BATTLE" pve_attack_ratio="150" pve_defend_ratio="50"/>
	<map id="300430000" cName="IDArena_pvp02_T" name="Discipline Training Grounds" name_id="403260" instance="true" water_level="0" death_level="0" world_size="2048" drop_type="NONE" except_buff="true" flags="GLIDE PVP DUEL_SAME_RACE NO_RETURN_BATTLE" pve_attack_ratio="150" pve_defend_ratio="50"/>
	<map id="300550000" cName="IDArena_Glory" name="Arena of Glory" name_id="403554" instance="true" water_level="16" death_level="0" world_size="2048" drop_type="NONE" except_buff="true" flags="GLIDE PVP DUEL_SAME_RACE NO_RETURN_BATTLE" pve_attack_ratio="150" pve_defend_ratio="50"/>
	<map id="300450000" cName="IDArena_Team01" name="Arena of Harmony" name_id="403505" instance="true" water_level="16" death_level="0" world_size="2048" drop_type="NONE" except_buff="true" flags="GLIDE PVP DUEL_SAME_RACE NO_RETURN_BATTLE" pve_attack_ratio="150" pve_defend_ratio="50"/>
	<map id="300570000" cName="IDArena_Team01_T" name="Harmony Training Grounds" name_id="403574" instance="true" water_level="16" death_level="0" world_size="2048" drop_type="NONE" except_buff="true" flags="GLIDE PVP DUEL_SAME_RACE NO_RETURN_BATTLE" pve_attack_ratio="150" pve_defend_ratio="50"/>
	<map id="300540000" cName="IDLDF5b_TD" name="The Eternal Bastion" name_id="403551" instance="true" water_level="60" death_level="0" world_size="1024" drop_type="BALAUREA_HIGH_INSTANCE" flags="GLIDE PVP DUEL_SAME_RACE NO_RETURN_BATTLE"/>
</world_maps>)x";

inline const char* instanceMapsXml() {
	return I3_MAPS_XML;
}

/** The npc ids the I3 handlers spawn or kill */
inline const std::vector<int32_t>& handlerNpcIds() {
	static const std::vector<int32_t> ids{
		// Ophidan Bridge
		235764, 235775, 235774, 235773, 235772, 701644, 235768, 235759, 235763, 235767, 235769, 235770, 235771, 730868, 856059, 235786, 731544,
		// Linkgate Foundry
		206361, 206362, 233898, 234990, 234991, 702338, 702389, 804578, 234193, 804629, 702592, 702590, 234992, 702339, 702591, 855087, 855088,
		// Kromede's Trial
		217002, 217000, 216982, 217005, 217001, 217003, 217004, 217006, 730308, 216968, 282093, 282095,
		// the PvP instances (Iron Wall Warfront, Idgel Dome, Kamar Battlefield, Engulfed Ophidan Bridge) and their AIs
		233473, 702042, 233491, 701943, 701944, 701945, 233494, 801903, 801772, 801766, 730861, 701900, 701901, 802383, 802384, 702300, 702301,
		802548, 802549, 287249, 234186, 234751, 702581, 702582, 702583, 855010, 702405, 802016, 802017, 802018, 802019, 701808, 701912, 801962,
		232847, 232848, 232849, 232850, 232851, 233261, 232852, 232853, 232854, 232846, 232857, 232858, 232859, 232860, 801956, 801957, 801958,
		801959, 801960, 801961, 701806, 701902, 802025, 802026, 701947, 701948, 701949, 701950, 802033, 802034, 802035, 802036, 802037, 802038,
		802039, 802040, 802041, 802042, 802043, 802044, 702043, 702044, 701984, 701985, 701986, 701987, 701988, 701989, 701976, 702018, 233490,
		233492, 233493, 233495, 701939, 701940, 701941, 701942, 701953, 701954, 701955, 701956, 701969, 701970, 701971, 701972,
		// the arenas and their AIs
		218690, 218682, 218685, 701187, 701188, 701184, 701183, 207116, 207099,
		// Eternal Bastion and its AIs
		209516, 209517, 209555, 209557, 230744, 230745, 230746, 230749, 230753, 230754, 230756, 230757, 230783, 230784, 230785, 231105, 231106,
		231107, 231108, 231110, 231111, 231112, 231113, 231114, 231115, 231116, 231117, 231118, 231119, 231120, 231122, 231123, 231124, 231125,
		231126, 231127, 231128, 231130, 231131, 231132, 231133, 231134, 231137, 231138, 231140, 231141, 231142, 231143, 231144, 231148, 231149,
		231150, 231152, 231153, 231154, 231155, 231156, 231157, 231158, 231159, 231160, 231162, 231163, 231164, 231165, 231167, 231168, 231169,
		231170, 231171, 231172, 231173, 231174, 231175, 231176, 231177, 231178, 231179, 231180, 231181, 233309, 233310, 233311, 233312, 233313,
		233314, 233315, 284320, 284685, 284686, 284695, 284696, 284697, 284699, 284707, 701596, 701597, 701598, 701599, 701600, 701601, 701602,
		701603, 701604, 701605, 701606, 701607, 701610, 701611, 701612, 701613, 701614, 701615, 701616, 701617, 701618, 701619, 701620, 701621,
		701625, 701913, 701914, 701915, 701916, 701917, 701922, 701923, 701924, 730871, 831330, 831332, 831333, 831334, 831335,
		// two plain npcs
		900200, 900300};
	return ids;
}

inline std::string handlerNpcAi(int32_t) {
	return "";
}

} // namespace aion::gameserver::handlers::instance::test
