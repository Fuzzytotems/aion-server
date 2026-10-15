// P6I-8a (docs/design/p6-instances-plan.md §4): the PvP instances of chunk I3 - BasicPvpInstance and IronWallWarfrontInstance,
// IdgelDomeInstance, KamarBattlefieldInstance, EngulfedOphidanBridgeInstance (game-server/data/handlers/instance/pvp/*.java) - on real
// instances of their maps, the fixture's DeterministicExecutor driving the phases. The player stands in another instance (an Ophidan Bridge)
// and joins the score through onEnterInstance: the start teleports (HousingService loads from the database) and the end's item rewards (no
// item rows) are left to a gate, and so is Iron Wall's start with a player inside (it calls the general portToStartPosition, which throws).

#include "I3TestSupport.h"

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <map>
#include <memory>
#include <regex>
#include <set>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/handlers/instance/OphidanBridgeInstance.h"
#include "aion/gameserver/handlers/instance/pvp/EngulfedOphidanBridgeInstance.h"
#include "aion/gameserver/handlers/instance/pvp/IdgelDomeInstance.h"
#include "aion/gameserver/handlers/instance/pvp/IronWallWarfrontInstance.h"
#include "aion/gameserver/handlers/instance/pvp/KamarBattlefieldInstance.h"
#include "aion/gameserver/model/gameobjects/player/motion/MotionList.h"
#include "aion/gameserver/model/instance/InstanceProgressionType.h"
#include "aion/gameserver/model/instance/instancescore/PvpInstanceScore.h"
#include "aion/gameserver/model/instance/playerreward/PvpInstancePlayerReward.h"
#include "aion/gameserver/model/templates/spawns/SpawnTemplate.h"

namespace aion::gameserver::handlers::instance::test {
namespace {

using model::gameobjects::Npc;
using model::gameobjects::player::Player;
using model::instance::InstanceProgressionType;
using model::instance::instancescore::PvpInstanceScore;
using model::instance::playerreward::PvpInstancePlayerReward;

class PvpTest : public InstanceHandlerTest {
protected:
	void SetUp() override {
		InstanceHandlerTest::SetUp();
		enter(create(OPHIDAN_BRIDGE, OphidanBridgeInstance_instanceFactory), 300.0f, 300.0f, 300.0f);
	}

	static PvpInstanceScore& scoreOf(world::WorldMapInstance& instance) {
		return dynamic_cast<PvpInstanceScore&>(*instance.getInstanceHandler()->getInstanceScore());
	}

	static InstanceProgressionType phase(world::WorldMapInstance& instance) { return scoreOf(instance).getInstanceProgressionType(); }

	/** the instance in START_PROGRESS after its phases (start: the delays to it), the player in its score */
	world::WorldMapInstance& started(int32_t mapId, ::aion::gameserver::handlers::InstanceFactory& factory, int64_t start) {
		world::WorldMapInstance& instance = create(mapId, factory);
		executor().advance(std::chrono::milliseconds(start));
		instance.getInstanceHandler()->onEnterInstance(player());
		return instance;
	}

	world::WorldMapInstance& ironWall() { return started(IRON_WALL_WARFRONT, pvp::IronWallWarfrontInstance_instanceFactory, 240000); }
	world::WorldMapInstance& idgel() { return started(IDGEL_DOME, pvp::IdgelDomeInstance_instanceFactory, 120000); }
	world::WorldMapInstance& kamar() { return started(KAMAR_BATTLEFIELD, pvp::KamarBattlefieldInstance_instanceFactory, 240000); }
	world::WorldMapInstance& engulfed() { return started(ENGULFED_OPHIDAN_BRIDGE, pvp::EngulfedOphidanBridgeInstance_instanceFactory, 120000); }

	runtime::Ptr<Npc> at(world::WorldMapInstance& instance, int32_t npcId, float x = 300.0f, float y = 300.0f) {
		return spawnNpc(instance, npcId, x, y, 300.0f, int8_t{0});
	}

	/** an npc the player hit, dead through the handler */
	runtime::Ptr<Npc> killed(world::WorldMapInstance& instance, int32_t npcId) {
		runtime::Ptr<Npc> npc = at(instance, npcId);
		hit(*npc);
		instance.getInstanceHandler()->onDie(*npc);
		return npc;
	}

	runtime::Ptr<Npc> used(world::WorldMapInstance& instance, int32_t npcId, int32_t staticId = 0) {
		runtime::Ptr<Npc> npc = staticId == 0 ? at(instance, npcId) : spawnStaticNpc(instance, npcId, 300.0f, 300.0f, 300.0f, staticId);
		instance.getInstanceHandler()->handleUseItemFinish(runtime::Ptr<Player>(player()), *npc);
		return npc;
	}

	int32_t playerPoints(world::WorldMapInstance& instance) {
		return runtime::cast<PvpInstancePlayerReward>(scoreOf(instance).getPlayerReward(player().getObjectId()))->getPoints();
	}

	static size_t count(world::WorldMapInstance& instance, std::initializer_list<int32_t> ids) {
		size_t n = 0;
		for (int32_t id : ids)
			n += npcs(instance, id).size();
		return n;
	}

	static std::string javaSource(const std::string& javaClass) {
		std::ifstream in(std::filesystem::path(AION_GAMESERVER_JAVA_DIR) / "data/handlers/instance/pvp" / (javaClass + ".java"));
		std::stringstream text;
		text << in.rdbuf();
		return text.str();
	}
};

TEST_F(PvpTest, EachHandlerIsTheJavaClassOfItsMap) {
	for (const auto& [javaClass, mapId] : std::map<std::string, int32_t>{{"IronWallWarfrontInstance", IRON_WALL_WARFRONT},
		     {"IdgelDomeInstance", IDGEL_DOME}, {"KamarBattlefieldInstance", KAMAR_BATTLEFIELD},
		     {"EngulfedOphidanBridgeInstance", ENGULFED_OPHIDAN_BRIDGE}}) {
		std::string source = javaSource(javaClass);
		std::smatch match;
		ASSERT_TRUE(std::regex_search(source, match, std::regex(R"(@InstanceID\((\d+)\))"))) << javaClass;
		EXPECT_EQ(std::stoi(match[1].str()), mapId) << javaClass;
	}
	EXPECT_NE(dynamic_cast<pvp::IronWallWarfrontInstance*>(
		          create(IRON_WALL_WARFRONT, pvp::IronWallWarfrontInstance_instanceFactory).getInstanceHandler().get()),
		nullptr);
	EXPECT_NE(dynamic_cast<pvp::IdgelDomeInstance*>(create(IDGEL_DOME, pvp::IdgelDomeInstance_instanceFactory).getInstanceHandler().get()), nullptr);
	EXPECT_NE(dynamic_cast<pvp::KamarBattlefieldInstance*>(
		          create(KAMAR_BATTLEFIELD, pvp::KamarBattlefieldInstance_instanceFactory).getInstanceHandler().get()),
		nullptr);
	EXPECT_NE(dynamic_cast<pvp::EngulfedOphidanBridgeInstance*>(
		          create(ENGULFED_OPHIDAN_BRIDGE, pvp::EngulfedOphidanBridgeInstance_instanceFactory).getInstanceHandler().get()),
		nullptr);
}

/** BasicPvpInstance.java:53-71 and Iron Wall :27-38, 146-155: 1000 points each, two minutes of reinforcement, a minute of preparation, the
 * start a minute later, the end after 40 minutes */
TEST_F(PvpTest, IronWallsPhases) {
	world::WorldMapInstance& instance = create(IRON_WALL_WARFRONT, pvp::IronWallWarfrontInstance_instanceFactory);
	EXPECT_EQ(scoreOf(instance).getElyosPoints(), 1000);
	EXPECT_EQ(scoreOf(instance).getAsmodiansPoints(), 1000);
	EXPECT_EQ(scoreOf(instance).getWinnerApReward(), 8400);
	EXPECT_EQ(phase(instance), InstanceProgressionType::REINFORCE_MEMBER);
	executor().advance(std::chrono::milliseconds(119999));
	EXPECT_EQ(phase(instance), InstanceProgressionType::REINFORCE_MEMBER);
	executor().advance(std::chrono::milliseconds(1));
	EXPECT_EQ(phase(instance), InstanceProgressionType::PREPARING);
	executor().advance(std::chrono::milliseconds(119999));
	EXPECT_EQ(phase(instance), InstanceProgressionType::PREPARING) << "onStart's own minute of preparation";
	executor().advance(std::chrono::milliseconds(1));
	EXPECT_EQ(phase(instance), InstanceProgressionType::START_PROGRESS);
	runtime::Ptr<Npc> bystander = at(instance, 900200);
	executor().advance(std::chrono::milliseconds(2399999));
	EXPECT_EQ(phase(instance), InstanceProgressionType::START_PROGRESS);
	executor().advance(std::chrono::milliseconds(1));
	EXPECT_EQ(phase(instance), InstanceProgressionType::END_PROGRESS);
	EXPECT_FALSE(bystander->isSpawned()) << "onStop deletes every npc";
}

/** Iron Wall :157-171 with the owner's correction (2026-10-15, docs/deviations/P6-instances.md): the start sends each player inside to their
 * race's side (Java: portToPosition overrides nothing, and the general portToStartPosition throws) */
TEST_F(PvpTest, IronWallsStartSendsEachRaceToItsSide) {
	world::WorldMapInstance& instance = create(IRON_WALL_WARFRONT, pvp::IronWallWarfrontInstance_instanceFactory);
	world::World& world = world::World::getInstance();
	std::vector<cptest::PlayerFixture> inside; // two players without a client; the actor stays on the Ophidan Bridge
	for (auto [objectId, race] : {std::pair{430002, model::Race::ELYOS}, std::pair{430003, model::Race::ASMODIANS}}) {
		inside.push_back(cptest::makePlayer(objectId, objectId + 5000, objectId == 430002 ? "Elyon" : "Daeva", race));
		Player& p = *inside.back().player;
		p.setMotions(std::make_unique<model::gameobjects::player::motion::MotionList>(p)); // the teleport packets read it
		world.storeObject(p);
		ASSERT_TRUE(world.setPosition(runtime::Ptr<model::gameobjects::VisibleObject>(p), instance.getMapId(), instance.getInstanceId(), 500.0f, 500.0f,
			300.0f, int8_t{0}));
		world.spawn(runtime::Ptr<model::gameobjects::VisibleObject>(p));
	}
	EXPECT_NO_THROW(executor().advance(std::chrono::milliseconds(240000)));
	EXPECT_EQ(phase(instance), InstanceProgressionType::START_PROGRESS) << "the start ran through";
	std::set<std::pair<float, float>> sides{{342.138f, 616.856f}, {711.403f, 621.797f}};
	std::pair<float, float> elyos{inside[0].player->getX(), inside[0].player->getY()};
	std::pair<float, float> asmo{inside[1].player->getX(), inside[1].player->getY()};
	EXPECT_TRUE(sides.contains(elyos)) << elyos.first;
	EXPECT_TRUE(sides.contains(asmo)) << asmo.first;
	EXPECT_NE(elyos, asmo) << "opposite sides";
	for (cptest::PlayerFixture& f : inside) { // out before the instance is destroyed (its exit needs the exit rows)
		f.player->getController().cancelAllTasks();
		if (f.player->isSpawned())
			world.despawn(*f.player);
		world.removeObject(*f.player);
	}
}

/** BasicPvpInstance.java:199-227, Iron Wall :72-99: the npc deaths score for the race of the player with the most damage, only after the
 * start */
TEST_F(PvpTest, IronWallsNpcsScoreForTheKillersRace) {
	world::WorldMapInstance& early = create(IRON_WALL_WARFRONT, pvp::IronWallWarfrontInstance_instanceFactory);
	early.getInstanceHandler()->onEnterInstance(player());
	killed(early, 233473);
	EXPECT_EQ(scoreOf(early).getElyosPoints(), 1000) << "REINFORCE_MEMBER: no points";

	world::WorldMapInstance& instance = ironWall();
	killed(instance, 233473);
	EXPECT_EQ(scoreOf(instance).getElyosPoints(), 1100);
	EXPECT_EQ(playerPoints(instance), 100);
	killed(instance, 702042);
	EXPECT_EQ(scoreOf(instance).getElyosPoints(), 1600);
	for (int32_t id : {233491, 701943, 701944, 701945})
		killed(instance, id);
	EXPECT_EQ(scoreOf(instance).getElyosPoints(), 21600);
	killed(instance, 900200);
	EXPECT_EQ(scoreOf(instance).getElyosPoints(), 21600) << "no points";
	EXPECT_EQ(scoreOf(instance).getAsmodiansPoints(), 1000);
	EXPECT_EQ(playerPoints(instance), 20600);
	EXPECT_EQ(phase(instance), InstanceProgressionType::START_PROGRESS) << "a lead of 20,600";
	runtime::Ptr<Npc> pashid = killed(instance, 233494);
	EXPECT_EQ(phase(instance), InstanceProgressionType::END_PROGRESS) << "Iron Wall :63-70: a lead of 30,000 ends the run";
	EXPECT_FALSE(pashid->isSpawned());
	world::WorldMapInstance& second = ironWall();
	killed(second, 233494);
	EXPECT_EQ(phase(second), InstanceProgressionType::END_PROGRESS) << "31,000 to 1,000: a lead of exactly 30,000";
}

/** Iron Wall :101-139: the used objects score and go; an Elyos at a flag post raises the Elyos flag and leaves the post */
TEST_F(PvpTest, IronWallsUsedObjects) {
	world::WorldMapInstance& instance = ironWall();
	runtime::Ptr<Npc> supply = used(instance, 801903);
	EXPECT_EQ(scoreOf(instance).getElyosPoints(), 2500);
	EXPECT_FALSE(supply->isSpawned());
	used(instance, 801772);
	used(instance, 801766);
	EXPECT_EQ(scoreOf(instance).getElyosPoints(), 2500 + 525 + 255);
	runtime::Ptr<Npc> post = used(instance, 730861);
	EXPECT_EQ(scoreOf(instance).getElyosPoints(), 2500 + 525 + 255 + 200);
	EXPECT_EQ(count(instance, {701900}), 1u);
	EXPECT_EQ(count(instance, {701901}), 0u);
	EXPECT_TRUE(post->isSpawned()) << "only the Asmodian branch deletes the post";
	runtime::Ptr<Npc> other = used(instance, 900200);
	EXPECT_TRUE(other->isSpawned());
	EXPECT_EQ(scoreOf(instance).getElyosPoints(), 2500 + 525 + 255 + 200);
}

/** BasicPvpInstance.java:126-144: a death costs the victim's race 100 points; an npc killer scores nothing */
TEST_F(PvpTest, ADeathCostsTheVictimsRace) {
	world::WorldMapInstance& instance = ironWall();
	runtime::Ptr<Npc> npc = at(instance, 900200);
	EXPECT_TRUE(instance.getInstanceHandler()->onDie(player(), *npc));
	EXPECT_EQ(scoreOf(instance).getElyosPoints(), 900);
	EXPECT_EQ(scoreOf(instance).getAsmodiansPoints(), 1000);
	EXPECT_EQ(playerPoints(instance), -100);
	EXPECT_EQ(scoreOf(instance).getElyosKills(), 0);
}

/** BasicPvpInstance.java:116-124, 157-168: the entry joins the score once; a leave before the end removes the reward */
TEST_F(PvpTest, TheScoreFollowsEntriesAndLeaves) {
	world::WorldMapInstance& instance = ironWall();
	instance.getInstanceHandler()->onEnterInstance(player());
	EXPECT_EQ(scoreOf(instance).getPlayerRewards().size(), 1u);
	instance.getInstanceHandler()->onLeaveInstance(player());
	EXPECT_FALSE(scoreOf(instance).containsPlayer(player().getObjectId()));
}

/** Idgel Dome :37-57: the score, both sides' quest npcs, turrets and levers at the creation; a minute of reinforcement and one of
 * preparation; Kunax after ten minutes; :115-136 his death ends the run */
TEST_F(PvpTest, IdgelDomesSidesAndKunax) {
	world::WorldMapInstance& instance = create(IDGEL_DOME, pvp::IdgelDomeInstance_instanceFactory);
	EXPECT_EQ(scoreOf(instance).getWinnerApReward(), 5600);
	EXPECT_EQ(count(instance, {802383}), 1u);
	EXPECT_EQ(count(instance, {802384}), 1u);
	EXPECT_EQ(count(instance, {702300}), 4u);
	EXPECT_EQ(count(instance, {702301}), 4u);
	EXPECT_EQ(count(instance, {802548}), 1u);
	EXPECT_EQ(count(instance, {802549}), 1u);
	executor().advance(std::chrono::milliseconds(60000));
	EXPECT_EQ(phase(instance), InstanceProgressionType::PREPARING);
	executor().advance(std::chrono::milliseconds(60000));
	EXPECT_EQ(phase(instance), InstanceProgressionType::START_PROGRESS);
	instance.getInstanceHandler()->onEnterInstance(player());
	killed(instance, 234186);
	killed(instance, 234751);
	EXPECT_EQ(scoreOf(instance).getElyosPoints(), 1320);
	executor().advance(std::chrono::milliseconds(599999));
	EXPECT_EQ(count(instance, {287249}), 0u);
	executor().advance(std::chrono::milliseconds(1));
	ASSERT_EQ(count(instance, {287249}), 1u);
	runtime::Ptr<Npc> kunax = npcs(instance, 287249).front();
	EXPECT_TRUE(dynamic_cast<pvp::IdgelDomeInstance&>(*instance.getInstanceHandler()).isBoss(*kunax));
	hit(*kunax);
	instance.getInstanceHandler()->onDie(*kunax);
	EXPECT_EQ(scoreOf(instance).getElyosPoints(), 7320);
	EXPECT_EQ(phase(instance), InstanceProgressionType::END_PROGRESS);
}

/** Idgel Dome :43-57: the four turrets of a side are one race's, the other side's the other race's (eight domes: both random sides,
 * most likely) */
TEST_F(PvpTest, IdgelDomesTurretsGoBySide) {
	for (int32_t round = 0; round < 8; ++round) {
		world::WorldMapInstance& instance = create(IDGEL_DOME, pvp::IdgelDomeInstance_instanceFactory);
		std::map<float, int32_t> turretAt;
		for (int32_t id : {702300, 702301})
			for (const runtime::Ptr<Npc>& turret : npcs(instance, id))
				turretAt[turret->getX()] = id;
		for (float x : {279.9443f, 263.9707f, 277.0629f})
			EXPECT_EQ(turretAt[x], turretAt[286.2041f]) << round;
		for (float x : {249.3263f, 252.1385f, 265.3730f})
			EXPECT_EQ(turretAt[x], turretAt[243.0305f]) << round;
		EXPECT_NE(turretAt[286.2041f], turretAt[243.0305f]) << round;
	}
}

/** Idgel Dome :138-160: a used chest's place gets a new chest two minutes later */
TEST_F(PvpTest, IdgelDomesChestsComeBack) {
	world::WorldMapInstance& instance = idgel();
	runtime::Ptr<Npc> chest = spawnNpc(instance, 702582, 310.0f, 320.0f, 300.0f, int8_t{7});
	instance.getInstanceHandler()->handleUseItemFinish(runtime::Ptr<Player>(player()), *chest);
	chest->getController().delete_();
	executor().advance(std::chrono::milliseconds(119999));
	EXPECT_EQ(count(instance, {702581, 702582, 702583}), 0u);
	executor().advance(std::chrono::milliseconds(1));
	ASSERT_EQ(count(instance, {702581, 702582, 702583}), 1u);
	runtime::Ptr<Npc> next;
	for (int32_t id : {702581, 702582, 702583})
		if (!npcs(instance, id).empty())
			next = npcs(instance, id).front();
	EXPECT_FLOAT_EQ(next->getX(), 310.0f);
	EXPECT_FLOAT_EQ(next->getY(), 320.0f);
	EXPECT_EQ(next->getHeading(), 7);
}

/** Kamar Battlefield :41-56, 58-134: the Garnon at one of nine places at the start; the teleporters after five minutes, the supplies after
 * ten, the generals after 18 */
TEST_F(PvpTest, KamarsTimeline) {
	world::WorldMapInstance& instance = kamar();
	EXPECT_EQ(phase(instance), InstanceProgressionType::START_PROGRESS);
	ASSERT_EQ(count(instance, {801903}), 1u);
	std::set<float> garnonXs{1138.4039f, 1184.5309f, 1241.9187f, 1270.4377f, 1325.634f, 1346.7902f, 1410.7446f, 1455.881f, 1540.113f};
	EXPECT_TRUE(garnonXs.contains(npcs(instance, 801903).front()->getX()));
	executor().advance(std::chrono::milliseconds(4 * 60000));
	EXPECT_EQ(count(instance, {802016, 802017, 802018, 802019}), 0u);
	executor().advance(std::chrono::milliseconds(60000));
	EXPECT_EQ(count(instance, {802016, 802017, 802018, 802019}), 4u);
	EXPECT_EQ(npcs(instance, 802016).front()->getSpawn()->getStaticId(), 223);
	executor().advance(std::chrono::milliseconds(5 * 60000));
	EXPECT_EQ(count(instance, {801772}), 3u);
	executor().advance(std::chrono::milliseconds(8 * 60000));
	EXPECT_EQ(count(instance, {232852}), 1u);
	EXPECT_EQ(count(instance, {232853}), 1u);
	EXPECT_EQ(count(instance, {232854}), 1u);
	EXPECT_EQ(count(instance, {232846}), 6u);
	std::set<float> generalXs;
	for (int32_t id : {232852, 232853, 232854})
		generalXs.insert(npcs(instance, id).front()->getX());
	EXPECT_EQ(generalXs, (std::set<float>{1437.7f, 1172.2f, 1428.67f})) << "each general at another place";
}

/** Kamar :156-212: 232853's death ends the run (Java stops before it scores, so its 3500 points are lost); a lead of 20,000 ends it too */
TEST_F(PvpTest, KamarsEnds) {
	world::WorldMapInstance& first = kamar();
	killed(first, 232846);
	EXPECT_EQ(scoreOf(first).getElyosPoints(), 1050);
	killed(first, 232853);
	EXPECT_EQ(scoreOf(first).getElyosPoints(), 1050) << "onStop comes first: the 3500 points find the run ended";
	EXPECT_EQ(phase(first), InstanceProgressionType::END_PROGRESS);

	world::WorldMapInstance& second = kamar();
	for (int32_t i = 0; i < 13; ++i)
		used(second, 801903);
	EXPECT_EQ(phase(second), InstanceProgressionType::START_PROGRESS) << "a lead of 19,500";
	used(second, 801772);
	EXPECT_EQ(phase(second), InstanceProgressionType::END_PROGRESS) << "a lead of 20,025";
}

/** Kamar :214-248: an Elyos at a flag post raises the Elyos flag, and the post goes */
TEST_F(PvpTest, KamarsFlagPosts) {
	world::WorldMapInstance& instance = kamar();
	runtime::Ptr<Npc> post = used(instance, 730861);
	EXPECT_EQ(scoreOf(instance).getElyosPoints(), 1200);
	EXPECT_EQ(count(instance, {701900}), 1u);
	EXPECT_FALSE(post->isSpawned());
}

/** Engulfed Ophidan Bridge :18-38, 40-46, 159-162, 202-216: the side npcs at the creation; every 30 s each race's generators score 100
 * each */
TEST_F(PvpTest, EngulfedGeneratorsScoreEvery30Seconds) {
	world::WorldMapInstance& instance = create(ENGULFED_OPHIDAN_BRIDGE, pvp::EngulfedOphidanBridgeInstance_instanceFactory);
	EXPECT_EQ(count(instance, {802025, 802026}), 2u);
	EXPECT_EQ(count(instance, {701948}), 5u);
	EXPECT_EQ(count(instance, {701949}), 5u);
	executor().advance(std::chrono::milliseconds(120000));
	EXPECT_EQ(phase(instance), InstanceProgressionType::START_PROGRESS);
	at(instance, 701943);
	at(instance, 701943);
	at(instance, 701944);
	executor().advance(std::chrono::milliseconds(29999));
	EXPECT_EQ(scoreOf(instance).getElyosPoints(), 1000);
	executor().advance(std::chrono::milliseconds(1));
	EXPECT_EQ(scoreOf(instance).getElyosPoints(), 1200);
	EXPECT_EQ(scoreOf(instance).getAsmodiansPoints(), 1100);
}

/** Engulfed :172-180 with :240-260, :290-305: the player captures a base (5000 points, the race's generator with the base's static id);
 * a generator's death returns the base to the Balaur and costs its race 5000 */
TEST_F(PvpTest, EngulfedBasesChangeHands) {
	world::WorldMapInstance& instance = engulfed();
	runtime::Ptr<Npc> balaur = used(instance, 701945, 158);
	EXPECT_FALSE(balaur->isSpawned());
	EXPECT_EQ(scoreOf(instance).getElyosPoints(), 6000);
	ASSERT_EQ(count(instance, {701943}), 1u);
	runtime::Ptr<Npc> generator = npcs(instance, 701943).front();
	EXPECT_EQ(generator->getSpawn()->getStaticId(), 169);
	used(instance, 900200);
	EXPECT_EQ(scoreOf(instance).getElyosPoints(), 6000) << "not a base";

	runtime::Ptr<Npc> asmodian = spawnStaticNpc(instance, 701944, 300.0f, 300.0f, 300.0f, 173);
	instance.getInstanceHandler()->onDie(*asmodian);
	EXPECT_FALSE(asmodian->isSpawned());
	ASSERT_EQ(count(instance, {701945}), 1u);
	EXPECT_EQ(npcs(instance, 701945).front()->getSpawn()->getStaticId(), 164);
	EXPECT_EQ(scoreOf(instance).getElyosPoints(), 6000);
	EXPECT_EQ(scoreOf(instance).getAsmodiansPoints(), 1000) << "the generator's race: the template's (none here) - not the Asmodians'";
}

/** Engulfed :48-66: after five minutes a supply at each base by its owner; the scrolls' messages */
TEST_F(PvpTest, EngulfedSuppliesAfterFiveMinutes) {
	world::WorldMapInstance& instance = engulfed();
	at(instance, 802038); // the north base is the Balaur's
	at(instance, 802033); // the defense base the Elyos'
	executor().advance(std::chrono::milliseconds(270000));
	EXPECT_EQ(count(instance, {702042, 702043, 702044}), 0u);
	executor().advance(std::chrono::milliseconds(30000));
	ASSERT_EQ(count(instance, {702042}), 1u);
	EXPECT_FLOAT_EQ(npcs(instance, 702042).front()->getX(), 545.2472f);
	ASSERT_EQ(count(instance, {702043}), 1u);
	EXPECT_FLOAT_EQ(npcs(instance, 702043).front()->getX(), 693.6922f);
	EXPECT_EQ(count(instance, {702044}), 0u);
	EXPECT_EQ(count(instance, {701984, 701985}), 2u);
}

} // namespace
} // namespace aion::gameserver::handlers::instance::test
