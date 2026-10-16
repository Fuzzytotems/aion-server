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

} // namespace aion::gameserver::handlers::instance

namespace aion::gameserver::handlers::instance::test {

inline constexpr int32_t ATURAM_SKY_FORTRESS = 300240000;

/** world_maps.xml's rows of the I4 maps under test */
inline constexpr const char* I4_MAPS_XML = R"x(<world_maps>
	<map id="300240000" cName="IDStation" name="Aturam Sky Fortress" name_id="401780" instance="true" water_level="16" death_level="0" world_size="1024" drop_type="BALAUREA_HIGH_INSTANCE" flags="RECALL GLIDE PVP DUEL_SAME_RACE NO_RETURN_BATTLE" pve_attack_ratio="150" pve_defend_ratio="50"/>
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
