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
#include "aion/gameserver/dataholders/SkillData.bind.h"
#include "aion/gameserver/dataholders/SkillData.h"
#include "aion/gameserver/handlers/ai/instance/abyss/FortressInstanceDukeAI.h"
#include "aion/gameserver/handlers/ai/instance/abyss/IllusionGateAI.h"
#include "aion/gameserver/handlers/ai/instance/azoturanFortress/BetrayerIcaronixAI.h"
#include "aion/gameserver/handlers/ai/instance/dredgion/ShulackDrudgeAI.h"
#include "aion/gameserver/handlers/ai/instance/dredgion/SurkanaAI.h"
#include "aion/gameserver/handlers/ai/instance/esoterrace/KexkraPrototypeAI.h"
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
}
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

} // namespace
} // namespace aion::gameserver::handlers::instance::test
