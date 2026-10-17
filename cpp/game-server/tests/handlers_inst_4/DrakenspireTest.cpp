// P6I-12 (docs/design/p6-instances-plan.md, I4 split): DrakenspireDepthsInstance (game-server/data/handlers/instance) on a real instance of
// its map. The doors (no static doors), the walks (no walker rows), the effects' starts and ends (no effect objects), the summon crystal's
// cast and the exit and revive teleports are left to a gate.

#include "I4TestSupport.h"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <memory>
#include <regex>
#include <sstream>
#include <string>
#include <vector>

#include "aion/gameserver/ai/AIState.h"
#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/handlers/ai/GeneralNpcAI.h"
#include "aion/gameserver/handlers/instance/DrakenspireDepthsInstance.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"

namespace aion::gameserver::handlers::instance::test {
namespace {

using model::gameobjects::Npc;
using network::aion::serverpackets::SM_SYSTEM_MESSAGE;

class DrakenspireTest : public InstanceHandlerTest {
protected:
	void SetUp() override {
		InstanceHandlerTest::SetUp();
		instance = &create(DRAKENSPIRE_DEPTHS, DrakenspireDepthsInstance_instanceFactory);
		enter(*instance, 300.0f, 300.0f, 300.0f);
		elyos = player().getRace() == model::Race::ELYOS;
	}

	runtime::Ptr<Npc> at(int32_t npcId, float x = 300.0f, float y = 300.0f) { return spawnNpc(*instance, npcId, x, y, 300.0f, int8_t{0}); }

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

	/** The twins' fight from the aggro to the fail at 120 s (the difficulty drops to normal) */
	void failTheTwins() {
		handler().onAggro(*at(236227));
		executor().advance(std::chrono::milliseconds(120000));
	}

	world::WorldMapInstance* instance = nullptr;
	bool elyos = true;
};

TEST_F(DrakenspireTest, TheHandlerIsTheJavaClassOfItsMap) {
	std::ifstream in(std::filesystem::path(AION_GAMESERVER_JAVA_DIR) / "data/handlers/instance/DrakenspireDepthsInstance.java");
	std::stringstream text;
	text << in.rdbuf();
	std::string source = text.str();
	std::smatch match;
	ASSERT_TRUE(std::regex_search(source, match, std::regex(R"(@InstanceID\((\d+)\))")));
	EXPECT_EQ(std::stoi(match[1].str()), DRAKENSPIRE_DEPTHS);
	EXPECT_NE(dynamic_cast<DrakenspireDepthsInstance*>(instance->getInstanceHandler().get()), nullptr);
	auto& drakenspire = dynamic_cast<DrakenspireDepthsInstance&>(handler());
	for (int32_t id : {236227, 236228, 236231, 236234, 236244, 236245, 236247})
		EXPECT_TRUE(drakenspire.isBoss(*at(id))) << id;
	EXPECT_FALSE(drakenspire.isBoss(*at(236246)));
}

/** DrakenspireDepthsInstance.java:71-86: the first entry's race brings its six npcs, once */
TEST_F(DrakenspireTest, TheFirstRaceBringsItsNpcs) {
	handler().onEnterInstance(player());
	EXPECT_EQ(count({elyos ? 209678 : 209743}), 1u);
	EXPECT_EQ(count({elyos ? 209680 : 209745}), 2u);
	EXPECT_EQ(count({elyos ? 209681 : 209746}), 2u);
	EXPECT_EQ(count({elyos ? 209743 : 209678}), 0u);
	handler().onEnterInstance(player());
	EXPECT_EQ(count({elyos ? 209680 : 209745}), 2u) << "once";
	EXPECT_FLOAT_EQ(npcs(*instance, elyos ? 209679 : 209744).front()->getY(), 179.259f);
}

/** :88-139: the spawns' messages: Orissan's, the siege weapon's three, Virtsha's, the protectors' two; the dragon's pillar (static 383) */
TEST_F(DrakenspireTest, TheSpawnsTellTheirMessages) {
	at(236230);
	EXPECT_EQ(sent(SM_SYSTEM_MESSAGE::STR_MSG_IDSEAL_IMMORTAL_03()), 0);
	executor().advance(std::chrono::milliseconds(2500));
	EXPECT_EQ(sent(SM_SYSTEM_MESSAGE::STR_MSG_IDSEAL_IMMORTAL_03()), 1);
	at(702720);
	executor().advance(std::chrono::milliseconds(9000));
	EXPECT_EQ(sent(SM_SYSTEM_MESSAGE::STR_MSG_IDSEAL_WAVE_01()), 1);
	EXPECT_EQ(sent(SM_SYSTEM_MESSAGE::STR_MSG_IDSEAL_WAVE_03()), 1);
	at(236243);
	EXPECT_EQ(sent(SM_SYSTEM_MESSAGE::STR_MSG_IDSEAL_WAVE_08()), 1);
	at(855464);
	EXPECT_EQ(sent(SM_SYSTEM_MESSAGE::STR_MSG_IDSEAL_GUARDIAN_01()), 1);
	executor().advance(std::chrono::milliseconds(3000));
	EXPECT_EQ(sent(SM_SYSTEM_MESSAGE::STR_MSG_IDSEAL_GUARDIAN_02()), 1);
	at(236247);
	ASSERT_EQ(count({702699}), 1u);
	EXPECT_EQ(npcs(*instance, 702699).front()->getSpawn()->getStaticId(), 383);
}

/** :146-155, 257-264: a twin dying alone leaves its sphere; 15 s later it comes back where the sphere still is, and the spheres go */
TEST_F(DrakenspireTest, ALoneTwinComesBack) {
	dies(236227, 310.0f, 320.0f);
	ASSERT_EQ(count({855708}), 1u);
	EXPECT_FLOAT_EQ(npcs(*instance, 855708).front()->getX(), 310.0f);
	EXPECT_EQ(sent(SM_SYSTEM_MESSAGE::STR_MSG_IDSEAL_TWIN_RESSURECT_02()), 1);
	executor().advance(std::chrono::milliseconds(14999));
	EXPECT_EQ(count({236227}), 1u) << "the dead one only";
	executor().advance(std::chrono::milliseconds(1));
	EXPECT_EQ(count({236227}), 2u);
	EXPECT_TRUE(std::ranges::any_of(npcs(*instance, 236227), [](const runtime::Ptr<Npc>& twin) { return twin->getX() == 531.0885f; }));
	EXPECT_EQ(count({855708, 855709}), 0u);
	EXPECT_EQ(sent(SM_SYSTEM_MESSAGE::STR_MSG_IDSEAL_TWIN_RESSURECT_03()), 1);
	EXPECT_EQ(count({236228}), 0u);
}

/** :146-150, 266-292: the second twin dying while the other's sphere is out completes the stage: the hard mode Orissan */
TEST_F(DrakenspireTest, TheTwinsTogetherBringHardOrissan) {
	handler().onEnterInstance(player());
	dies(236228);
	ASSERT_EQ(count({855709}), 1u);
	dies(236227);
	EXPECT_EQ(sent(SM_SYSTEM_MESSAGE::STR_MSG_IDSEAL_TWIN_07()), 1);
	EXPECT_EQ(count({855708, 855709}), 0u);
	EXPECT_EQ(count({236232}), 1u);
	EXPECT_EQ(count({236229}), 0u);
	executor().advance(std::chrono::milliseconds(15000));
	EXPECT_EQ(count({236227}), 1u) << "the cancelled respawn";
	EXPECT_EQ(sent(SM_SYSTEM_MESSAGE::STR_MSG_IDSEAL_TWIN_RESSURECT_03()), 0) << "the respawn task was cancelled";
	EXPECT_EQ(count({elyos ? 209695 : 209760}), 0u);
	executor().advance(std::chrono::milliseconds(3000)); // 2 s + 16 s
	EXPECT_EQ(count({elyos ? 209695 : 209760}), 1u);
	EXPECT_EQ(count({elyos ? 209694 : 209759}), 1u);
}

/** :216-255, 285-292: the twins' aggro starts the 5 minute clock once; at its end the fountless twins come; their deaths bring normal Orissan */
TEST_F(DrakenspireTest, TheTwinsClockRunsOut) {
	handler().onAggro(*at(236227));
	handler().onAggro(*at(236228));
	EXPECT_EQ(sent(SM_SYSTEM_MESSAGE::STR_MSG_IDSEAL_TWIN_01()), 1) << "once";
	executor().advance(std::chrono::milliseconds(5000));
	EXPECT_EQ(sent(SM_SYSTEM_MESSAGE::STR_MSG_IDSEAL_TWIN_03()), 1);
	executor().advance(std::chrono::milliseconds(94999));
	EXPECT_EQ(sent(SM_SYSTEM_MESSAGE::STR_MSG_IDSEAL_TWIN_04()), 0) << "not before the eighth run";
	executor().advance(std::chrono::milliseconds(1)); // 100 s: the eighth run
	EXPECT_EQ(sent(SM_SYSTEM_MESSAGE::STR_MSG_IDSEAL_TWIN_04()), 1);
	EXPECT_EQ(count({236225}), 0u);
	executor().advance(std::chrono::milliseconds(19999));
	EXPECT_EQ(sent(SM_SYSTEM_MESSAGE::STR_MSG_IDSEAL_TWIN_05()), 1) << "the ninth run, 5 s delayed";
	EXPECT_EQ(sent(SM_SYSTEM_MESSAGE::STR_MSG_IDSEAL_TWIN_06()), 0);
	dies(236228); // alone: its sphere and a respawn in 15 s, which the fail cancels
	executor().advance(std::chrono::milliseconds(1));
	EXPECT_EQ(sent(SM_SYSTEM_MESSAGE::STR_MSG_IDSEAL_TWIN_06()), 1);
	EXPECT_EQ(count({236227, 236228}), 0u);
	EXPECT_EQ(count({236225}), 1u);
	EXPECT_EQ(count({236226}), 1u);
	executor().advance(std::chrono::milliseconds(60000));
	EXPECT_EQ(sent(SM_SYSTEM_MESSAGE::STR_MSG_IDSEAL_TWIN_06()), 1) << "cancelled";
	EXPECT_EQ(sent(SM_SYSTEM_MESSAGE::STR_MSG_IDSEAL_TWIN_RESSURECT_03()), 0) << "the fail cancelled the respawn";
	runtime::Ptr<Npc> lava = npcs(*instance, 236225).front();
	handler().onDie(*lava);
	EXPECT_EQ(count({236229}), 0u) << "the other still lives";
	lava->getController().delete_(); // dead and gone: getNpc no longer finds it
	handler().onDie(*npcs(*instance, 236226).front());
	EXPECT_EQ(count({236229}), 1u);
	EXPECT_EQ(count({236232}), 0u);
}

/** :294-334: Orissan's exhausted forms count his phases: the second tells it, the third lowers the difficulty by 2 (normal tells 07), the
 * fourth tells the easy mode's 10 */
TEST_F(DrakenspireTest, OrissansPhasesLowerTheDifficulty) {
	failTheTwins();
	for (int32_t i = 0; i < 4; ++i)
		at(236231);
	executor().advance(std::chrono::milliseconds(5000));
	EXPECT_EQ(sent(SM_SYSTEM_MESSAGE::STR_MSG_IDSEAL_IMMORTAL_06()), 1);
	EXPECT_EQ(sent(SM_SYSTEM_MESSAGE::STR_MSG_IDSEAL_IMMORTAL_07()), 1);
	EXPECT_EQ(sent(SM_SYSTEM_MESSAGE::STR_MSG_IDSEAL_IMMORTAL_08()), 0);
	EXPECT_EQ(sent(SM_SYSTEM_MESSAGE::STR_MSG_IDSEAL_IMMORTAL_10()), 1);
	EXPECT_EQ(sent(SM_SYSTEM_MESSAGE::STR_MSG_IDSEAL_IMMORTAL_09()), 0);
	EXPECT_EQ(sent(SM_SYSTEM_MESSAGE::STR_MSG_IDSEAL_IMMORTAL_05()), 4);
}

/** :174-178, 336-352: Orissan's death takes the crystals, resets the count, brings the wave sensor and 4 s later the race's npcs */
TEST_F(DrakenspireTest, OrissansDeathOpensTheWaves) {
	handler().onEnterInstance(player());
	runtime::Ptr<Npc> crystal = at(855699);
	dies(236234);
	EXPECT_FALSE(crystal->isSpawned());
	EXPECT_EQ(count({206405}), 1u);
	EXPECT_EQ(count({elyos ? 209711 : 209776}), 0u);
	executor().advance(std::chrono::milliseconds(4000));
	EXPECT_EQ(count({elyos ? 209711 : 209776}), 1u);
	EXPECT_EQ(count({elyos ? 209705 : 209770}), 2u);
	at(236231); // the count starts over: the first phase tells nothing
	executor().advance(std::chrono::milliseconds(5000));
	EXPECT_EQ(sent(SM_SYSTEM_MESSAGE::STR_MSG_IDSEAL_IMMORTAL_06()), 0);
}

/** :180-184, 433-481: the commanders' kills complete the waves at the difficulty's count (5 on hard): a perfect defence brings Beritra */
TEST_F(DrakenspireTest, TheCommandersKillsCompleteTheWaves) {
	handler().onEnterInstance(player());
	publishSkills({20838});
	runtime::Ptr<Npc> weapon = at(elyos ? 702719 : 702720);
	auto root = std::make_unique<ai::GeneralNpcAI>(*weapon);
	ai::GeneralNpcAI& weaponAi = *root;
	weapon->replaceAi(std::move(root));
	weaponAi.setStateIfNot(gameserver::ai::AIState::IDLE);
	at(731580, 310.0f);
	runtime::Ptr<Npc> attacker = at(236216);
	runtime::Ptr<Npc> portal = at(731581);
	for (int32_t i = 0; i < 4; ++i)
		dies(236239 + i % 5);
	EXPECT_EQ(count({236245}), 0u);
	EXPECT_TRUE(attacker->isSpawned());
	dies(236243);
	EXPECT_FALSE(attacker->isSpawned());
	EXPECT_FALSE(portal->isSpawned()) << "the wave portals go too";
	EXPECT_EQ(sent(SM_SYSTEM_MESSAGE::STR_MSG_IDSEAL_WAVE_BONUS_04()), 1);
	ASSERT_EQ(count({236245}), 1u);
	EXPECT_FLOAT_EQ(npcs(*instance, 236245).front()->getX(), 152.38f);
	EXPECT_TRUE(weapon->isSpawned());
}

/** :200-204, 476-479: the eighth fallen defender fails the waves: the siege weapons go */
TEST_F(DrakenspireTest, TheDefendersFallFailsTheWaves) {
	runtime::Ptr<Npc> weapon = at(702719);
	for (int32_t i = 0; i < 7; ++i)
		dies(236248 + i % 2);
	EXPECT_TRUE(weapon->isSpawned());
	dies(236249);
	EXPECT_FALSE(weapon->isSpawned());
	EXPECT_EQ(count({236244, 236245}), 0u);
}

/** :206-212: a blown gate decays after 4 s and tells its place's message */
TEST_F(DrakenspireTest, TheGatesTellTheirPlace) {
	runtime::Ptr<Npc> wave = spawnStaticNpc(*instance, 731580, 300.0f, 300.0f, 300.0f, 332);
	runtime::Ptr<Npc> twin = spawnStaticNpc(*instance, 731580, 310.0f, 300.0f, 300.0f, 603);
	runtime::Ptr<Npc> other = spawnStaticNpc(*instance, 731580, 320.0f, 300.0f, 300.0f, 1);
	for (Npc* npc : {wave.get(), twin.get(), other.get()})
		handler().onDie(*npc);
	EXPECT_EQ(sent(SM_SYSTEM_MESSAGE::STR_MSG_IDSEAL_WAVE_09()), 1);
	EXPECT_EQ(sent(SM_SYSTEM_MESSAGE::STR_MSG_IDSEAL_TWIN_08()), 1);
}

/** :185-193, 483-513: Beritra's death leaves his remains and the exit of the difficulty: hard has the hard mode's npcs and five exits */
TEST_F(DrakenspireTest, BeritraLeavesTheExit) {
	handler().onEnterInstance(player());
	dies(236245, 150.0f, 520.0f);
	ASSERT_EQ(count({833013}), 1u);
	EXPECT_FLOAT_EQ(npcs(*instance, 833013).front()->getX(), 150.0f);
	EXPECT_EQ(count({elyos ? 209738 : 209803}), 1u);
	EXPECT_EQ(count({elyos ? 209742 : 209807}), 2u);
	EXPECT_EQ(count({731548}), 5u);
}

/** :483-500: after the twins' fail the exit is the normal mode's */
TEST_F(DrakenspireTest, ANormalRunHasTheNormalExit) {
	handler().onEnterInstance(player());
	failTheTwins();
	dies(236244);
	EXPECT_EQ(count({833012}), 1u);
	EXPECT_EQ(count({elyos ? 209739 : 209804}), 1u);
	EXPECT_EQ(count({elyos ? 209742 : 209807}), 1u);
	EXPECT_EQ(count({elyos ? 209738 : 209803}), 0u);
}

/** :515-524, 354-377, 379-419: the wave sensor's player starts the waves once: 30 s later the siege weapon, 6 s after that the first portals */
TEST_F(DrakenspireTest, TheSensorStartsTheWaves) {
	handler().onEnterInstance(player());
	runtime::Ptr<Npc> sensor = at(206405);
	handler().onCreatureDetected(*sensor, *at(900200));
	EXPECT_TRUE(sensor->isSpawned()) << "an npc";
	handler().onCreatureDetected(*sensor, player());
	EXPECT_FALSE(sensor->isSpawned());
	EXPECT_EQ(count({elyos ? 209731 : 209796}), 1u);
	executor().advance(std::chrono::milliseconds(29999));
	EXPECT_EQ(count({elyos ? 702719 : 702720}), 0u);
	executor().advance(std::chrono::milliseconds(1));
	EXPECT_EQ(count({elyos ? 702719 : 702720}), 1u);
	EXPECT_EQ(count({elyos ? 209723 : 209788}), 1u);
	executor().advance(std::chrono::milliseconds(6000));
	ASSERT_EQ(count({731581}), 3u);
	std::vector<int32_t> staticIds;
	for (const runtime::Ptr<Npc>& portal : npcs(*instance, 731581))
		staticIds.push_back(portal->getSpawn()->getStaticId());
	std::ranges::sort(staticIds);
	EXPECT_EQ(staticIds, (std::vector<int32_t>{84, 405, 548}));
	EXPECT_EQ(sent(SM_SYSTEM_MESSAGE::STR_MSG_IDSEAL_WAVE_05()), 0);
	executor().advance(std::chrono::milliseconds(60000));
	EXPECT_EQ(sent(SM_SYSTEM_MESSAGE::STR_MSG_IDSEAL_WAVE_05()), 1) << "hard mode";
}

} // namespace
} // namespace aion::gameserver::handlers::instance::test
