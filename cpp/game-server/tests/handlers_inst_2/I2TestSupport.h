#pragma once

// Chunk I2's instance maps, npc ids and handler factories for the shared fixture (tests/handlers_inst_1/InstanceFixture.h).

#include "../handlers_inst_1/InstanceFixture.h"

namespace aion::gameserver::handlers::instance {

// the marker factories of the handlers under test (AION_INSTANCE_HANDLER defines them in the handler library)
::aion::gameserver::handlers::InstanceFactory StonespearReachInstance_instanceFactory;

} // namespace aion::gameserver::handlers::instance

namespace aion::gameserver::handlers::instance::crucible {
::aion::gameserver::handlers::InstanceFactory CrucibleChallengeInstance_instanceFactory;
} // namespace aion::gameserver::handlers::instance::crucible

namespace aion::gameserver::handlers::instance::test {

inline constexpr int32_t STONESPEAR_REACH = 301500000;
inline constexpr int32_t CRUCIBLE_CHALLENGE = 300320000;
using crucible::CrucibleChallengeInstance_instanceFactory;

/** world_maps.xml's rows of the I2 maps under test (world_maps.xml:111, 60) */
inline constexpr const char* I2_MAPS_XML = R"x(<world_maps>
	<map id="301500000" cName="IDLegion" name="Stonespear Reach" name_id="404646" instance="true" water_level="90" death_level="0" world_size="512" drop_type="NONE" flags="BIND RECALL GLIDE PVP DUEL_SAME_RACE NO_RETURN_BATTLE"/>
	<map id="300320000" cName="IDArena_Solo" name="Crucible Challenge" name_id="401841" instance="true" water_level="16" death_level="0" world_size="2048" drop_type="NONE" flags="PVP DUEL_SAME_RACE NO_RETURN_BATTLE" pve_attack_ratio="150" pve_defend_ratio="50"/>
</world_maps>)x";

inline const char* instanceMapsXml() {
	return I2_MAPS_XML;
}

/** Stonespear Reach's npc ids by role (StonespearReachInstance.java:100-206) */
inline const std::vector<int32_t>& stonespearTrash() {
	static const std::vector<int32_t> ids{855765, 855766, 855767, 855768, 855769, 855770, 855771, 855772, 855773, 855788, 855789, 855790, 855791,
		855792, 855793, 855794, 855795, 855796, 855811, 855812, 855813, 855814, 855815, 855816, 855817, 855818, 855819, 855834, 855835, 855836,
		855837, 855838, 855839, 855840, 855841, 855842};
	return ids;
}

inline const std::vector<int32_t>& stonespearStones() {
	static const std::vector<int32_t> ids{855763, 855786, 855809, 855832, 856466, 856467, 856468, 856469};
	return ids;
}

/** Crucible Challenge's npc ids: the ones its handler and the crucibleChallenge AIs spawn, kill or talk through */
inline const std::vector<int32_t>& crucibleChallengeNpcs() {
	static const std::vector<int32_t> ids{205663, 205666, 205667, 205668, 205669, 205670, 205671, 205672, 205673, 205674, 205675, 205676, 205677,
		205678, 205679, 205682, 205683, 205684, 205685, 205686, 205687, 217758, 217783, 217784, 217785, 217786, 217787, 217788, 217789, 217790,
		217791, 217792, 217793, 217794, 217795, 217797, 217798, 217799, 217800, 217801, 217802, 217803, 217806, 217807, 217808, 217809, 217810,
		217811, 217812, 217813, 217814, 217815, 217816, 217819, 217820, 217827, 217828, 217829, 217837, 217838, 217840, 217841, 217842, 217843,
		217844, 217845, 217847, 217848, 218185, 218190, 218191, 218560, 218561, 218562, 218565, 218571, 730459, 730460};
	return ids;
}

/** The npc ids the I2 handlers spawn or kill */
inline const std::vector<int32_t>& handlerNpcIds() {
	static const std::vector<int32_t> ids = [] {
		std::vector<int32_t> all{833284, 855764, 855787, 855810, 855833, 856303, 856305, 855774, 855775, 855776, 855797, 855798, 855799, 855820,
			855821, 855822, 855843};
		all.insert(all.end(), stonespearTrash().begin(), stonespearTrash().end());
		all.insert(all.end(), stonespearStones().begin(), stonespearStones().end());
		all.insert(all.end(), crucibleChallengeNpcs().begin(), crucibleChallengeNpcs().end());
		return all;
	}();
	return ids;
}

/** No ai names: every npc has a DummyAI, the AI cases install their AI (I2AiTest.cpp) */
inline std::string handlerNpcAi(int32_t) {
	return "";
}

} // namespace aion::gameserver::handlers::instance::test
