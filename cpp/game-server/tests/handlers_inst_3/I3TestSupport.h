#pragma once

// Chunk I3's instance maps, npc ids and handler factories for the shared fixture (tests/handlers_inst_1/InstanceFixture.h).

#include "../handlers_inst_1/InstanceFixture.h"

namespace aion::gameserver::handlers::instance {

// the marker factories of the handlers under test (AION_INSTANCE_HANDLER defines them in the handler library)
::aion::gameserver::handlers::InstanceFactory OphidanBridgeInstance_instanceFactory;
::aion::gameserver::handlers::InstanceFactory OphidanBridgeInstance_L_instanceFactory;
::aion::gameserver::handlers::InstanceFactory LinkgateFoundryInstance_instanceFactory;
::aion::gameserver::handlers::InstanceFactory KromedesTrialInstance_instanceFactory;

} // namespace aion::gameserver::handlers::instance

namespace aion::gameserver::handlers::instance::test {

inline constexpr int32_t KROMEDES_TRIAL = 300230000;
inline constexpr int32_t OPHIDAN_BRIDGE = 300590000;
inline constexpr int32_t LINKGATE_FOUNDRY = 301270000;
inline constexpr int32_t LUCKY_OPHIDAN_BRIDGE = 301320000;

/** world_maps.xml's rows of the I3 maps under test */
inline constexpr const char* I3_MAPS_XML = R"x(<world_maps>
	<map id="300230000" cName="IDCromede" name="Kromede's Trial" name_id="401727" instance="true" water_level="16" death_level="0" world_size="1024" drop_type="OTHER_INSTANCES" flags="GLIDE PVP DUEL_SAME_RACE NO_RETURN_BATTLE" pve_attack_ratio="150" pve_defend_ratio="50"/>
	<map id="300590000" cName="IDLDF5_Under_01" name="Ophidan Bridge" name_id="403621" instance="true" water_level="0" death_level="0" world_size="1024" drop_type="BALAUREA_HIGH_INSTANCE" flags="GLIDE PVP DUEL_SAME_RACE NO_RETURN_BATTLE"/>
	<map id="301270000" cName="IDLDF4Re_01" name="Linkgate Foundry" name_id="404099" instance="true" water_level="16" death_level="0" world_size="512" drop_type="BALAUREA_HIGH_INSTANCE" flags="GLIDE PVP DUEL_SAME_RACE NO_RETURN_BATTLE"/>
	<map id="301320000" cName="IDLDF5_Under_01_PC" name="Lucky Ophidan Bridge" name_id="404163" instance="true" water_level="0" death_level="0" world_size="1024" drop_type="BALAUREA_HIGH_INSTANCE" flags="GLIDE PVP DUEL_SAME_RACE NO_RETURN_BATTLE"/>
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
		// two plain npcs
		900200, 900300};
	return ids;
}

inline std::string handlerNpcAi(int32_t) {
	return "";
}

} // namespace aion::gameserver::handlers::instance::test
