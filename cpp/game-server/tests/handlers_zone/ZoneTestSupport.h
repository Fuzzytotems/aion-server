#pragma once

// Chunk Z1's map and npc ids for the shared instance fixture (tests/handlers_inst_1/InstanceFixture.h): the zone handlers do not read the
// map, so the cases run on a plain instance of Haramel's map with a GeneralInstanceHandler.

#include "../handlers_inst_1/InstanceFixture.h"

#include "aion/gameserver/instance/handlers/GeneralInstanceHandler.h"

namespace aion::gameserver::handlers::instance::test {

inline constexpr int32_t ZONE_TEST_MAP = 300200000;

/** world_maps.xml's row of the map the zone cases run on (Haramel) */
inline constexpr const char* Z1_MAPS_XML = R"x(<world_maps>
	<map id="300200000" cName="IDNovice" name="Haramel" name_id="401728" instance="true" water_level="16" death_level="0" world_size="512" drop_type="OTHER_INSTANCES" flags="GLIDE PVP DUEL_SAME_RACE NO_RETURN_BATTLE" pve_attack_ratio="150" pve_defend_ratio="50"/>
</world_maps>)x";

inline const char* instanceMapsXml() {
	return Z1_MAPS_XML;
}

/** A plain npc, the killer or a bystander of the PvP zone cases */
inline const std::vector<int32_t>& handlerNpcIds() {
	static const std::vector<int32_t> ids{900200};
	return ids;
}

inline std::string handlerNpcAi(int32_t) {
	return "";
}

/** The fixture's instance factory: a plain GeneralInstanceHandler (no handler under test is an instance handler) */
inline runtime::Ref<::aion::gameserver::instance::handlers::InstanceHandler> generalInstanceFactory(world::WorldMapInstance& instance) {
	return ::aion::gameserver::instance::handlers::GeneralInstanceHandler::create(instance);
}

} // namespace aion::gameserver::handlers::instance::test
