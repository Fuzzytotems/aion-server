#pragma once

// Chunk I2's instance maps, npc ids and handler factories for the shared fixture (tests/handlers_inst_1/InstanceFixture.h).

#include "../handlers_inst_1/InstanceFixture.h"

namespace aion::gameserver::handlers::instance {

// the marker factories of the handlers under test (AION_INSTANCE_HANDLER defines them in the handler library)
::aion::gameserver::handlers::InstanceFactory StonespearReachInstance_instanceFactory;
::aion::gameserver::handlers::InstanceFactory DarkPoetaInstance_instanceFactory;
::aion::gameserver::handlers::InstanceFactory IlluminaryObeliskInstance_instanceFactory;
::aion::gameserver::handlers::InstanceFactory InfernalIlluminaryObelisk_instanceFactory;

} // namespace aion::gameserver::handlers::instance

namespace aion::gameserver::handlers::instance::crucible {
::aion::gameserver::handlers::InstanceFactory CrucibleChallengeInstance_instanceFactory;
::aion::gameserver::handlers::InstanceFactory EmpyreanCrucibleInstance_instanceFactory;
} // namespace aion::gameserver::handlers::instance::crucible

namespace aion::gameserver::handlers::instance::test {

inline constexpr int32_t STONESPEAR_REACH = 301500000;
inline constexpr int32_t CRUCIBLE_CHALLENGE = 300320000;
inline constexpr int32_t EMPYREAN_CRUCIBLE = 300300000;
inline constexpr int32_t DARK_POETA = 300040000;
inline constexpr int32_t ILLUMINARY_OBELISK = 301230000;
inline constexpr int32_t INFERNAL_ILLUMINARY_OBELISK = 301370000;
using crucible::CrucibleChallengeInstance_instanceFactory;
using crucible::EmpyreanCrucibleInstance_instanceFactory;

/** world_maps.xml's rows of the I2 maps under test (world_maps.xml:111, 60, 59, 32, 94, 107) */
inline constexpr const char* I2_MAPS_XML = R"x(<world_maps>
	<map id="301500000" cName="IDLegion" name="Stonespear Reach" name_id="404646" instance="true" water_level="90" death_level="0" world_size="512" drop_type="NONE" flags="BIND RECALL GLIDE PVP DUEL_SAME_RACE NO_RETURN_BATTLE"/>
	<map id="300320000" cName="IDArena_Solo" name="Crucible Challenge" name_id="401841" instance="true" water_level="16" death_level="0" world_size="2048" drop_type="NONE" flags="PVP DUEL_SAME_RACE NO_RETURN_BATTLE" pve_attack_ratio="150" pve_defend_ratio="50"/>
	<map id="300300000" cName="IDArena" name="Empyrean Crucible" name_id="401828" instance="true" water_level="16" death_level="0" world_size="2048" drop_type="NONE" flags="PVP DUEL_SAME_RACE NO_RETURN_BATTLE" pve_attack_ratio="150" pve_defend_ratio="50"/>
	<map id="300040000" cName="IDLF1" name="Dark Poeta" name_id="401172" instance="true" water_level="100" death_level="0" world_type="ABYSS" world_size="1536" drop_type="ABYSS_INSTANCE" flags="GLIDE PVP DUEL_SAME_RACE NO_RETURN_BATTLE" pve_attack_ratio="150" pve_defend_ratio="50"/>
	<map id="301230000" cName="IDLDF5_Under_03" name="Illuminary Obelisk" name_id="404057" instance="true" water_level="16" death_level="235" world_size="512" drop_type="BALAUREA_HIGH_INSTANCE" flags="GLIDE PVP DUEL_SAME_RACE NO_RETURN_BATTLE"/>
	<map id="301370000" cName="IDLDF5_Under_03_H" name="Infernal Illuminary Obelisk" name_id="404404" instance="true" water_level="16" death_level="0" world_size="512" drop_type="BALAUREA_HIGH_INSTANCE" flags="GLIDE PVP DUEL_SAME_RACE NO_RETURN_BATTLE"/>
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

/** Empyrean Crucible's npc ids (its handler's), less the ones Crucible Challenge already lists */
inline const std::vector<int32_t>& empyreanCrucibleNpcs() {
	static const std::vector<int32_t> ids{205331, 205332, 205333, 205334, 205335, 205336, 205338, 205339, 205340, 205341, 205342, 205413,
		205414, 205426, 205427, 205428, 205429, 205430, 211984, 217486, 217487, 217489, 217491, 217492, 217493, 217501, 217502, 217503, 217504,
		217505, 217506, 217507, 217508, 217509, 217510, 217511, 217512, 217513, 217514, 217515, 217516, 217517, 217518, 217519, 217520, 217521,
		217522, 217523, 217524, 217525, 217526, 217527, 217528, 217545, 217547, 217548, 217549, 217550, 217552, 217553, 217554, 217556, 217557,
		217558, 217559, 217560, 217561, 217562, 217563, 217564, 217565, 217566, 217567, 217568, 217569, 217570, 217572, 217573, 217576, 217577,
		217578, 217579, 217580, 217581, 217582, 217583, 217584, 217585, 217586, 217587, 217588, 217590, 217591, 217592, 217593, 217594, 217595,
		217596, 217597, 217651, 217652, 217653, 217654, 217735, 217737, 217740, 217741, 217742, 217743, 217744, 217745, 217746, 217747, 217748,
		217749, 217750, 217753, 217756, 217778, 280790, 281108, 281109, 281110, 281111, 281112, 281113, 281114, 281191, 281192, 281193, 281194,
		281322, 700527, 700528, 799567, 799568, 799569, 799573};
	return ids;
}

/** Dark Poeta's npc ids (its handler's and the darkPoeta AIs'), less the ones listed above */
inline const std::vector<int32_t>& darkPoetaNpcs() {
	static const std::vector<int32_t> ids{214841, 214842, 214843, 214849, 214850, 214851, 214864, 214871, 214880, 214883, 214885, 214894,
		214895, 214896, 214897, 214904, 215262, 215263, 215280, 215281, 215282, 215283, 215284, 215386, 215387, 215388, 215389, 215428, 215429,
		215430, 215431, 215432, 281116, 281148, 281149, 281150, 281151, 281178, 281258, 281259, 281267, 281268, 281334, 700439, 700440, 700441,
		700442, 700443, 700444, 700445, 700446, 700447, 700478, 700516, 700517, 700518, 700520, 700556, 700558, 730211};
	return ids;
}

/** The Illuminary Obelisks' npc ids (the handlers' and the illuminaryObelisk AIs'), less the ones listed above */
inline const std::vector<int32_t>& obeliskNpcs() {
	static const std::vector<int32_t> ids{233720, 233721, 233722, 233723, 233724, 233725, 233726, 233727, 233728, 233729, 233730, 233731,
		233732, 233733, 233734, 233735, 233736, 233737, 233738, 233739, 233740, 233857, 233880, 233881, 233882, 233883, 234682, 234683, 234684,
		234685, 234686, 234687, 234688, 234689, 234720, 234721, 234722, 234723, 234724, 234725, 234726, 234727, 234728, 234729, 234730, 234731,
		234732, 234733, 234734, 234738, 702009, 702014, 702015, 702016, 702017, 702216, 702218, 702219, 702220, 702221, 702222, 702223, 702224,
		702225, 702226, 702227, 702228, 702229, 730886, 730905, 802048, 802049};
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
		all.insert(all.end(), empyreanCrucibleNpcs().begin(), empyreanCrucibleNpcs().end());
		all.insert(all.end(), {205337, 205343, 205344, 282363, 282364, 282366, 282367, 282368, 282373, 282378}); // the empyreanCrucible AIs' own
		all.insert(all.end(), darkPoetaNpcs().begin(), darkPoetaNpcs().end());
		all.insert(all.end(), obeliskNpcs().begin(), obeliskNpcs().end());
		all.insert(all.end(), {702010, 702011, 702012}); // the generators (the obelisk AI cases place them)
		return all;
	}();
	return ids;
}

/** No ai names: every npc has a DummyAI, the AI cases install their AI (I2AiTest.cpp) */
inline std::string handlerNpcAi(int32_t) {
	return "";
}

} // namespace aion::gameserver::handlers::instance::test
