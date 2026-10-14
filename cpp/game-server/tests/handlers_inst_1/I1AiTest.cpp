// P6I-2 (docs/design/p6-instances-plan.md §4): chunk I1's three small AI directories - azoturanFortress/BetrayerIcaronixAI,
// abyss/FortressInstanceDukeAI, abyss/IllusionGateAI, dredgion/ShulackDrudgeAI, dredgion/SurkanaAI (game-server/data/handlers/ai/instance) -
// on npcs of a real instance (Haramel's map: the AIs do not read the map), each AI installed with replaceAi. The test executable links P5-05's
// handler library for the AIs' base classes (CMakeLists.txt, p6-instances-plan.md §3).

#include "InstanceHandlerTestSupport.h"

#include <chrono>
#include <cmath>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "aion/gameserver/ai/AIState.h"
#include "aion/gameserver/ai/AbstractAI.h"
#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/dataholders/AIData.bind.h"
#include "aion/gameserver/dataholders/AIData.h"
#include "aion/gameserver/dataholders/SkillData.bind.h"
#include "aion/gameserver/dataholders/SkillData.h"
#include "aion/gameserver/handlers/ai/instance/abyss/FortressInstanceDukeAI.h"
#include "aion/gameserver/handlers/ai/instance/abyss/IllusionGateAI.h"
#include "aion/gameserver/handlers/ai/instance/azoturanFortress/BetrayerIcaronixAI.h"
#include "aion/gameserver/handlers/ai/instance/dredgion/ShulackDrudgeAI.h"
#include "aion/gameserver/handlers/ai/instance/dredgion/SurkanaAI.h"
#include "aion/gameserver/handlers/ai/instance/esoterrace/CaptainMuruganAI.h"
#include "aion/gameserver/handlers/ai/instance/esoterrace/DaliaCharlandsAI.h"
#include "aion/gameserver/handlers/ai/instance/esoterrace/GreenfingersAI.h"
#include "aion/gameserver/ai/poll/AIQuestion.h"
#include "aion/gameserver/handlers/ai/instance/esoterrace/KexkraPrototypeAI.h"
#include "aion/gameserver/model/templates/spawns/SpawnTemplate.h"
#include "aion/gameserver/handlers/ai/instance/esoterrace/WardenSuramaAI.h"
#include "aion/gameserver/handlers/ai/instance/theHexway/AdjutantGalamatAI.h"
#include "aion/gameserver/handlers/ai/instance/theHexway/CaptainJarkaAI.h"
#include "aion/gameserver/handlers/ai/instance/theHexway/CaptainLakharaAI.h"
#include "aion/gameserver/handlers/ai/instance/theHexway/ChestTeleporterAI.h"
#include "aion/gameserver/network/aion/serverpackets/SM_MESSAGE.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/network/aion/serverpackets/SM_PLAY_MOVIE.h"
#include "aion/gameserver/model/stats/container/NpcLifeStats.h"
#include "aion/gameserver/network/aion/serverpackets/SM_DIALOG_WINDOW.h"
#include "aion/gameserver/skillengine/model/SkillTemplate.h"

namespace aion::gameserver::handlers::ai::instance {
namespace azoturanFortress {
::aion::gameserver::handlers::AIFactory BetrayerIcaronixAI_aiFactory;
}
namespace abyss {
::aion::gameserver::handlers::AIFactory FortressInstanceDukeAI_aiFactory;
::aion::gameserver::handlers::AIFactory IllusionGateAI_aiFactory;
} // namespace abyss
namespace dredgion {
::aion::gameserver::handlers::AIFactory ShulackDrudgeAI_aiFactory;
::aion::gameserver::handlers::AIFactory SurkanaAI_aiFactory;
} // namespace dredgion
namespace esoterrace {
::aion::gameserver::handlers::AIFactory KexkraPrototypeAI_aiFactory;
::aion::gameserver::handlers::AIFactory WardenSuramaAI_aiFactory;
::aion::gameserver::handlers::AIFactory DaliaCharlandsAI_aiFactory;
::aion::gameserver::handlers::AIFactory GreenfingersAI_aiFactory;
::aion::gameserver::handlers::AIFactory CaptainMuruganAI_aiFactory;
} // namespace esoterrace
namespace theHexway {
::aion::gameserver::handlers::AIFactory AdjutantGalamatAI_aiFactory;
::aion::gameserver::handlers::AIFactory CaptainJarkaAI_aiFactory;
::aion::gameserver::handlers::AIFactory CaptainLakharaAI_aiFactory;
::aion::gameserver::handlers::AIFactory ChestTeleporterAI_aiFactory;
} // namespace theHexway
} // namespace aion::gameserver::handlers::ai::instance

namespace aion::gameserver::handlers::instance::test {
namespace {

namespace ais = ::aion::gameserver::handlers::ai::instance;
using model::gameobjects::Npc;

// the protected handlers the cases drive directly (Java: the AI's events)
struct IcaronixProbe : ais::azoturanFortress::BetrayerIcaronixAI {
	using BetrayerIcaronixAI::BetrayerIcaronixAI;
	using BetrayerIcaronixAI::handleAttack;
};
struct DukeProbe : ais::abyss::FortressInstanceDukeAI {
	using FortressInstanceDukeAI::FortressInstanceDukeAI;
	using FortressInstanceDukeAI::handleDespawned;
	using FortressInstanceDukeAI::handleDied;
};
struct GateProbe : ais::abyss::IllusionGateAI {
	using IllusionGateAI::IllusionGateAI;
	using IllusionGateAI::handleDied;
	using IllusionGateAI::handleSpawned;
};
struct DrudgeProbe : ais::dredgion::ShulackDrudgeAI {
	using ShulackDrudgeAI::ShulackDrudgeAI;
	using ShulackDrudgeAI::handleDialogStart;
};
struct SurkanaProbe : ais::dredgion::SurkanaAI {
	using SurkanaAI::SurkanaAI;
	using SurkanaAI::handleAttack;
};
struct KexkraProbe : ais::esoterrace::KexkraPrototypeAI {
	using KexkraPrototypeAI::KexkraPrototypeAI;
	using KexkraPrototypeAI::handleAttack;
};
struct WardenProbe : ais::esoterrace::WardenSuramaAI {
	using WardenSuramaAI::WardenSuramaAI;
	using WardenSuramaAI::handleAttack;
	using WardenSuramaAI::handleBackHome;
};
struct DaliaProbe : ais::esoterrace::DaliaCharlandsAI {
	using DaliaCharlandsAI::DaliaCharlandsAI;
	using DaliaCharlandsAI::handleAttack;
	int backHomes = 0;
	void handleBackHome() override {
		++backHomes;
		DaliaCharlandsAI::handleBackHome();
	}
};
struct MuruganProbe : ais::esoterrace::CaptainMuruganAI {
	using CaptainMuruganAI::CaptainMuruganAI;
	using CaptainMuruganAI::handleAttack;
	using CaptainMuruganAI::handleDied;
};
struct JarkaProbe : ais::theHexway::CaptainJarkaAI {
	using CaptainJarkaAI::CaptainJarkaAI;
	using CaptainJarkaAI::handleDied;
	using CaptainJarkaAI::handleSpawned;
};
struct ChestTeleporterProbe : ais::theHexway::ChestTeleporterAI {
	using ChestTeleporterAI::ChestTeleporterAI;
	using ChestTeleporterAI::handleCreatureMoved;
};
struct GalamatProbe : ais::theHexway::AdjutantGalamatAI {
	using AdjutantGalamatAI::AdjutantGalamatAI;
	using AdjutantGalamatAI::handleDied;
	using AdjutantGalamatAI::handleSpawned;
};
/** ai_templates (spawn_helpers.xml's shape) with an empty summons list for the npc: a SummonerAI's spawn reads it (SummonerAI.java:57-61) */
void publishEmptySummons(xml::LoadContext& context, int32_t npcId) {
	dataholders::DataManager::AI_DATA.resetForTests();
	dataholders::DataManager::AI_DATA.publish(xml::bindString<dataholders::AIData>(context,
		R"(<ai_templates><ai npcId=")" + std::to_string(npcId) + R"("><summons></summons></ai></ai_templates>)"));
}

/** records the CREATURE_AGGRO events an npc gets */
struct AggroRecorder : gameserver::ai::NpcAI {
	explicit AggroRecorder(Npc& owner) : NpcAI(owner) {}
	int aggros = 0;

protected:
	void handleCreatureAggro(model::gameobjects::Creature& /*creature*/) override { ++aggros; }
};

class I1AiTest : public InstanceHandlerTest {
protected:
	void SetUp() override {
		InstanceHandlerTest::SetUp();
		instance = &create(HARAMEL, HaramelInstance_instanceFactory);
		enter(*instance, 224.0f, 300.0f, 142.0f);
	}

	template <class AI>
	AI& install(Npc& npc) {
		auto ai = std::make_unique<AI>(npc);
		AI& typed = *ai;
		npc.replaceAi(std::move(ai));
		typed.setStateIfNot(gameserver::ai::AIState::IDLE);
		return typed;
	}

	runtime::Ptr<Npc> place(int32_t npcId, float x, float y) { return spawnNpc(*instance, npcId, x, y, 142.0f, int8_t{7}); }

	size_t count(std::initializer_list<int32_t> ids) {
		size_t n = 0;
		for (int32_t id : ids)
			n += npcs(*instance, id).size();
		return n;
	}

	world::WorldMapInstance* instance = nullptr;
};

/** The five AION_AI factories build their classes (the names are the registry scan's check) */
TEST_F(I1AiTest, TheFactoriesBuildTheirClasses) {
	runtime::Ptr<Npc> npc = place(900200, 230.0f, 300.0f);
	EXPECT_NE(dynamic_cast<ais::azoturanFortress::BetrayerIcaronixAI*>(ais::azoturanFortress::BetrayerIcaronixAI_aiFactory(*npc).get()), nullptr);
	EXPECT_NE(dynamic_cast<ais::abyss::FortressInstanceDukeAI*>(ais::abyss::FortressInstanceDukeAI_aiFactory(*npc).get()), nullptr);
	EXPECT_NE(dynamic_cast<ais::abyss::IllusionGateAI*>(ais::abyss::IllusionGateAI_aiFactory(*npc).get()), nullptr);
	EXPECT_NE(dynamic_cast<ais::dredgion::ShulackDrudgeAI*>(ais::dredgion::ShulackDrudgeAI_aiFactory(*npc).get()), nullptr);
	EXPECT_NE(dynamic_cast<ais::dredgion::SurkanaAI*>(ais::dredgion::SurkanaAI_aiFactory(*npc).get()), nullptr);
	EXPECT_NE(dynamic_cast<ais::esoterrace::KexkraPrototypeAI*>(ais::esoterrace::KexkraPrototypeAI_aiFactory(*npc).get()), nullptr);
	EXPECT_NE(dynamic_cast<ais::esoterrace::WardenSuramaAI*>(ais::esoterrace::WardenSuramaAI_aiFactory(*npc).get()), nullptr);
	EXPECT_NE(dynamic_cast<ais::esoterrace::DaliaCharlandsAI*>(ais::esoterrace::DaliaCharlandsAI_aiFactory(*npc).get()), nullptr);
	EXPECT_NE(dynamic_cast<ais::esoterrace::GreenfingersAI*>(ais::esoterrace::GreenfingersAI_aiFactory(*npc).get()), nullptr);
	EXPECT_NE(dynamic_cast<ais::esoterrace::CaptainMuruganAI*>(ais::esoterrace::CaptainMuruganAI_aiFactory(*npc).get()), nullptr);
	EXPECT_NE(dynamic_cast<ais::theHexway::AdjutantGalamatAI*>(ais::theHexway::AdjutantGalamatAI_aiFactory(*npc).get()), nullptr);
	EXPECT_NE(dynamic_cast<ais::theHexway::CaptainJarkaAI*>(ais::theHexway::CaptainJarkaAI_aiFactory(*npc).get()), nullptr);
	EXPECT_NE(dynamic_cast<ais::theHexway::CaptainLakharaAI*>(ais::theHexway::CaptainLakharaAI_aiFactory(*npc).get()), nullptr);
	EXPECT_NE(dynamic_cast<ais::theHexway::ChestTeleporterAI*>(ais::theHexway::ChestTeleporterAI_aiFactory(*npc).get()), nullptr);
}

/** BetrayerIcaronixAI.java:24-34: above half HP an attack changes nothing; at half, Icaronix the Betrayer takes the place with 50 % HP */
TEST_F(I1AiTest, IcaronixLeavesHisPlaceToTheBetrayerAtHalfHp) {
	runtime::Ptr<Npc> icaronix = place(900200, 230.0f, 300.0f);
	IcaronixProbe& ai = install<IcaronixProbe>(*icaronix);
	icaronix->getLifeStats()->setCurrentHpPercent(60);
	ai.handleAttack(runtime::Ptr<model::gameobjects::Creature>(player()));
	EXPECT_TRUE(npcs(*instance, 214599).empty());
	EXPECT_TRUE(icaronix->isSpawned());
	icaronix->getLifeStats()->setCurrentHpPercent(50);
	ai.handleAttack(runtime::Ptr<model::gameobjects::Creature>(player()));
	ASSERT_EQ(npcs(*instance, 214599).size(), 1u);
	runtime::Ptr<Npc> betrayer = npcs(*instance, 214599).front();
	EXPECT_FLOAT_EQ(betrayer->getX(), 230.0f);
	EXPECT_FLOAT_EQ(betrayer->getY(), 300.0f);
	EXPECT_EQ(betrayer->getHeading(), icaronix->getHeading()) << "the owner's heading (the attack turned it)";
	EXPECT_EQ(betrayer->getLifeStats()->getHpPercentage(), 50);
	EXPECT_FALSE(icaronix->isSpawned()) << "AIActions.deleteOwner";
}

/** FortressInstanceDukeAI.java:23-45: skill 18003 brings a summon at the duke; his death and despawn take the four summon ids, no other */
TEST_F(I1AiTest, TheDukesSummonsComeWithSkill18003AndGoWithHim) {
	dataholders::DataManager::SKILL_DATA.resetForTests(); // the base fixture's empty holder; its TearDown resets this one
	xml::LoadContext skillContext;
	std::string skills = "<skill_data>";
	for (int32_t id : {18003, 18004})
		skills += R"(<skill_template skill_id=")" + std::to_string(id) +
			R"(" name="duke" nameId="1" stack="DUKE" lvl="1" skilltype="MAGICAL" skillsubtype="NONE" tslot="SPEC" activation="ACTIVE")"
			R"( cooldown="0" duration="0"><properties first_target="ME" target_relation="FRIEND" target_type="ONLYONE" /></skill_template>)";
	dataholders::DataManager::SKILL_DATA.publish(xml::bindString<dataholders::SkillData>(skillContext, skills + "</skill_data>"));
	runtime::Ptr<Npc> duke = place(900200, 230.0f, 300.0f);
	DukeProbe& ai = install<DukeProbe>(*duke);
	ai.onEndUseSkill(dataholders::DataManager::SKILL_DATA->getSkillTemplate(18004), 1);
	EXPECT_TRUE(npcs(*instance, 284978).empty());
	ai.onEndUseSkill(dataholders::DataManager::SKILL_DATA->getSkillTemplate(18003), 1);
	ASSERT_EQ(npcs(*instance, 284978).size(), 1u);
	EXPECT_FLOAT_EQ(npcs(*instance, 284978).front()->getX(), 230.0f);
	for (int32_t id : {284979, 284980, 284981})
		place(id, 240.0f, 300.0f);
	runtime::Ptr<Npc> other = place(900300, 240.0f, 300.0f);
	ai.handleDied();
	EXPECT_EQ(count({284978, 284979, 284980, 284981}), 0u);
	EXPECT_TRUE(other->isSpawned());
	place(284980, 240.0f, 300.0f);
	ai.handleDespawned();
	EXPECT_EQ(count({284980}), 0u);
	EXPECT_TRUE(other->isSpawned());
}

/** IllusionGateAI.java:27-51: from 10 s, every 30 s two of the gate's id + 1..3 next to it; its death ends that */
TEST_F(I1AiTest, TheIllusionGateBringsTwoEvery30Seconds) {
	runtime::Ptr<Npc> gate = place(900100, 230.0f, 300.0f);
	GateProbe& ai = install<GateProbe>(*gate);
	ai.handleSpawned();
	executor().advance(std::chrono::milliseconds(9999));
	EXPECT_EQ(count({900101, 900102, 900103}), 0u);
	executor().advance(std::chrono::milliseconds(1));
	EXPECT_EQ(count({900101, 900102, 900103}), 2u);
	for (runtime::Ptr<Npc>& npc : npcs(*instance)) {
		if (npc->getNpcId() > 900100 && npc->getNpcId() <= 900103)
			EXPECT_LE(std::hypot(npc->getX() - 230.0f, npc->getY() - 300.0f), 1.01f) << "within 0..1 m";
	}
	executor().advance(std::chrono::milliseconds(30000));
	EXPECT_EQ(count({900101, 900102, 900103}), 4u);
	ai.handleDied();
	executor().advance(std::chrono::milliseconds(60000));
	EXPECT_EQ(count({900101, 900102, 900103}), 4u) << "the task is cancelled";
	EXPECT_EQ(count({900100}), 1u) << "the gate itself is not one of them";
}

/** ShulackDrudgeAI.java:32-34: its dialog is page 1011 (handleDialogFinish's supplies need the item rows: left to a gate) */
TEST_F(I1AiTest, TheDrudgesDialogIsPage1011) {
	runtime::Ptr<Npc> drudge = place(900300, 230.0f, 300.0f);
	DrudgeProbe& ai = install<DrudgeProbe>(*drudge);
	ai.handleDialogStart(player());
	EXPECT_EQ(countSent(serialized(network::aion::serverpackets::SM_DIALOG_WINDOW(drudge->getObjectId(), 1011))), 1);
}

/** SurkanaAI.java:23-34: an attack calls the live npcs it knows within 25 m to aggro */
TEST_F(I1AiTest, ASurkanaCallsTheRoomWithin25Meters) {
	runtime::Ptr<Npc> surkana = place(900200, 230.0f, 300.0f);
	SurkanaProbe& ai = install<SurkanaProbe>(*surkana);
	runtime::Ptr<Npc> near = place(900300, 250.0f, 300.0f); // 20 m
	runtime::Ptr<Npc> far = place(900300, 260.0f, 300.0f);  // 30 m
	AggroRecorder& nearAi = install<AggroRecorder>(*near);
	AggroRecorder& farAi = install<AggroRecorder>(*far);
	InstanceKnownListPairing::pair(*surkana, *near);
	InstanceKnownListPairing::pair(*surkana, *far);
	ai.handleAttack(runtime::Ptr<model::gameobjects::Creature>(player()));
	EXPECT_EQ(nearAi.aggros, 1);
	EXPECT_EQ(farAi.aggros, 0);
}

/** KexkraPrototypeAI.java:26-40: at 75 % HP the living players it knows see movie 472, Kexkra (217206) comes and the prototype goes */
TEST_F(I1AiTest, TheKexkraPrototypeGivesWayToKexkraAt75Percent) {
	leave(); // Esoterrace's map: Kexkra's place (1320, 1171) is outside Haramel's 512 m
	instance = &create(300250000, abyss::EsoterraceInstance_instanceFactory);
	enter(*instance, 1310.0f, 1170.0f, 52.0f);
	runtime::Ptr<Npc> prototype = spawnNpc(*instance, 900200, 1312.0f, 1170.0f, 52.0f, int8_t{0});
	KexkraProbe& ai = install<KexkraProbe>(*prototype);
	InstanceKnownListPairing::pair(*prototype, player());
	prototype->getLifeStats()->setCurrentHpPercent(80);
	ai.handleAttack(runtime::Ptr<model::gameobjects::Creature>(player()));
	EXPECT_TRUE(npcs(*instance, 217206).empty());
	prototype->getLifeStats()->setCurrentHpPercent(75);
	ai.handleAttack(runtime::Ptr<model::gameobjects::Creature>(player()));
	EXPECT_EQ(countSent(serialized(network::aion::serverpackets::SM_PLAY_MOVIE(false, 0, 0, 472, true))), 1);
	ASSERT_EQ(npcs(*instance, 217206).size(), 1u);
	EXPECT_FLOAT_EQ(npcs(*instance, 217206).front()->getX(), 1320.639282f);
	EXPECT_FALSE(prototype->isSpawned());
}

/** WardenSuramaAI.java:27-61: at 50, 25 and 5 % the eight geysers (static ids) rise for 13 s with two messages; back home the phases restart */
TEST_F(I1AiTest, WardenSuramasGeysersRiseAtHerPhases) {
	dataholders::DataManager::SKILL_DATA.resetForTests(); // the base fixture's empty holder; its TearDown resets this one
	xml::LoadContext skillContext;
	dataholders::DataManager::SKILL_DATA.publish(xml::bindString<dataholders::SkillData>(skillContext,
		R"(<skill_data><skill_template skill_id="19332" name="geysers" nameId="1" stack="GEYSER" lvl="1" skilltype="MAGICAL" skillsubtype="NONE")"
		R"( tslot="SPEC" activation="ACTIVE" cooldown="0" duration="0"><properties first_target="ME" target_relation="FRIEND")"
		R"( target_type="ONLYONE" /></skill_template></skill_data>)"));
	leave(); // Esoterrace's map: the geysers stand at about (1317, 1170)
	instance = &create(300250000, abyss::EsoterraceInstance_instanceFactory);
	enter(*instance, 1310.0f, 1170.0f, 52.0f);
	runtime::Ptr<Npc> surama = spawnNpc(*instance, 900200, 1312.0f, 1170.0f, 52.0f, int8_t{0});
	WardenProbe& ai = install<WardenProbe>(*surama);
	InstanceKnownListPairing::pair(*surama, player());
	const std::initializer_list<int32_t> geysers{282171, 282172, 282173, 282174, 282425, 282426, 282427, 282428};
	surama->getLifeStats()->setCurrentHpPercent(60);
	ai.handleAttack(runtime::Ptr<model::gameobjects::Creature>(player()));
	EXPECT_EQ(count(geysers), 0u);
	surama->getLifeStats()->setCurrentHpPercent(50);
	ai.handleAttack(runtime::Ptr<model::gameobjects::Creature>(player()));
	ASSERT_EQ(count(geysers), 8u);
	EXPECT_EQ(npcs(*instance, 282173).front()->getSpawn()->getStaticId(), 598);
	EXPECT_EQ(countSent(serialized(network::aion::serverpackets::SM_SYSTEM_MESSAGE::STR_MSG_IDF4Re_Drana_10())), 1);
	EXPECT_EQ(countSent(serialized(network::aion::serverpackets::SM_SYSTEM_MESSAGE::STR_MSG_IDF4Re_Drana_09())), 1);
	executor().advance(std::chrono::milliseconds(12999));
	EXPECT_EQ(count(geysers), 8u);
	executor().advance(std::chrono::milliseconds(1));
	EXPECT_EQ(count(geysers), 0u) << "13 s";
	ai.handleAttack(runtime::Ptr<model::gameobjects::Creature>(player()));
	EXPECT_EQ(count(geysers), 0u) << "the 50 % phase once";
	ai.handleBackHome();
	surama->getLifeStats()->setCurrentHpPercent(50); // the way home heals her
	ai.handleAttack(runtime::Ptr<model::gameobjects::Creature>(player()));
	EXPECT_EQ(count(geysers), 8u) << "back home resets the phases";
}

/** DaliaCharlandsAI.java:29-66: at 75, 50 and 25 % three greenfingers with their walker routes; healed above 80 % or back home, the phases
 * start again */
TEST_F(I1AiTest, DaliaCharlandsSendsHerGreenfingers) {
	leave(); // Esoterrace's map: the greenfingers start at about (1174, 674)
	instance = &create(300250000, abyss::EsoterraceInstance_instanceFactory);
	enter(*instance, 1170.0f, 670.0f, 297.5f);
	runtime::Ptr<Npc> dalia = spawnNpc(*instance, 900200, 1172.0f, 672.0f, 297.5f, int8_t{0});
	DaliaProbe& ai = install<DaliaProbe>(*dalia);
	InstanceKnownListPairing::pair(*dalia, player());
	dalia->getAggroList().addDamage(player(), 1, true, std::nullopt); // a target: without one each attack sends her home, resetting the phases
	const std::initializer_list<int32_t> helpers{282176, 282177, 282178};
	dalia->getLifeStats()->setCurrentHpPercent(80);
	ai.handleAttack(runtime::Ptr<model::gameobjects::Creature>(player()));
	EXPECT_EQ(count(helpers), 0u);
	dalia->getLifeStats()->setCurrentHpPercent(75);
	ai.handleAttack(runtime::Ptr<model::gameobjects::Creature>(player()));
	ASSERT_EQ(count(helpers), 3u);
	EXPECT_EQ(npcs(*instance, 282177).front()->getSpawn()->getWalkerId(), std::optional<std::string>("3002500001"));
	EXPECT_EQ(npcs(*instance, 282176).front()->getSpawn()->getWalkerId(), std::optional<std::string>("3002500002"));
	EXPECT_EQ(npcs(*instance, 282178).front()->getSpawn()->getWalkerId(), std::optional<std::string>("3002500003"));
	EXPECT_FLOAT_EQ(npcs(*instance, 282178).front()->getX(), 1176.2f);
	dalia->getLifeStats()->setCurrentHpPercent(50);
	ai.handleAttack(runtime::Ptr<model::gameobjects::Creature>(player()));
	EXPECT_EQ(count(helpers), 6u);
	EXPECT_EQ(ai.backHomes, 0) << "she fights: no way home so far";
	dalia->getLifeStats()->setCurrentHpPercent(85); // healed: the next attack resets the phases
	ai.handleAttack(runtime::Ptr<model::gameobjects::Creature>(player()));
	dalia->getLifeStats()->setCurrentHpPercent(75);
	ai.handleAttack(runtime::Ptr<model::gameobjects::Creature>(player()));
	EXPECT_EQ(count(helpers), 9u) << "the 75 % phase again";
	ai.handleBackHome();
	dalia->getLifeStats()->setCurrentHpPercent(75);
	ai.handleAttack(runtime::Ptr<model::gameobjects::Creature>(player()));
	EXPECT_EQ(count(helpers), 12u) << "back home resets the phases";
}

/** GreenfingersAI.java:30-32, 78-84: it does not think; immune to abnormal states, no decay, respawn or reward (its walk's step arm needs a
 * walker route: left to a gate) */
TEST_F(I1AiTest, AGreenfingerDoesNotThinkAndGivesNothing) {
	runtime::Ptr<Npc> greenfinger = place(900200, 230.0f, 300.0f);
	ais::esoterrace::GreenfingersAI& ai = install<ais::esoterrace::GreenfingersAI>(*greenfinger);
	using gameserver::ai::poll::AIQuestion;
	EXPECT_FALSE(ai.canThink());
	EXPECT_TRUE(ai.ask(AIQuestion::IS_IMMUNE_TO_ABNORMAL_STATES));
	EXPECT_FALSE(ai.ask(AIQuestion::ALLOW_DECAY));
	EXPECT_FALSE(ai.ask(AIQuestion::ALLOW_RESPAWN));
	EXPECT_FALSE(ai.ask(AIQuestion::REWARD_AP_XP_DP_LOOT));
	auto plain = std::make_unique<gameserver::handlers::ai::AggressiveNpcAI>(*greenfinger);
	EXPECT_EQ(ai.ask(AIQuestion::CAN_SHOUT), plain->ask(AIQuestion::CAN_SHOUT)) << "the rest as AggressiveNpcAI";
}

/** CaptainMuruganAI.java:32-126: every 20 s after the first attack a shout and 19325; below half HP the second shout 10 s later; his death
 * ends the tasks with his last shout */
TEST_F(I1AiTest, CaptainMuruganShoutsEveryTwentySeconds) {
	dataholders::DataManager::SKILL_DATA.resetForTests(); // the base fixture's empty holder; its TearDown resets this one
	xml::LoadContext skillContext;
	std::string skills = "<skill_data>";
	for (int32_t id : {19324, 19325})
		skills += R"(<skill_template skill_id=")" + std::to_string(id) +
			R"(" name="murugan" nameId="1" stack="MURUGAN" lvl="1" skilltype="MAGICAL" skillsubtype="NONE" tslot="SPEC" activation="ACTIVE")"
			R"( cooldown="0" duration="0"><properties first_target="ME" target_relation="FRIEND" target_type="ONLYONE" /></skill_template>)";
	dataholders::DataManager::SKILL_DATA.publish(xml::bindString<dataholders::SkillData>(skillContext, skills + "</skill_data>"));
	runtime::Ptr<Npc> murugan = place(900200, 230.0f, 300.0f);
	MuruganProbe& ai = install<MuruganProbe>(*murugan);
	InstanceKnownListPairing::pair(*murugan, player());
	murugan->getAggroList().addDamage(player(), 1, true, std::nullopt); // a target: without one each attack sends him home
	ai.handleAttack(runtime::Ptr<model::gameobjects::Creature>(player()));
	executor().advance(std::chrono::milliseconds(19999));
	(*client)->clearSent(); // the first attack's cast (19324) and its effect: no shout before the 20 s
	executor().advance(std::chrono::milliseconds(1));
	size_t afterShout = sent().size();
	EXPECT_GT(afterShout, 0u) << "the shout 1500194";
	ai.handleAttack(runtime::Ptr<model::gameobjects::Creature>(player())); // a second attack starts nothing more
	murugan->getLifeStats()->setCurrentHpPercent(40);
	(*client)->clearSent();
	executor().advance(std::chrono::milliseconds(20000)); // the next round: the shout, and the special task in 10 s
	size_t round = sent().size();
	EXPECT_GT(round, 0u);
	(*client)->clearSent();
	executor().advance(std::chrono::milliseconds(10000));
	EXPECT_GT(sent().size(), 0u) << "the second shout 1500193, 10 s later";
	ai.handleDied();
	(*client)->clearSent();
	executor().advance(std::chrono::milliseconds(60000));
	EXPECT_TRUE(sent().empty()) << "the tasks ended";
}

/** CaptainJarkaAI.java:21-33: his death leaves a smoke (gone at once) and takes the npcs he knows within 15 m */
TEST_F(I1AiTest, CaptainJarkasDeathClearsHisSurroundings) {
	runtime::Ptr<Npc> jarka = place(900200, 230.0f, 300.0f);
	JarkaProbe& ai = install<JarkaProbe>(*jarka);
	xml::LoadContext aiContext;
	publishEmptySummons(aiContext, 900200);
	ai.handleSpawned();
	runtime::Ptr<Npc> near = place(900300, 240.0f, 300.0f); // 10 m
	runtime::Ptr<Npc> far = place(900300, 250.0f, 300.0f);  // 20 m
	InstanceKnownListPairing::pair(*jarka, *near);
	InstanceKnownListPairing::pair(*jarka, *far);
	ai.handleDied();
	EXPECT_FALSE(near->isSpawned());
	EXPECT_TRUE(far->isSpawned());
	EXPECT_TRUE(npcs(*instance, 282465).empty()) << "the smoke is deleted at once";
	dataholders::DataManager::AI_DATA.resetForTests();
}

/** ChestTeleporterAI.java:38-51: a living player who moves within 8 m in its sight hears the orb once */
TEST_F(I1AiTest, TheChestTeleporterTellsAboutTheOrbOnce) {
	runtime::Ptr<Npc> teleporter = place(900200, 226.0f, 300.0f);
	ChestTeleporterProbe& ai = install<ChestTeleporterProbe>(*teleporter);
	const std::vector<uint8_t> orb = serialized(network::aion::serverpackets::SM_MESSAGE(*teleporter,
		"A mysterious orb of relocation is hidden in this chest", model::ChatType::NPC));
	runtime::Ptr<Npc> other = place(900300, 225.0f, 300.0f);
	ai.handleCreatureMoved(*other); // not a player
	ai.handleCreatureMoved(player());
	ai.handleCreatureMoved(player());
	EXPECT_EQ(countSent(orb), 1);
}

/** AdjutantGalamatAI.java:51-130: skill 21799 starts the shield phase - an add every 4 s from the start, the damage taken counted - and 25 s
 * later the evaluation raises her damage multiplicator (1.2, +0.35 for the player the damage would have killed) */
TEST_F(I1AiTest, AdjutantGalamatsShieldPhase) {
	dataholders::DataManager::SKILL_DATA.resetForTests(); // the base fixture's empty holder; its TearDown resets this one
	xml::LoadContext skillContext;
	dataholders::DataManager::SKILL_DATA.publish(xml::bindString<dataholders::SkillData>(skillContext,
		R"(<skill_data><skill_template skill_id="21799" name="shield" nameId="1" stack="SHIELD" lvl="1" skilltype="MAGICAL")"
		R"( skillsubtype="NONE" tslot="SPEC" activation="ACTIVE" cooldown="0" duration="0"><properties first_target="ME")"
		R"( target_relation="FRIEND" target_type="ONLYONE" /></skill_template></skill_data>)"));
	runtime::Ptr<Npc> galamat = place(900200, 230.0f, 300.0f);
	GalamatProbe& ai = install<GalamatProbe>(*galamat);
	xml::LoadContext aiContext;
	publishEmptySummons(aiContext, 900200);
	ai.handleSpawned();
	InstanceKnownListPairing::pair(*galamat, player());
	EXPECT_FLOAT_EQ(ai.modifyOwnerDamage(100.0f, player(), nullptr), 100.0f);
	ai.onEndUseSkill(dataholders::DataManager::SKILL_DATA->getSkillTemplate(21799), 1);
	executor().advance(std::chrono::milliseconds(0));
	EXPECT_EQ(count({219616}), 1u) << "an add at once";
	executor().advance(std::chrono::milliseconds(4000));
	EXPECT_EQ(count({219616}), 2u);
	// the shield effect is not on her (no effect controller entry): the first counted hit ends the phase (Java: findBySkillId == null)
	EXPECT_FLOAT_EQ(ai.modifyDamage(player(), 50.0f, nullptr), 50.0f);
	executor().advance(std::chrono::milliseconds(30000));
	EXPECT_EQ(count({219616}), 2u) << "the phase ended: no more adds";
	EXPECT_FLOAT_EQ(ai.modifyOwnerDamage(100.0f, player(), nullptr), 100.0f) << "no evaluation after the reset";
	ai.onEndUseSkill(dataholders::DataManager::SKILL_DATA->getSkillTemplate(21799), 1);
	executor().advance(std::chrono::milliseconds(25000));
	EXPECT_FLOAT_EQ(ai.modifyOwnerDamage(100.0f, player(), nullptr), 120.0f) << "1.2 after an evaluation without damage";
	ai.handleDied();
	dataholders::DataManager::AI_DATA.resetForTests();
}

} // namespace
} // namespace aion::gameserver::handlers::instance::test
