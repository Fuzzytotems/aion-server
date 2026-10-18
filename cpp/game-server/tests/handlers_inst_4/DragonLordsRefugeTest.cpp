// P6I-13 cp1 (docs/design/p6-instances-plan.md, I4 split): DragonLordsRefugeInstance and AnguishedDragonLordsRefugeInstance
// (game-server/data/handlers/instance) on real instances of their maps. The teleports (the portals, the exit, the revive), the roof's and the
// npcs' skill casts and the skill effects' own work are left to a gate.

#include "I4TestSupport.h"

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <memory>
#include <regex>
#include <sstream>
#include <string>

#include "aion/gameserver/ai/AIState.h"
#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/controllers/effect/EffectController.h"
#include "aion/gameserver/handlers/ai/GeneralNpcAI.h"
#include "aion/gameserver/handlers/instance/AnguishedDragonLordsRefugeInstance.h"
#include "aion/gameserver/handlers/instance/DragonLordsRefugeInstance.h"
#include "aion/gameserver/model/RaceInfo.h"
#include "aion/gameserver/model/gameobjects/player/PlayerCommonData.h"
#include "aion/gameserver/model/templates/spawns/SpawnTemplate.h"
#include "aion/gameserver/network/aion/serverpackets/SM_QUEST_ACTION.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/skillengine/SkillEngine.h"

namespace aion::gameserver::handlers::instance::test {
namespace {

using model::gameobjects::Npc;
using network::aion::serverpackets::SM_QUEST_ACTION;
using network::aion::serverpackets::SM_SYSTEM_MESSAGE;

class DragonLordsRefugeTest : public InstanceHandlerTest {
protected:
	void SetUp() override { setUpOn(DRAGON_LORDS_REFUGE, DragonLordsRefugeInstance_instanceFactory); }

	void setUpOn(int32_t mapId, ::aion::gameserver::handlers::InstanceFactory& factory) {
		InstanceHandlerTest::SetUp();
		instance = &create(mapId, factory);
		enter(*instance, 300.0f, 300.0f, 300.0f);
		raceId = model::getRaceId(player().getRace());
	}

	runtime::Ptr<Npc> at(int32_t npcId, float x = 300.0f, float y = 300.0f) { return spawnNpc(*instance, npcId, x, y, 300.0f, int8_t{0}); }

	/** An npc with a root AI (the fixture's npcs have a DummyAI; a skill's use asks for an NpcAI) */
	runtime::Ptr<Npc> withAi(int32_t npcId, float x = 300.0f, float y = 300.0f) {
		runtime::Ptr<Npc> npc = at(npcId, x, y);
		auto root = std::make_unique<ai::GeneralNpcAI>(*npc);
		ai::GeneralNpcAI& typed = *root;
		npc->replaceAi(std::move(root));
		typed.setStateIfNot(gameserver::ai::AIState::IDLE);
		return npc;
	}

	runtime::Ptr<Npc> dies(int32_t npcId, float x = 300.0f, float y = 300.0f) {
		runtime::Ptr<Npc> npc = at(npcId, x, y);
		handler().onDie(*npc);
		return npc;
	}

	auto& handler() { return *instance->getInstanceHandler(); }

	size_t count(std::initializer_list<int32_t> ids) {
		size_t n = 0;
		for (int32_t id : ids)
			n += npcs(*instance, id).size();
		return n;
	}

	int64_t sent(SM_SYSTEM_MESSAGE&& msg) { return countSent(serialized(std::move(msg))); }

	static std::string javaSource(const char* javaClass) {
		std::ifstream in(std::filesystem::path(AION_GAMESERVER_JAVA_DIR) / "data/handlers/instance" / (std::string(javaClass) + ".java"));
		std::stringstream text;
		text << in.rdbuf();
		return text.str();
	}

	world::WorldMapInstance* instance = nullptr;
	int32_t raceId = 0;
};

/** The anguished refuge's instance */
class AnguishedRefugeTest : public DragonLordsRefugeTest {
protected:
	void SetUp() override { setUpOn(ANGUISHED_DRAGON_LORDS_REFUGE, AnguishedDragonLordsRefugeInstance_instanceFactory); }
};

TEST_F(DragonLordsRefugeTest, EachHandlerIsTheJavaClassOfItsMap) {
	for (const auto& [javaClass, mapId] : {std::pair{"DragonLordsRefugeInstance", DRAGON_LORDS_REFUGE},
		     std::pair{"AnguishedDragonLordsRefugeInstance", ANGUISHED_DRAGON_LORDS_REFUGE}}) {
		std::string source = javaSource(javaClass);
		std::smatch match;
		ASSERT_TRUE(std::regex_search(source, match, std::regex(R"(@InstanceID\((\d+)\))"))) << javaClass;
		EXPECT_EQ(std::stoi(match[1].str()), mapId) << javaClass;
	}
	EXPECT_NE(dynamic_cast<DragonLordsRefugeInstance*>(instance->getInstanceHandler().get()), nullptr);
	EXPECT_EQ(dynamic_cast<AnguishedDragonLordsRefugeInstance*>(instance->getInstanceHandler().get()), nullptr);
}

TEST_F(AnguishedRefugeTest, TheHandlerIsTheAnguishedClass) {
	EXPECT_NE(dynamic_cast<AnguishedDragonLordsRefugeInstance*>(instance->getInstanceHandler().get()), nullptr);
}

/** DragonLordsRefugeInstance.java:59-66, 111-118: a minute after Tiamat's dragon form the four portals come, 10 s after the first the
 * four spiritualists */
TEST_F(DragonLordsRefugeTest, TheDragonBringsPortalsAndSpiritualists) {
	at(219361);
	executor().advance(std::chrono::milliseconds(59999));
	EXPECT_EQ(count({730673, 730674, 730675, 730676}), 0u);
	executor().advance(std::chrono::milliseconds(1));
	EXPECT_EQ(count({730673, 730674, 730675, 730676}), 4u);
	EXPECT_FLOAT_EQ(npcs(*instance, 730676).front()->getX(), 547.822f);
	executor().advance(std::chrono::milliseconds(9999));
	EXPECT_EQ(count({283163, 283164, 283165, 283166}), 0u);
	executor().advance(std::chrono::milliseconds(1));
	EXPECT_EQ(count({283163, 283164, 283165, 283166}), 4u);
	EXPECT_EQ(npcs(*instance, 283165).front()->getHeading(), 46);
}

/** :67-78, 152-158: the weakened Tiamat starts the 30 minute countdown; at its end the refuge empties but for the portals, and the exit comes */
TEST_F(DragonLordsRefugeTest, TheCountdownEndsTheRefuge) {
	runtime::Ptr<Npc> portal = at(730633);
	runtime::Ptr<Npc> other = at(900200);
	runtime::Ptr<Npc> tiamat = at(219362);
	EXPECT_EQ(countSent(serialized(SM_QUEST_ACTION(0, 1800))), 1);
	executor().advance(std::chrono::milliseconds(2000));
	EXPECT_EQ(sent(SM_SYSTEM_MESSAGE::IDTIAMAT_TIAMAT_COUNTDOWN_START()), 1);
	executor().advance(std::chrono::milliseconds(1800000 - 2001));
	EXPECT_TRUE(other->isSpawned());
	executor().advance(std::chrono::milliseconds(1));
	EXPECT_EQ(sent(SM_SYSTEM_MESSAGE::IDTIAMAT_TIAMAT_COUNTDOWN_OVER()), 1);
	EXPECT_FALSE(other->isSpawned());
	EXPECT_FALSE(tiamat->isSpawned());
	EXPECT_TRUE(portal->isSpawned());
	ASSERT_EQ(count({730630}), 1u);
	EXPECT_EQ(npcs(*instance, 730630).front()->getSpawn()->getStaticId(), 23);
}

/** :179-182: the destroy cancels the countdown */
TEST_F(DragonLordsRefugeTest, TheDestroyCancelsTheCountdown) {
	runtime::Ptr<Npc> other = at(900200);
	at(219362);
	handler().onInstanceDestroy();
	executor().advance(std::chrono::milliseconds(1800000));
	EXPECT_EQ(sent(SM_SYSTEM_MESSAGE::IDTIAMAT_TIAMAT_COUNTDOWN_OVER()), 0);
	EXPECT_TRUE(other->isSpawned());
}

/** :79-104, 119-121: the phases' messages and their delays */
TEST_F(DragonLordsRefugeTest, ThePhasesTellTheirMessages) {
	at(219488);
	at(219492);
	at(219490);
	at(283140);
	at(730695);
	executor().advance(std::chrono::milliseconds(2500));
	EXPECT_EQ(sent(SM_SYSTEM_MESSAGE::STR_IDTIAMAT_TIAMAT_SPAWN_BLACKHOLE()), 1);
	EXPECT_EQ(sent(SM_SYSTEM_MESSAGE::STR_MSG_IDTIAMAT_KALYNDI_SURKANA_SPAWN()), 1);
	EXPECT_EQ(sent(SM_SYSTEM_MESSAGE::STR_MSG_IDTIAMAT_MARCHUTAN_2PHASE_DEADLYATK()), 0);
	executor().advance(std::chrono::milliseconds(500));
	EXPECT_EQ(sent(SM_SYSTEM_MESSAGE::STR_MSG_IDTIAMAT_MARCHUTAN_2PHASE_DEADLYATK()), 1);
	executor().advance(std::chrono::milliseconds(12000));
	EXPECT_EQ(sent(SM_SYSTEM_MESSAGE::STR_MSG_IDTIAMAT_KAISINEL_2PHASE_GROGGY()), 1);
	EXPECT_EQ(sent(SM_SYSTEM_MESSAGE::STR_MSG_IDTIAMAT_TIAMAT_2PHASE_START_LIGHT()), 0);
	executor().advance(std::chrono::milliseconds(5000));
	EXPECT_EQ(sent(SM_SYSTEM_MESSAGE::STR_MSG_IDTIAMAT_TIAMAT_2PHASE_START_LIGHT()), 1);
	EXPECT_EQ(sent(SM_SYSTEM_MESSAGE::STR_MSG_IDTIAMAT_TIAMAT_2PHASE_START_DARK()), 0);
}

/** :162-167: Calindi's death takes Surkana; 6 s later the aetheric field goes */
TEST_F(DragonLordsRefugeTest, CalindisDeathOpensTheField) {
	publishSkills({20919});
	withAi(219360);
	runtime::Ptr<Npc> surkana = at(730695);
	runtime::Ptr<Npc> field = at(730694);
	dies(219359);
	EXPECT_FALSE(surkana->isSpawned());
	executor().advance(std::chrono::milliseconds(5999));
	EXPECT_TRUE(field->isSpawned());
	executor().advance(std::chrono::milliseconds(1));
	EXPECT_FALSE(field->isSpawned());
}

/** :168-174: the dragon's death brings the weakened Tiamat at its place and the debris (static 17) */
TEST_F(DragonLordsRefugeTest, TheDragonsDeathBringsTheWeakenedTiamat) {
	withAi(730699);
	withAi(730700);
	dies(219361);
	EXPECT_EQ(count({219362}), 1u);
	EXPECT_EQ(count({283134}), 1u);
	ASSERT_EQ(count({730704}), 1u);
	EXPECT_EQ(npcs(*instance, 730704).front()->getSpawn()->getStaticId(), 17);
	EXPECT_FLOAT_EQ(npcs(*instance, 219362).front()->getX(), 451.97f);
}

/** :175-186: the weakened Tiamat's death stops the countdown, empties the refuge but for the last phase's lords and the portals, and brings
 * the chest, Kahrun and the race's lord */
TEST_F(DragonLordsRefugeTest, TiamatsDeathEndsTheRefuge) {
	player().getCommonData()->setRace(model::Race::ASMODIANS); // race id 1: Marchutan
	raceId = model::getRaceId(player().getRace());
	handler().onEnterInstance(player());
	runtime::Ptr<Npc> lord = at(219490);
	runtime::Ptr<Npc> other = at(900200);
	runtime::Ptr<Npc> tiamat = at(219362);
	handler().onDie(*tiamat);
	EXPECT_EQ(countSent(serialized(SM_QUEST_ACTION(0, 0))), 1);
	EXPECT_TRUE(lord->isSpawned());
	EXPECT_TRUE(tiamat->isSpawned());
	EXPECT_FALSE(other->isSpawned());
	EXPECT_EQ(count({701542}), 1u);
	EXPECT_EQ(count({800430}), 1u);
	EXPECT_EQ(count({800356}), 1u);
	EXPECT_EQ(count({800464, 800465}), 2u);
	executor().advance(std::chrono::milliseconds(1800000));
	EXPECT_EQ(sent(SM_SYSTEM_MESSAGE::IDTIAMAT_TIAMAT_COUNTDOWN_OVER()), 0) << "cancelled";
}

/** :187-193, 214-226: the fourth incarnation's death closes the phase; 30 s later the race's first lord becomes its second */
TEST_F(DragonLordsRefugeTest, TheFourIncarnationsCloseThePhase) {
	publishSkills({20979, 20980, 20981, 20982, 20984});
	handler().onEnterInstance(player());
	withAi(219361);
	runtime::Ptr<Npc> lord = at(219488 + raceId * 3);
	for (int32_t id = 219365; id <= 219367; ++id)
		dies(id);
	EXPECT_EQ(sent(SM_SYSTEM_MESSAGE::STR_MSG_IDTIAMAT_TIAMAT_2PHASE_CLOSE_ALL()), 0);
	dies(219368);
	EXPECT_EQ(sent(SM_SYSTEM_MESSAGE::STR_MSG_IDTIAMAT_TIAMAT_2PHASE_CLOSE_ALL()), 1);
	executor().advance(std::chrono::milliseconds(29999));
	EXPECT_TRUE(lord->isSpawned());
	executor().advance(std::chrono::milliseconds(1));
	EXPECT_FALSE(lord->isSpawned());
	ASSERT_EQ(count({219489 + raceId * 3}), 1u);
	EXPECT_FLOAT_EQ(npcs(*instance, 219489 + raceId * 3).front()->getX(), 516.285f);
	handler().onDie(*lord); // the lord's death after the four kills waits for the progress
	EXPECT_EQ(sent(SM_SYSTEM_MESSAGE::STR_MSG_IDTIAMAT_TIAMAT_DEADLYHOWLING()), 0);
}

/** :200-212: the first lord's death before the four kills: the howling, then the refuge ends 5 s later (the roof stays) */
TEST_F(DragonLordsRefugeTest, TheLordsEarlyDeathEndsTheRefuge) {
	publishSkills({20983});
	withAi(219361);
	runtime::Ptr<Npc> incarnation = withAi(219366);
	runtime::Ptr<Npc> roof = at(730699);
	runtime::Ptr<Npc> other = at(900200);
	dies(219491);
	EXPECT_EQ(sent(SM_SYSTEM_MESSAGE::STR_MSG_IDTIAMAT_TIAMAT_DEADLYHOWLING()), 1);
	executor().advance(std::chrono::milliseconds(4999));
	EXPECT_TRUE(other->isSpawned());
	executor().advance(std::chrono::milliseconds(1));
	EXPECT_FALSE(other->isSpawned());
	EXPECT_FALSE(incarnation->isSpawned());
	EXPECT_TRUE(roof->isSpawned());
	EXPECT_EQ(count({730630}), 1u);
}

/** :271-279, 228-252: Tiamat's first sight of a player starts the dialogue once; 54.5 s later Kahrun and she go and Tiamat and Calindi come */
TEST_F(DragonLordsRefugeTest, TheDialogueBringsTiamatAndCalindi) {
	publishSkills({20597, 20918});
	runtime::Ptr<Npc> kahrun = withAi(800341);
	runtime::Ptr<Npc> tiamat = withAi(219360);
	handler().onCreatureDetected(*at(900200), player());
	handler().onCreatureDetected(*tiamat, *at(900300));
	executor().advance(std::chrono::milliseconds(30000)); // an npc starts nothing
	handler().onCreatureDetected(*tiamat, player());
	handler().onCreatureDetected(*tiamat, player());
	executor().advance(std::chrono::milliseconds(54499));
	EXPECT_TRUE(kahrun->isSpawned());
	EXPECT_EQ(count({219359}), 0u);
	executor().advance(std::chrono::milliseconds(1));
	EXPECT_FALSE(kahrun->isSpawned());
	EXPECT_FALSE(tiamat->isSpawned());
	EXPECT_EQ(count({219360}), 1u) << "once";
	EXPECT_EQ(count({219359}), 1u);
	EXPECT_FLOAT_EQ(npcs(*instance, 219359).front()->getX(), 483.463f);
}

/** :266-269: the first entry's race decides the lords */
TEST_F(DragonLordsRefugeTest, TheFirstRaceDecidesTheLords) {
	handler().onEnterInstance(player());
	player().getCommonData()->setRace(player().getRace() == model::Race::ELYOS ? model::Race::ASMODIANS : model::Race::ELYOS);
	handler().onEnterInstance(player());
	dies(219362);
	EXPECT_EQ(count({800350 + raceId * 6}), 1u);
	EXPECT_EQ(count({800350 + (1 - raceId) * 6}), 0u);
}

/** AnguishedDragonLordsRefugeInstance.java:20-74, 120-129: its own ids and exit; the dialogue brings its Tiamat and Calindi */
TEST_F(AnguishedRefugeTest, TheAnguishedRefugeHasItsOwnIds) {
	at(236276);
	executor().advance(std::chrono::milliseconds(70000));
	EXPECT_EQ(count({730673, 730674, 730675, 730676}), 4u);
	EXPECT_EQ(count({856483, 856484, 856485, 856486}), 4u);
	EXPECT_EQ(count({283163, 283164, 283165, 283166}), 0u);
	runtime::Ptr<Npc> other = at(900200);
	at(236277);
	executor().advance(std::chrono::milliseconds(1800000));
	EXPECT_EQ(sent(SM_SYSTEM_MESSAGE::IDTIAMAT_TIAMAT_COUNTDOWN_OVER()), 1);
	EXPECT_FALSE(other->isSpawned());
	EXPECT_EQ(count({833482}), 1u);
	EXPECT_EQ(count({730630}), 0u);
}

/** Anguished :151-159: a spiritualist goes as it dies */
TEST_F(AnguishedRefugeTest, TheAnguishedSpiritualistsGo) {
	EXPECT_FALSE(dies(856484)->isSpawned());
	EXPECT_TRUE(dies(283164)->isSpawned()) << "the normal refuge's spiritualist stays";
}

/** Anguished :140-148 (J-P6I13-2, ported): the weakened Tiamat's death spares the normal refuge's 219490, not its own 856022 */
TEST_F(AnguishedRefugeTest, TheAnguishedEndSparesTheNormalIds) {
	handler().onEnterInstance(player());
	runtime::Ptr<Npc> normalLord = at(219490);
	runtime::Ptr<Npc> ownLord = at(856022);
	runtime::Ptr<Npc> tiamat = at(236277);
	handler().onDie(*tiamat);
	EXPECT_TRUE(normalLord->isSpawned());
	EXPECT_FALSE(ownLord->isSpawned());
	EXPECT_FALSE(tiamat->isSpawned());
	EXPECT_EQ(count({702729}), 1u);
	EXPECT_EQ(count({701542}), 0u);
	EXPECT_EQ(count({833482}), 1u);
}

/** Anguished :160-171 (J-P6I13-1, ported): the first lord's early death ends the refuge 7 s later; its incarnations do not cast (the normal
 * refuge's spiritualists would) */
TEST_F(AnguishedRefugeTest, TheAnguishedLordsEarlyDeath) {
	publishSkills({20983});
	withAi(236276);
	runtime::Ptr<Npc> other = at(900200);
	dies(856023);
	EXPECT_EQ(sent(SM_SYSTEM_MESSAGE::STR_MSG_IDTIAMAT_TIAMAT_DEADLYHOWLING()), 1);
	executor().advance(std::chrono::milliseconds(6999));
	EXPECT_TRUE(other->isSpawned());
	executor().advance(std::chrono::milliseconds(1));
	EXPECT_FALSE(other->isSpawned());
}

/** Anguished :176-186: the incarnations' four deaths close the phase; 30 s later the race's second lord */
TEST_F(AnguishedRefugeTest, TheAnguishedIncarnations) {
	publishSkills({20979, 20980, 20981, 20982, 20984});
	handler().onEnterInstance(player());
	withAi(236276);
	runtime::Ptr<Npc> lord = at(856020 + raceId * 3);
	for (int32_t id = 236278; id <= 236281; ++id)
		dies(id);
	EXPECT_EQ(sent(SM_SYSTEM_MESSAGE::STR_MSG_IDTIAMAT_TIAMAT_2PHASE_CLOSE_ALL()), 1);
	executor().advance(std::chrono::milliseconds(30000));
	EXPECT_FALSE(lord->isSpawned());
	EXPECT_EQ(count({856021 + raceId * 3}), 1u);
}

} // namespace
} // namespace aion::gameserver::handlers::instance::test
