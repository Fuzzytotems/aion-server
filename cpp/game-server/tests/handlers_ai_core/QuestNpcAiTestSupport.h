#pragma once

// Test support of the two npc AIs of the retail ascension route (the ascension lane, on top of the M5d engine overlay): AbyssGuardSimpleAI
// ("simple_abyssguard", m5d-plan.md A-01; P5-05) and AscensationNpcAI ("ascensationquestnpc"; chunk A1, under this lane's file lease on
// handlers/ai/quests/AscensationNpcAI.*). AiWorldTest plus the shipped rows below.
//
// Every row is a VERBATIM excerpt of the Java tree's data/static_data (file:line at each), never an invented one (the M5b-1 lesson recorded in
// tests/ai/AiWorldTestSupport.h). The one edit is named where it is made: the <equipment> element of the npc templates is left out, because its
// item IDREFs need the item templates bound in the same context and nothing these cases drive reads an npc's gear (the precedent is
// tests/ai/NpcSkillTestSupport.h). TRIBE_RELATIONS_DATA holds only the real rows below; NPC_DATA keeps AiWorldTest's own templates beside them.
//
// The npcs:
// - 203752 Jucleas (Sanctum; tribe GUARD, type ABYSS_GUARD, srange 7, sangle 300): the guard of quest 1007 whose click did nothing without
//   the AI (the ascension analysis, "M5d stage 1a" row).
// - 204075 Balder (Pandaemonium; tribe GUARD_DARK, level 55): the Asmodian twin of the same row, and Jucleas' enemy - TribeRelationService's
//   hard-coded `GUARD -> GUARD_DARK` arm makes the pair aggressive (TribeRelationService.java:37-44).
// - 204275 "shadow court" (tribe GUARD_DARK, level 1): the only difference to Balder that matters here is the level, below the guard AI's 2.
// - 203725 Leah (Sanctum; tribe GENERAL): an npc Jucleas is no enemy of.
// - 250109 "balaur monster falling from carrier level 2" (tribe GUARD_DRAGON, level 2): an enemy exactly at the guard AI's level floor -
//   TribeRelationService's hard-coded `GUARD -> GUARD_DRAGON` arm makes Jucleas aggressive to it (TribeRelationService.java:37-44).
// - 211042 raider (the solo instance of quest 1006; tribe AGGRESSIVESINGLEMONSTER, ai ascensationquestnpc, srange 20, no sangle = 360).
// - 211043 orissan (the boss of the same instance, ai ascensationquestnpc): the one of the four with an npc skill, 16526 (npc_skills.xml:5068-5070).

#include <gtest/gtest.h>

#include <cstdint>
#include <memory>
#include <string>

#include "aion/commons/utils/Rnd.h"
#include "aion/gameserver/ai/AIState.h"
#include "aion/gameserver/ai/AbstractAI.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/NpcData.bind.h"
#include "aion/gameserver/dataholders/NpcData.h"
#include "aion/gameserver/dataholders/TribeRelationsData.bind.h"
#include "aion/gameserver/dataholders/TribeRelationsData.h"
#include "aion/gameserver/handlers/HandlerRegistry.h"
#include "aion/gameserver/model/gameobjects/Npc.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/sched/DeterministicExecutor.h"

#include "../ai/AiWorldTestSupport.h"

namespace aion::gameserver::ai::testing {

inline constexpr int32_t JUCLEAS = 203752;
inline constexpr int32_t BALDER = 204075;
inline constexpr int32_t SHADOW_COURT = 204275;
inline constexpr int32_t LEAH = 203725;
inline constexpr int32_t BALAUR_LEVEL_2 = 250109;
inline constexpr int32_t RAIDER = 211042;
inline constexpr int32_t ORISSAN = 211043;

/** npcs/npc_templates.xml, verbatim except the <equipment> of each row (see the file comment) */
inline const char* const QUEST_NPC_AI_TEMPLATES_XML =
	// npcs/npc_templates.xml:9953-9968 (without the <equipment> of seven items)
	R"(<npc_template npc_id="203752" level="55" name="jucleas" name_id="351244" height="2" title_id="370041" group_drop="LIGHT")"
	R"( rank="DISCIPLINED" rating="NORMAL" race="ELYOS" tribe="GUARD" type="ABYSS_GUARD" ai="simple_abyssguard" srange="7" sangle="300")"
	R"( arange="2" attack_speed="2000" hpgauge="3"><stats maxHp="18756"><speeds walk="1.5" group_walk="1.5" run="6" run_fight="4.2")"
	R"( group_run_fight="4.2" /></stats><bound_radius front="0.25" side="0.35" upper="2" />)"
	R"(<talk_info distance="5" is_dialog="true" can_talk_invisible="false" /></npc_template>)"
	// npcs/npc_templates.xml:14565-14580 (without the <equipment> of seven items)
	R"(<npc_template npc_id="204075" level="55" name="balder" name_id="352317" height="2" title_id="370041" group_drop="DARK")"
	R"( rank="DISCIPLINED" rating="NORMAL" race="ASMODIANS" tribe="GUARD_DARK" type="ABYSS_GUARD" ai="simple_abyssguard" srange="7")"
	R"( sangle="300" arange="2" attack_speed="2000" hpgauge="3"><stats maxHp="18756"><speeds walk="1.5" group_walk="1.5" run="6")"
	R"( run_fight="4.2" group_run_fight="4.2" /></stats><bound_radius front="0.25" side="0.35" upper="2" />)"
	R"(<talk_info distance="5" is_dialog="true" can_talk_invisible="false" /></npc_template>)"
	// npcs/npc_templates.xml:17500-17513 (without the <equipment> of five items)
	R"(<npc_template npc_id="204275" level="1" name="shadow court" name_id="352536" height="2" group_drop="DARK" rank="DISCIPLINED")"
	R"( rating="NORMAL" race="ASMODIANS" tribe="GUARD_DARK" type="ABYSS_GUARD" ai="simple_abyssguard" srange="7" sangle="300" arange="2")"
	R"( attack_speed="2000" hpgauge="3"><stats maxHp="18756"><speeds walk="1.5" group_walk="1.5" run="6" run_fight="4.2")"
	R"( group_run_fight="4.2" /></stats><bound_radius front="0.25" side="0.35" upper="2" />)"
	R"(<talk_info distance="5" can_talk_invisible="false" /></npc_template>)"
	// npcs/npc_templates.xml:9587-9598 (without the <equipment> of three items)
	R"(<npc_template npc_id="203725" level="40" name="leah" name_id="351406" height="1.44" title_id="350442" group_drop="NONE")"
	R"( rank="DISCIPLINED" rating="NORMAL" race="ELYOS" tribe="GENERAL" type="GENERAL" ai="general" srange="10" sangle="240")"
	R"( attack_speed="2000" hpgauge="3"><stats maxHp="9426"><speeds walk="1.3" group_walk="1.68" run="6" run_fight="4.2")"
	R"( group_run_fight="4.2" /></stats><bound_radius front="0.2" side="0.28" upper="1.44" />)"
	R"(<talk_info distance="5" is_dialog="true" can_talk_invisible="false" /></npc_template>)"
	// npcs/npc_templates.xml:60615-60623 (without <equipment><item>100000018</item></equipment>)
	R"(<npc_template npc_id="211042" level="9" name="raider" name_id="302770" height="2.8860002" title_id="355021")"
	R"( group_drop="LIZARDMANFIGHTER" rank="NOVICE" rating="NORMAL" race="NAGA" tribe="AGGRESSIVESINGLEMONSTER" type="MONSTER")"
	R"( ai="ascensationquestnpc" srange="20" arange="2" attack_speed="2356" hpgauge="2"><stats maxHp="177"><speeds walk="1.76")"
	R"( group_walk="1.76" run="7" run_fight="6" group_run_fight="7" /></stats><bound_radius front="1.025" side="2" upper="2.886" />)"
	R"(</npc_template>)"
	// npcs/npc_templates.xml:60624-60632 (without <equipment><item>100900013</item></equipment>)
	R"(<npc_template npc_id="211043" level="9" name="orissan" name_id="355006" height="4.6" title_id="355007" group_drop="NAGAWARRIOR")"
	R"( rank="SEASONED" rating="NORMAL" race="NAGA" tribe="AGGRESSIVESINGLEMONSTER" type="MONSTER" ai="ascensationquestnpc" srange="20")"
	R"( arange="2" attack_speed="2142" hpgauge="4" cancel_level="90"><stats maxHp="1461"><speeds walk="1" group_walk="1" run="7")"
	R"( run_fight="6" group_run_fight="7" /></stats><bound_radius front="2" side="3.8" upper="4.6" /></npc_template>)"
	// npcs/npc_templates.xml:199779-199788 (without the <equipment> of two items)
	R"(<npc_template npc_id="250109" level="2" name="balaur monster falling from carrier level 2" name_id="301534" height="2.3")"
	R"( group_drop="DRAKANFIGHTER" rank="DISCIPLINED" rating="NORMAL" race="DRAKAN" tribe="GUARD_DRAGON" type="ABYSS_GUARD" abyss_type="RAID")"
	R"( ai="aggressive" srange="10" sangle="270" arange="2" attack_speed="2100" cast_speed="100" hpgauge="3" floatcorpse="true">)"
	R"(<stats maxHp="4162"><speeds walk="1.07" group_walk="1.07" run="8" run_fight="6" group_run_fight="8" /></stats>)"
	R"(<bound_radius front="0.75" side="0.475" upper="2.3" /></npc_template>)";

/** tribe/tribe_relations.xml, verbatim: the rows of every tribe the npcs above and the two races of the players have */
inline const char* const QUEST_NPC_AI_TRIBE_RELATIONS_XML =
	R"(<tribe_relations>)"
	// tribe/tribe_relations.xml:29-31
	R"(<tribe name="AGGRESSIVESINGLEMONSTER" base="MONSTER"><aggro>PC PC_DARK</aggro></tribe>)"
	// tribe/tribe_relations.xml:719-721
	R"(<tribe name="GENERAL"><none>NEUTRAL_DGUARD YDUMMY_DGUARD YDUMMY2_DGUARD LDF4B_SPARRING_DGUARD LDF4B_SPARRING_DGUARD2)"
	R"( LDF5_DUMMY1_DGUARD LDF5_DUMMY2_DGUARD LDF5_SPARRING1_DGUARD LDF5_SPARRING2_DGUARD</none></tribe>)"
	// tribe/tribe_relations.xml:805-807
	R"(<tribe name="GUARD"><friend>DUMMY DUMMY2</friend></tribe>)"
	// tribe/tribe_relations.xml:817-819
	R"(<tribe name="GUARD_DARK"><friend>DUMMY DUMMY2</friend></tribe>)"
	// tribe/tribe_relations.xml:829-831
	R"(<tribe name="GUARD_DRAGON"><friend>USEALL_TELEPORTER_LI USEALL_TELEPORTER_DA</friend></tribe>)"
	// tribe/tribe_relations.xml:2170-2173
	R"(<tribe name="MONSTER"><hostile>YUN_GUARD</hostile><friend>POLYMORPHPARROT USEALL_TELEPORTER_LI USEALL_TELEPORTER_DA</friend></tribe>)"
	// tribe/tribe_relations.xml:2302-2310
	R"(<tribe name="PC"><friend>LIGHT_SUR_MOB LIGHT_LICH</friend><none>LASBERG NEUTRAL_DGUARD YDUMMY_DGUARD YDUMMY2_DGUARD)"
	R"( LDF4B_SPARRING_DGUARD LDF4B_SPARRING_DGUARD2 XDRAKAN_UNATTACK LDF5_DUMMY1_DGUARD LDF5_DUMMY2_DGUARD LDF5_SPARRING1_DGUARD)"
	R"( LDF5_SPARRING2_DGUARD</none></tribe>)"
	R"(<tribe name="PC_DARK"><friend>DARK_SUR_MOB DARK_LICH</friend><neutral>FIELD_OBJECT_ALL FIELD_OBJECT_ALL_HOSTILEMONSTER</neutral>)"
	R"(<none>NEUTRAL_LGUARD YDUMMY_LGUARD YDUMMY2_LGUARD LDF4B_SPARRING_GUARD LDF4B_SPARRING_GUARD2 XDRAKAN_UNATTACK LDF5_DUMMY1_LGUARD)"
	R"( LDF5_DUMMY2_LGUARD LDF5_SPARRING1_LGUARD LDF5_SPARRING2_LGUARD</none></tribe>)"
	R"(</tribe_relations>)";

/**
 * AiWorldTest with the shipped rows above, one active map region around (500, 500) and a saved Rnd generator (the attack cases seed it). This
 * executable links the empty AI registry (a handler target's tests do, AionChunks.cmake), so, as RootAiHandlersTest does for the root
 * handlers, each case reaches an AION_AI marker through the factory function it defines and gives the npc the AI with replaceAi.
 */
class QuestNpcAiWorldTest : public AiWorldTest {
protected:
	void SetUp() override {
		AiWorldTest::SetUp();
		savedGenerator = commons::utils::Rnd::generator();
		std::string npcTemplates = aiNpcTemplatesXml();
		npcTemplates.insert(npcTemplates.rfind("</npc_templates>"), QUEST_NPC_AI_TEMPLATES_XML);
		dataholders::DataManager::NPC_DATA.resetForTests();
		dataholders::DataManager::NPC_DATA.publish(xml::bindString<dataholders::NpcData>(contexts.emplace_back(), npcTemplates));
		dataholders::DataManager::TRIBE_RELATIONS_DATA.resetForTests();
		dataholders::DataManager::TRIBE_RELATIONS_DATA.publish(
			xml::bindString<dataholders::TribeRelationsData>(contexts.emplace_back(), QUEST_NPC_AI_TRIBE_RELATIONS_XML));
		AI_TEST_SCOPE;
		// Java: a map region is only active while a player is in it, and both aggro checks return early otherwise
		regionActivator = activateRegionAt(500, 500, 100);
		executor->runReady(); // MapRegion::activate posts the ACTIVATE notification of its creatures; drain it before the cases count tasks
	}

	void TearDown() override {
		regionActivator = nullptr;
		AiWorldTest::TearDown();
		commons::utils::Rnd::generator() = savedGenerator;
	}

	/**
	 * Gives the npc the AI a factory built, in the state a spawned npc's AI is in (a new AI is CREATED, which handles only the spawn events;
	 * the spawn's SPAWNED event moves it to IDLE). Returns the AI as the class the case expects, failing the case if the factory built another.
	 */
	template <class AI>
	AI& installFrom(::aion::gameserver::handlers::AIFactory& factory, model::gameobjects::Npc& npc) {
		std::unique_ptr<AbstractAI> ai = factory(npc);
		AI* typed = dynamic_cast<AI*>(ai.get());
		EXPECT_NE(typed, nullptr);
		npc.replaceAi(std::move(ai));
		typed->setStateIfNot(AIState::IDLE);
		return *typed;
	}

	/** Gives the npc a test leaf of a handler class (a recording or probing subclass), idle */
	template <class AI>
	AI& installLeaf(model::gameobjects::Npc& npc) {
		auto ai = std::make_unique<AI>(npc);
		AI& result = *ai;
		npc.replaceAi(std::move(ai));
		result.setStateIfNot(AIState::IDLE);
		return result;
	}

	/** An Asmodian character at (x, y, z): makeWorldPlayer creates an Elyos, and Player::getRace reads the common data (Player.cpp) */
	runtime::Ref<model::gameobjects::player::Player> makeAsmodian(int32_t objectId, float x, float y, float z) {
		runtime::Ref<model::gameobjects::player::Player> player = makeWorldPlayer(objectId, x, y, z);
		player->getCommonData()->setRace(model::Race::ASMODIANS);
		return player;
	}

	runtime::Ref<model::gameobjects::player::Player> regionActivator;
	commons::utils::Rnd::Xoshiro256PlusPlus savedGenerator{0};
};

} // namespace aion::gameserver::ai::testing
