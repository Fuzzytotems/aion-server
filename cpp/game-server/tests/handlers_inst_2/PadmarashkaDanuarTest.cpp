// P6-I2 Padmarashka's Cave and the Danuar Sanctuaries (game-server/data/handlers/instance/PadmarashkasCaveInstance.java,
// DanuarSanctuaryInstance.java, SeizedDanuarSanctuaryInstance.java) on real instances of their maps. Left out: the nest's movie (no zones
// in the test world), the teleports and the revive (they move through the teleport service), and the effect 20101's content (no effect data).

#include "I2TestSupport.h"
#include "CrucibleTestSupport.h"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <functional>
#include <memory>
#include <regex>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "aion/commons/utils/Rnd.h"
#include "aion/gameserver/controllers/effect/EffectController.h"
#include "aion/gameserver/dataholders/SkillData.bind.h"
#include "aion/gameserver/dataholders/SkillData.h"
#include "aion/gameserver/handlers/ai/GeneralNpcAI.h"
#include "aion/gameserver/handlers/instance/DanuarSanctuaryInstance.h"
#include "aion/gameserver/handlers/instance/PadmarashkasCaveInstance.h"
#include "aion/gameserver/handlers/instance/SeizedDanuarSanctuaryInstance.h"
#include "aion/gameserver/model/stats/container/NpcLifeStats.h"
#include "aion/gameserver/model/templates/spawns/SpawnTemplate.h"
#include "aion/gameserver/skillengine/model/SkillTemplate.h"

namespace aion::gameserver::handlers::instance::test {
namespace {

using model::gameobjects::Npc;

/** an AI recording the skills its npc starts and the creature events it gets */
struct RecordingAI : ::aion::gameserver::handlers::ai::GeneralNpcAI {
	using GeneralNpcAI::GeneralNpcAI;
	std::vector<int32_t> casts;
	void onStartUseSkill(const skillengine::model::SkillTemplate* skillTemplate, int32_t skillLevel) override {
		casts.push_back(skillTemplate->getSkillId());
		GeneralNpcAI::onStartUseSkill(skillTemplate, skillLevel);
	}
	void handleCreatureAggro(model::gameobjects::Creature& creature) override { aggros++; }
	int32_t aggros = 0;
};

class PadmarashkaDanuarTestBase : public InstanceHandlerTest {
protected:
	void open(int32_t mapId, ::aion::gameserver::handlers::InstanceFactory& factory, model::Race race = model::Race::ELYOS) {
		publishCrucibleData(context);
		instance = &create(mapId, factory);
		enterAs(race);
		handler().onEnterInstance(player());
		(*client)->clearSent();
	}

	void enterAs(model::Race race) {
		if (race == model::Race::ELYOS) {
			enter(*instance, 1000.0f, 700.0f, 282.0f);
			return;
		}
		actor = cptest::makePlayer(430002, 9432, "Shade", race);
		world::World& world = world::World::getInstance();
		world.storeObject(*actor.player);
		ASSERT_TRUE(world.setPosition(runtime::Ptr<model::gameobjects::VisibleObject>(*actor.player), instance->getMapId(), instance->getInstanceId(),
			1000.0f, 700.0f, 282.0f, int8_t{0}));
		world.spawn(runtime::Ptr<model::gameobjects::VisibleObject>(*actor.player));
		client = std::make_unique<cptest::TestClient>();
		client->enterWorld(actor);
	}

	void TearDown() override {
		InstanceHandlerTest::TearDown();
		resetCrucibleData();
	}

	/** one plain template per id; `lasting` ids get a minute's stat buff, so an applied effect stays on its effected (EffectController) */
	void publishSkills(std::initializer_list<int32_t> ids, std::initializer_list<int32_t> lasting = {}) {
		dataholders::DataManager::SKILL_DATA.resetForTests();
		std::string skills = "<skill_data>";
		for (int32_t id : ids) {
			const bool buff = std::find(lasting.begin(), lasting.end(), id) != lasting.end();
			skills += R"(<skill_template skill_id=")" + std::to_string(id) +
				R"(" name="cave" nameId="1" stack="CAVE)" + std::to_string(id) + R"(" lvl="1" skilltype="MAGICAL" skillsubtype="NONE" tslot="SPEC")"
				R"( activation="ACTIVE" cooldown="0" duration="0"><properties first_target="ME" target_relation="ALL" target_type="ONLYONE" />)" +
				std::string(buff ? R"(<effects><statup duration2="60000" e="1" noresist="true"><change stat="PHYSICAL_DEFENSE" func="ADD" value="1" />)"
				                   R"(</statup></effects>)"
				                 : "") +
				"</skill_template>";
		}
		dataholders::DataManager::SKILL_DATA.publish(xml::bindString<dataholders::SkillData>(skillContext, skills + "</skill_data>"));
	}

	::aion::gameserver::instance::handlers::InstanceHandler& handler() { return *instance->getInstanceHandler(); }

	size_t count(int32_t npcId) { return npcs(*instance, npcId).size(); }

	runtime::Ptr<Npc> place(int32_t npcId) { return spawnNpc(*instance, npcId, 1000.0f, 701.0f, 282.0f, int8_t{0}); }

	void kill(int32_t npcId, int32_t times = 1) {
		for (int32_t i = 0; i < times; i++)
			place(npcId)->getLifeStats()->setCurrentHp(0);
	}

	RecordingAI& install(Npc& npc) {
		auto ai = std::make_unique<RecordingAI>(npc);
		RecordingAI& typed = *ai;
		npc.replaceAi(std::move(ai));
		typed.setStateIfNot(gameserver::ai::AIState::IDLE);
		return typed;
	}

	void after(int64_t millis) { executor().advance(std::chrono::milliseconds(millis)); }

	static void seedFor(const std::function<bool()>& draws) {
		for (uint64_t seed = 1; seed < 10000; seed++) {
			commons::utils::Rnd::seedCurrentThreadForTests(seed);
			if (draws()) {
				commons::utils::Rnd::seedCurrentThreadForTests(seed);
				return;
			}
		}
		FAIL() << "no seed";
	}

	xml::LoadContext skillContext;
	world::WorldMapInstance* instance = nullptr;
};

class PadmarashkasCaveTest : public PadmarashkaDanuarTestBase {
protected:
	void SetUp() override {
		InstanceHandlerTest::SetUp();
		publishSkills({19186, 19187, 20101}, {20101}); // 19186: removeEffect looks its template up
		open(PADMARASHKAS_CAVE, PadmarashkasCaveInstance_instanceFactory);
		padmarashka = place(218756);
		InstanceKnownListPairing::pair(*padmarashka, player());
		padmaAi = &install(*padmarashka);
		padmarashka->getEffectController()->setAbnormal(skillengine::effect::AbnormalState::SLEEP);
	}

	runtime::Ptr<Npc> padmarashka;
	RecordingAI* padmaAi = nullptr;
};

class DanuarSanctuaryTest : public PadmarashkaDanuarTestBase {
protected:
	void SetUp() override {
		InstanceHandlerTest::SetUp();
		open(DANUAR_SANCTUARY, DanuarSanctuaryInstance_instanceFactory);
	}
};

class SeizedDanuarSanctuaryTest : public PadmarashkaDanuarTestBase {
protected:
	void SetUp() override { InstanceHandlerTest::SetUp(); }
};

int32_t javaInstanceId(const char* file) {
	std::ifstream in(std::filesystem::path(AION_GAMESERVER_JAVA_DIR) / file);
	std::stringstream text;
	text << in.rdbuf();
	const std::string source = text.str();
	std::smatch match;
	return std::regex_search(source, match, std::regex(R"(@InstanceID\((\d+)\))")) ? std::stoi(match[1].str()) : 0;
}

TEST_F(PadmarashkasCaveTest, TheHandlerIsTheJavaClassOfItsMap) {
	EXPECT_EQ(javaInstanceId("data/handlers/instance/PadmarashkasCaveInstance.java"), PADMARASHKAS_CAVE);
	EXPECT_NE(dynamic_cast<PadmarashkasCaveInstance*>(&handler()), nullptr);
}

TEST_F(DanuarSanctuaryTest, TheHandlerIsTheJavaClassOfItsMap) {
	EXPECT_EQ(javaInstanceId("data/handlers/instance/DanuarSanctuaryInstance.java"), DANUAR_SANCTUARY);
	EXPECT_NE(dynamic_cast<DanuarSanctuaryInstance*>(&handler()), nullptr);
}

TEST_F(SeizedDanuarSanctuaryTest, TheHandlerIsTheJavaClassOfItsMap) {
	open(SEIZED_DANUAR_SANCTUARY, SeizedDanuarSanctuaryInstance_instanceFactory);
	EXPECT_EQ(javaInstanceId("data/handlers/instance/SeizedDanuarSanctuaryInstance.java"), SEIZED_DANUAR_SANCTUARY);
	EXPECT_NE(dynamic_cast<SeizedDanuarSanctuaryInstance*>(&handler()), nullptr);
}

/** PadmarashkasCaveInstance.java: the fourth protector wakes Padmarashka: no sleep, her skill, and her aggro a second later */
TEST_F(PadmarashkasCaveTest, TheProtectorsWakeHer) {
	kill(218670);
	kill(218671);
	kill(218673);
	EXPECT_TRUE(padmarashka->getEffectController()->isAbnormalSet(skillengine::effect::AbnormalState::SLEEP));
	EXPECT_TRUE(padmaAi->casts.empty());
	kill(218674);
	EXPECT_FALSE(padmarashka->getEffectController()->isAbnormalSet(skillengine::effect::AbnormalState::SLEEP));
	EXPECT_EQ(padmaAi->casts, (std::vector<int32_t>{19187}));
	after(999);
	EXPECT_EQ(padmaAi->aggros, 0);
	after(1);
	EXPECT_EQ(padmaAi->aggros, 1) << "CREATURE_AGGRO with the first player inside";
	kill(218670, 3);
	EXPECT_EQ(padmaAi->casts.size(), 1u) << "the count restarts at 0";
	kill(218671);
	EXPECT_EQ(padmaAi->casts.size(), 2u);
}

/** :30-37: a dead Padmarashka is not woken */
TEST_F(PadmarashkasCaveTest, ADeadPadmarashkaSleeps) {
	padmarashka->getLifeStats()->setCurrentHp(0);
	kill(218670, 4);
	EXPECT_TRUE(padmaAi->casts.empty());
}

/** :41-48: the twentieth egg enrages her */
TEST_F(PadmarashkasCaveTest, TwentyEggs) {
	kill(282613, 10);
	kill(282614, 9);
	EXPECT_FALSE(padmarashka->getEffectController()->hasAbnormalEffect(20101));
	kill(282613);
	EXPECT_TRUE(padmarashka->getEffectController()->hasAbnormalEffect(20101));
}

/** :58-62: the destruction resets the protectors' count */
TEST_F(PadmarashkasCaveTest, TheDestructionResetsTheCount) {
	kill(218670, 3);
	handler().onInstanceDestroy();
	kill(218670);
	EXPECT_TRUE(padmaAi->casts.empty());
}

/** DanuarSanctuaryInstance.java: one of the three bosses at the creation; its death opens the exit */
TEST_F(DanuarSanctuaryTest, TheBossAndTheExit) {
	EXPECT_EQ(count(235624) + count(235625) + count(235626), 1u);
	kill(235625);
	EXPECT_EQ(count(701876), 1u);
	kill(235619);
	EXPECT_EQ(count(701876), 1u) << "the seized sanctuary's boss";
}

/** :28-34: 233187 leaves 233087 (static id 1699); the barricade and two npcs only go */
TEST_F(DanuarSanctuaryTest, TheDeaths) {
	runtime::Ptr<Npc> npc = place(233187);
	handler().onDie(*npc);
	EXPECT_FALSE(npc->isSpawned()) << "the handler deletes it";
	ASSERT_EQ(count(233087), 1u);
	EXPECT_EQ(npcs(*instance, 233087)[0]->getSpawn()->getStaticId(), 1699);
	for (int32_t id : {730866, 233448, 233447}) {
		runtime::Ptr<Npc> other = place(id);
		handler().onDie(*other);
		EXPECT_FALSE(other->isSpawned()) << id;
	}
	runtime::Ptr<Npc> stays = place(233085);
	handler().onDie(*stays);
	EXPECT_TRUE(stays->isSpawned());
}

/** :54-61: the cannon fires once at the barricade, which dies 5.7 s later */
TEST_F(DanuarSanctuaryTest, TheCannon) {
	publishSkills({20385});
	runtime::Ptr<Npc> barricade = place(730866);
	runtime::Ptr<Npc> cannon = place(730863);
	RecordingAI& ai = install(*cannon);
	handler().handleUseItemFinish(runtime::Ptr<model::gameobjects::player::Player>(player()), *cannon);
	EXPECT_EQ(ai.casts, (std::vector<int32_t>{20385}));
	after(5699);
	EXPECT_FALSE(barricade->isDead());
	after(1);
	EXPECT_TRUE(barricade->isDead());
	handler().handleUseItemFinish(runtime::Ptr<model::gameobjects::player::Player>(player()), *cannon);
	EXPECT_EQ(ai.casts.size(), 1u) << "once";
}

/** :70-77: the boss by Rnd */
TEST_F(DanuarSanctuaryTest, TheBossByRnd) {
	seedFor([] { return commons::utils::Rnd::get(0, 2) == 2; });
	runtime::Ref<world::WorldMapInstance> second = runtime::Ref<world::WorldMapInstance>(create(DANUAR_SANCTUARY, DanuarSanctuaryInstance_instanceFactory));
	EXPECT_EQ(npcs(*second, 235626).size(), 1u);
}

/** SeizedDanuarSanctuaryInstance.java: an Elyos first player gets the Elyos quest npcs, once */
TEST_F(SeizedDanuarSanctuaryTest, TheElyosQuestNpcs) {
	open(SEIZED_DANUAR_SANCTUARY, SeizedDanuarSanctuaryInstance_instanceFactory);
	EXPECT_EQ(count(233129), 4u);
	EXPECT_EQ(count(233130), 2u);
	EXPECT_EQ(count(233131), 2u);
	EXPECT_EQ(count(233126), 0u);
	handler().onEnterInstance(player());
	EXPECT_EQ(count(233129), 4u) << "once";
	EXPECT_EQ(count(235619) + count(235620) + count(235621), 1u);
	kill(235620);
	EXPECT_EQ(count(701876), 1u);
}

/** :115-124: an Asmodian first player gets the Asmodian ones */
TEST_F(SeizedDanuarSanctuaryTest, TheAsmodianQuestNpcs) {
	open(SEIZED_DANUAR_SANCTUARY, SeizedDanuarSanctuaryInstance_instanceFactory, model::Race::ASMODIANS);
	EXPECT_EQ(count(233126), 4u);
	EXPECT_EQ(count(233127), 2u);
	EXPECT_EQ(count(233128), 2u);
	EXPECT_EQ(count(233129), 0u);
}

/** :43-55: the seized sanctuary's own deaths and cannon */
TEST_F(SeizedDanuarSanctuaryTest, TheDeathsAndTheCannon) {
	publishSkills({20385});
	open(SEIZED_DANUAR_SANCTUARY, SeizedDanuarSanctuaryInstance_instanceFactory);
	kill(235624);
	EXPECT_EQ(count(701876), 0u) << "the other sanctuary's boss";
	runtime::Ptr<Npc> npc = place(233187);
	npc->getLifeStats()->setCurrentHp(0);
	EXPECT_EQ(count(233087), 1u);
	runtime::Ptr<Npc> drop = place(233448);
	handler().onDie(*drop);
	EXPECT_FALSE(drop->isSpawned());
	runtime::Ptr<Npc> barricade = place(730866);
	runtime::Ptr<Npc> cannon = place(730863);
	RecordingAI& ai = install(*cannon);
	handler().handleUseItemFinish(runtime::Ptr<model::gameobjects::player::Player>(player()), *cannon);
	EXPECT_EQ(ai.casts, (std::vector<int32_t>{20385}));
	after(5700);
	EXPECT_TRUE(barricade->isDead());
}

/** :86-92: the first of the three bosses by Rnd */
TEST_F(SeizedDanuarSanctuaryTest, TheFirstBossByRnd) {
	seedFor([] { return commons::utils::Rnd::get(0, 2) == 0; });
	open(SEIZED_DANUAR_SANCTUARY, SeizedDanuarSanctuaryInstance_instanceFactory);
	EXPECT_EQ(count(235619), 1u);
}

/** :86-92: the boss by Rnd */
TEST_F(SeizedDanuarSanctuaryTest, TheBossByRnd) {
	seedFor([] { return commons::utils::Rnd::get(0, 2) == 1; });
	open(SEIZED_DANUAR_SANCTUARY, SeizedDanuarSanctuaryInstance_instanceFactory);
	EXPECT_EQ(count(235620), 1u);
}

} // namespace
} // namespace aion::gameserver::handlers::instance::test
