#pragma once

// Lane C's zone maps, npc ids and helpers for ZoneA1Fixture.h: the zone bosses and world NPCs of A1 (handlers/ai/worlds/{inggison, heiron,
// kaldor, gelkmaros, levinshor, pernon, eltnen, brusthonin}). One definition of the fixture's three hooks per test executable: this header.

#include "ZoneA1Fixture.h"

#include <initializer_list>
#include <memory>
#include <string>
#include <vector>

#include "aion/gameserver/ai/AIState.h"
#include "aion/gameserver/dataholders/SkillData.bind.h"
#include "aion/gameserver/dataholders/SkillData.h"

namespace aion::gameserver::handlers::ai::worlds::test {

inline constexpr int32_t ELTNEN = 210020000;
inline constexpr int32_t HEIRON = 210040000;
inline constexpr int32_t INGGISON = 210050000;
inline constexpr int32_t BRUSTHONIN = 220050000;
inline constexpr int32_t GELKMAROS = 220070000;
inline constexpr int32_t KALDOR = 600090000;
inline constexpr int32_t LEVINSHOR = 600100000;
inline constexpr int32_t PERNON = 710010000;

/** world_maps.xml's rows of the eight zones */
inline const char* zoneMapsXml() {
	return R"x(<world_maps>
	<map id="210020000" cName="LF2" name="Eltnen" name_id="400262" beginner_twin_count="5" max_user="200" water_level="6" death_level="0" world_type="ELYSEA" world_size="3072" drop_type="ELYSEA" flags="BIND RECALL GLIDE RIDE PVP DUEL_SAME_RACE" pve_attack_ratio="150" pve_defend_ratio="50"/>
	<map id="210040000" cName="LF3" name="Heiron" name_id="400263" beginner_twin_count="3" max_user="200" water_level="101" death_level="0" world_type="ELYSEA" world_size="3072" drop_type="ELYSEA" flags="BIND RECALL GLIDE RIDE PVP DUEL_SAME_RACE" pve_attack_ratio="150" pve_defend_ratio="50"/>
	<map id="210050000" cName="LF4" name="Inggison" name_id="401340" water_level="59" death_level="0" world_type="BALAUREA" world_size="3072" drop_type="BALAUREA" flags="BIND RECALL GLIDE RIDE PVP DUEL_SAME_RACE" pve_attack_ratio="150" pve_defend_ratio="50"/>
	<map id="220050000" cName="DF2A" name="Brusthonin" name_id="400933" twin_count="1" beginner_twin_count="2" max_user="200" water_level="5" death_level="0" world_type="ASMODAE" world_size="3072" drop_type="ASMODAE" flags="BIND RECALL GLIDE RIDE PVP DUEL_SAME_RACE" pve_attack_ratio="150" pve_defend_ratio="50"/>
	<map id="220070000" cName="DF4" name="Gelkmaros" name_id="401369" water_level="16" death_level="0" world_type="BALAUREA" world_size="3072" drop_type="BALAUREA" flags="BIND RECALL GLIDE RIDE PVP DUEL_SAME_RACE" pve_attack_ratio="150" pve_defend_ratio="50"/>
	<map id="600090000" cName="LDF5_Fortress" name="Kaldor" name_id="404064" water_level="1" death_level="0" world_type="BALAUREA" world_size="1536" drop_type="BALAUREA_HIGH" flags="BIND RECALL GLIDE RIDE PVP DUEL_SAME_RACE NO_RETURN_BATTLE"/>
	<map id="600100000" cName="LDF4_Advance" name="Levinshor" name_id="404076" max_user="200" water_level="225" death_level="0" world_type="BALAUREA" world_size="2048" drop_type="BALAUREA_HIGH" flags="BIND RECALL GLIDE RIDE PVP DUEL_SAME_RACE"/>
	<map id="710010000" cName="Housing_DF_personal" name="Pernon" name_id="401832" water_level="70" death_level="0" world_type="ASMODAE" world_size="3072" drop_type="ASMODAE" flags="RECALL GLIDE RIDE PVP DUEL_SAME_RACE"/>
</world_maps>)x";
}

inline const std::vector<int32_t>& zoneNpcIds() {
	static const std::vector<int32_t> ids{// two plain npcs the AIs under test are installed on
		900200, 900300,
		// inggison: the geyser, the hatchling of the huge egg, Sematariux, his thunder shields, his 282123, egg, bolt marks, tornadoes and hatchling
		700545, 217097, 216520, 281931, 281932, 281933, 282123, 281451, 281452, 281453, 281456,
		// heiron: Bollvig's 204655 and 212314, his summons, the Klawspawn's 212120
		204655, 212314, 280802, 280803, 280804, 212120,
		// kaldor: the map icon, Berserk Anoha, the Commanders Anoha
		702618, 855263, 804594, 804595,
		// gelkmaros: Padmarashka's rock slides and protectors
		281936, 281938, 281939, 281940, 281941,
		// levinshor: the four ancient monsters
		235217, 235218, 235219, 235220};
	return ids;
}

inline std::string zoneNpcAi(int32_t /*npcId*/) {
	return "";
}

/** The fixture with a zone instance, the player standing in it, and the helpers the slice tests share */
class ZoneA1AiTest : public ZoneAiTest {
protected:
	void open(int32_t mapId, float x, float y, float z) {
		instance = &create(mapId);
		enter(*instance, x, y, z);
	}

	/** The AI installed on the npc in the IDLE state (replaceAi: the template's AI is a DummyAI) */
	template <class AI>
	AI& install(model::gameobjects::Npc& npc) {
		auto ai = std::make_unique<AI>(npc);
		AI& typed = *ai;
		npc.replaceAi(std::move(ai));
		typed.setStateIfNot(gameserver::ai::AIState::IDLE);
		return typed;
	}

	runtime::Ptr<model::gameobjects::Npc> place(int32_t npcId, float x, float y, float z) { return spawnNpc(*instance, npcId, x, y, z, int8_t{0}); }

	/** A single-time spawn with a creator, as AbstractAI.spawn makes one (the creator id the spawned npc's AI reads) */
	runtime::Ptr<model::gameobjects::Npc> placeCreatedBy(model::gameobjects::VisibleObject& creator, int32_t npcId, float x, float y, float z) {
		runtime::Ref<model::templates::spawns::SpawnTemplate> spawn = spawnengine::SpawnEngine::newSingleTimeSpawn(instance->getMapId(), npcId, x, y,
			z, int8_t{0}, runtime::Ptr<model::gameobjects::VisibleObject>(creator), std::nullopt);
		return runtime::as<model::gameobjects::Npc>(spawnengine::SpawnEngine::spawnObject(*spawn, instance->getInstanceId()));
	}

	size_t count(int32_t npcId) { return npcs(*instance, npcId).size(); }

	/** SKILL_DATA with one plain template per id (the base fixture publishes an empty holder; its TearDown resets this one) */
	void publishSkills(std::initializer_list<int32_t> ids) {
		dataholders::DataManager::SKILL_DATA.resetForTests();
		std::string skills = "<skill_data>";
		for (int32_t id : ids)
			skills += R"(<skill_template skill_id=")" + std::to_string(id) +
				R"(" name="zone" nameId="1" stack="ZONE" lvl="1" skilltype="MAGICAL" skillsubtype="NONE" tslot="SPEC" activation="ACTIVE")"
				R"( cooldown="0" duration="0"><properties first_target="ME" target_relation="FRIEND" target_type="ONLYONE" /></skill_template>)";
		dataholders::DataManager::SKILL_DATA.publish(xml::bindString<dataholders::SkillData>(skillContext, skills + "</skill_data>"));
	}

	xml::LoadContext skillContext;
	world::WorldMapInstance* instance = nullptr;
};

} // namespace aion::gameserver::handlers::ai::worlds::test
