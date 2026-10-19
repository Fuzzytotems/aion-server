#pragma once

// Chunk I4's instance maps, npc ids and handler factories for the shared fixture (tests/handlers_inst_1/InstanceFixture.h).

#include "../handlers_inst_1/InstanceFixture.h"

#include <initializer_list>
#include <string>

#include "aion/gameserver/dataholders/SkillData.bind.h"
#include "aion/gameserver/dataholders/SkillData.h"

namespace aion::gameserver::handlers::instance {

// the marker factories of the handlers under test (AION_INSTANCE_HANDLER defines them in the handler library)
::aion::gameserver::handlers::InstanceFactory AturamSkyFortressInstance_instanceFactory;
::aion::gameserver::handlers::InstanceFactory BeshmundirInstance_instanceFactory;
::aion::gameserver::handlers::InstanceFactory DrakenspireDepthsInstance_instanceFactory;
::aion::gameserver::handlers::InstanceFactory DragonLordsRefugeInstance_instanceFactory;
::aion::gameserver::handlers::InstanceFactory AnguishedDragonLordsRefugeInstance_instanceFactory;

} // namespace aion::gameserver::handlers::instance

namespace aion::gameserver::handlers::instance::test {

inline constexpr int32_t ATURAM_SKY_FORTRESS = 300240000;
inline constexpr int32_t BESHMUNDIR_TEMPLE = 300170000;
inline constexpr int32_t DRAKENSPIRE_DEPTHS = 301390000;
inline constexpr int32_t DRAGON_LORDS_REFUGE = 300520000;
inline constexpr int32_t ANGUISHED_DRAGON_LORDS_REFUGE = 300630000;

/** world_maps.xml's rows of the I4 maps under test */
inline constexpr const char* I4_MAPS_XML = R"x(<world_maps>
	<map id="300240000" cName="IDStation" name="Aturam Sky Fortress" name_id="401780" instance="true" water_level="16" death_level="0" world_size="1024" drop_type="BALAUREA_HIGH_INSTANCE" flags="RECALL GLIDE PVP DUEL_SAME_RACE NO_RETURN_BATTLE" pve_attack_ratio="150" pve_defend_ratio="50"/>
	<map id="300170000" cName="IDCatacombs" name="Beshmundir Temple" name_id="401411" instance="true" water_level="100" death_level="0" world_size="2048" drop_type="BALAUREA_INSTANCE" flags="GLIDE PVP DUEL_SAME_RACE NO_RETURN_BATTLE" pve_attack_ratio="150" pve_defend_ratio="50"/>
	<map id="301390000" cName="IDSeal" name="Drakenspire Depths" name_id="404456" instance="true" water_level="16" death_level="0" world_size="1024" drop_type="BALAUREA_HIGH_INSTANCE" flags="GLIDE PVP DUEL_SAME_RACE NO_RETURN_BATTLE"/>
	<map id="300520000" cName="IDTiamat_2" name="Dragon Lord's Refuge" name_id="403523" instance="true" water_level="16" death_level="0" world_size="1024" drop_type="BALAUREA_HIGH_INSTANCE" flags="PVP DUEL_SAME_RACE NO_RETURN_BATTLE" pve_attack_ratio="150" pve_defend_ratio="50"/>
	<map id="300630000" cName="IDTiamat_2_Hard" name="Anguished Dragon Lord's Refuge" name_id="404664" instance="true" water_level="16" death_level="0" world_size="1024" drop_type="BALAUREA_HIGH_INSTANCE" flags="PVP DUEL_SAME_RACE NO_RETURN_BATTLE"/>
</world_maps>)x";

inline const char* instanceMapsXml() {
	return I4_MAPS_XML;
}

/** The npc ids the I4 handlers spawn or kill */
inline const std::vector<int32_t>& handlerNpcIds() {
	static const std::vector<int32_t> ids{
		// Aturam Sky Fortress and its AIs
		702651, 282281, 700982, 282279, 700983, 282278, 702650, 282277, 702652, 282280, 217373, 730375, 701043, 701030, 217371, 730374, 217370,
		217656, 217382, 218577, 701029, 217369, 217368, 217655, 730398, 730397, 730410, 731533, 731534, 217374, 217375, 799657,
		// Beshmundir Temple and its AIs
		216583, 216587, 799518, 216584, 216588, 799519, 216585, 216589, 799520, 216586, 216590, 216735, 216734, 216737, 216245, 799342, 216157,
		216238, 216159, 216165, 216246, 216739, 216740, 216158, 216239, 700608, 216263, 216182, 216183, 730275, 216250, 216169, 216527, 216206,
		216207, 216208, 216209, 216210, 216211, 216212, 216213, 216532, 730276, 281659, 281660, 281645, 281545, 281756, 216215,
		// Drakenspire Depths and its AIs
		209678, 209679, 209680, 209681, 209743, 209744, 209745, 209746, 236230, 236233, 236231, 236234, 702719, 702720, 236243, 236247, 702699,
		855460, 855461, 855462, 855463, 855464, 236225, 236226, 236227, 236228, 855708, 855709, 236232, 236229, 236224, 236239, 236240, 236241,
		236242, 236244, 236245, 236246, 833012, 833013, 236248, 236249, 731580, 731581, 731548, 209738, 209739, 209740, 209741, 209742, 209803,
		209804, 209805, 209806, 209807, 206405, 855607, 855608, 855699, 855700, 209705, 209770, 209704, 209769, 209711, 209776, 209709, 209774,
		209710, 209775, 209708, 209773, 236204, 236205, 236206, 236216, 236217, 236218, 236219, 236220, 207128, 207129, 855444, 855447, 855452,
		855454, 855456, 855458, 855621, 855622, 855625, 209690, 209755, 209693, 209758, 209695, 209760, 209694, 209759, 209731, 209796, 209720,
		209785, 209722, 209787, 209723, 209788, 209734, 209735, 209736, 209737, 209799, 209800, 209801, 209802, 855450, 855435, 856300, 855742,
		855745, 855746, 855747, 855748, 855749, 855750, 855751, 855752, 702695, 702696, 855446, 855448, 855449,
		// Dragon Lord's Refuge (and Anguished) and its AIs
		219359, 219360, 219361, 219362, 219365, 219366, 219367, 219368, 219488, 219489, 219490, 219491, 219492, 219493, 219532, 219533, 219534,
		219535, 219536, 219537, 219538, 219539, 283134, 283140, 283163, 283164, 283165, 283166, 730625, 730630, 730633, 730634, 730635, 730636,
		730673, 730674, 730675, 730676, 730694, 730695, 730696, 730699, 730700, 730704, 701542, 800341, 800350, 800356, 800430, 800464, 800465,
		236274, 236275, 236276, 236277, 236278, 236279, 236280, 236281, 856020, 856021, 856022, 856023, 856024, 856025, 856026, 856483, 856484,
		856485, 856486, 833482, 702729, 283130, 283131, 283132, 283133, 283135, 283139, 283141, 283142, 283143, 283237, 283238, 283240, 283241,
		283244, 282727, 282729, 282731, 731629, 731630, 856298, 856299, 856046, 856042, 856048, 219363,
		// two plain npcs
		900200, 900300};
	return ids;
}

inline std::string handlerNpcAi(int32_t) {
	return "";
}

/** Skill rows for the ids (the base fixture's holder is empty; its TearDown resets this one): self buffs, enough for getSkill and its use */
inline void publishSkills(std::initializer_list<int32_t> skillIds) {
	dataholders::DataManager::SKILL_DATA.resetForTests();
	xml::LoadContext skillContext;
	std::string skills = "<skill_data>";
	for (int32_t id : skillIds)
		skills += R"(<skill_template skill_id=")" + std::to_string(id) +
			R"(" name="i4" nameId="1" stack="I4_)" + std::to_string(id) + R"(" lvl="1" skilltype="MAGICAL" skillsubtype="NONE" tslot="BUFF")"
			R"( activation="ACTIVE" cooldown="0" duration="0"><properties first_target="ME" target_relation="FRIEND" target_type="ONLYONE" />)"
			R"(</skill_template>)";
	dataholders::DataManager::SKILL_DATA.publish(xml::bindString<dataholders::SkillData>(skillContext, skills + "</skill_data>"));
}

} // namespace aion::gameserver::handlers::instance::test
