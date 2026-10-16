// P6I-6 (docs/design/p6-instances-plan.md §4): DanuarMysticariumInstance, SauroSupplyBaseInstance, TalocsHollowInstance and the three Danuar
// Reliquaries (game-server/data/handlers/instance) on real instances of their maps. Teleports (they reach HousingService, which loads from the
// database), the item grants (no item rows), the generators' skills (no skill rows) and the doors (no door npcs) are left to a gate.

#include "I5TestSupport.h"

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <map>
#include <regex>
#include <sstream>
#include <string>
#include <vector>

#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/handlers/instance/DanuarMysticariumInstance.h"
#include "aion/gameserver/handlers/instance/DanuarReliquaryInstance.h"
#include "aion/gameserver/handlers/instance/DanuarReliquaryInstance_L.h"
#include "aion/gameserver/handlers/instance/InfernalDanuarReliquaryInstance.h"
#include "aion/gameserver/handlers/instance/NightmareCircus.h"
#include "aion/gameserver/handlers/instance/SauroSupplyBaseInstance.h"
#include "aion/gameserver/handlers/instance/TalocsHollowInstance.h"
#include "aion/gameserver/network/aion/serverpackets/SM_PLAY_MOVIE.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"

namespace aion::gameserver::handlers::instance::test {
namespace {

using model::gameobjects::Npc;
using model::gameobjects::player::Player;
using network::aion::serverpackets::SM_PLAY_MOVIE;
using network::aion::serverpackets::SM_SYSTEM_MESSAGE;

class I5InstanceTest : public InstanceHandlerTest {
protected:
	world::WorldMapInstance& mysticarium() { return create(DANUAR_MYSTICARIUM, DanuarMysticariumInstance_instanceFactory); }
	world::WorldMapInstance& sauro() { return create(SAURO_SUPPLY_BASE, SauroSupplyBaseInstance_instanceFactory); }
	world::WorldMapInstance& talocs() { return create(TALOCS_HOLLOW, TalocsHollowInstance_instanceFactory); }
	world::WorldMapInstance& reliquary() { return create(DANUAR_RELIQUARY, DanuarReliquaryInstance_instanceFactory); }

	runtime::Ptr<Npc> at(world::WorldMapInstance& instance, int32_t npcId, float x = 300.0f, float y = 300.0f) {
		return spawnNpc(instance, npcId, x, y, 300.0f, int8_t{0});
	}

	runtime::Ptr<Npc> dies(world::WorldMapInstance& instance, int32_t npcId) {
		runtime::Ptr<Npc> npc = at(instance, npcId);
		instance.getInstanceHandler()->onDie(*npc);
		return npc;
	}

	void use(world::WorldMapInstance& instance, Npc& npc) {
		instance.getInstanceHandler()->handleUseItemFinish(runtime::Ptr<Player>(player()), npc);
	}

	static size_t count(world::WorldMapInstance& instance, std::initializer_list<int32_t> ids) {
		size_t n = 0;
		for (int32_t id : ids)
			n += npcs(instance, id).size();
		return n;
	}

	static std::string javaSource(const char* javaClass) {
		std::ifstream in(std::filesystem::path(AION_GAMESERVER_JAVA_DIR) / "data/handlers/instance" / (std::string(javaClass) + ".java"));
		std::stringstream text;
		text << in.rdbuf();
		return text.str();
	}
};

TEST_F(I5InstanceTest, EachHandlerIsTheJavaClassOfItsMap) {
	for (const auto& [javaClass, mapId] : std::map<std::string, int32_t>{{"DanuarMysticariumInstance", DANUAR_MYSTICARIUM},
		     {"SauroSupplyBaseInstance", SAURO_SUPPLY_BASE}, {"TalocsHollowInstance", TALOCS_HOLLOW}, {"DanuarReliquaryInstance", DANUAR_RELIQUARY},
		     {"DanuarReliquaryInstance_L", LUCKY_DANUAR_RELIQUARY}, {"InfernalDanuarReliquaryInstance", INFERNAL_DANUAR_RELIQUARY},
		     {"NightmareCircus", NIGHTMARE_CIRCUS}}) {
		std::string source = javaSource(javaClass.c_str());
		std::smatch match;
		ASSERT_TRUE(std::regex_search(source, match, std::regex(R"(@InstanceID\((\d+)\))"))) << javaClass;
		EXPECT_EQ(std::stoi(match[1].str()), mapId) << javaClass;
	}
	EXPECT_NE(dynamic_cast<DanuarMysticariumInstance*>(mysticarium().getInstanceHandler().get()), nullptr);
	EXPECT_NE(dynamic_cast<SauroSupplyBaseInstance*>(sauro().getInstanceHandler().get()), nullptr);
	EXPECT_NE(dynamic_cast<TalocsHollowInstance*>(talocs().getInstanceHandler().get()), nullptr);
	EXPECT_NE(dynamic_cast<DanuarReliquaryInstance*>(reliquary().getInstanceHandler().get()), nullptr);
	world::WorldMapInstance& lucky = create(LUCKY_DANUAR_RELIQUARY, DanuarReliquaryInstance_L_instanceFactory);
	EXPECT_NE(dynamic_cast<DanuarReliquaryInstance_L*>(lucky.getInstanceHandler().get()), nullptr);
	world::WorldMapInstance& infernal = create(INFERNAL_DANUAR_RELIQUARY, InfernalDanuarReliquaryInstance_instanceFactory);
	EXPECT_NE(dynamic_cast<InfernalDanuarReliquaryInstance*>(infernal.getInstanceHandler().get()), nullptr);
	world::WorldMapInstance& circus = create(NIGHTMARE_CIRCUS, NightmareCircus_instanceFactory);
	EXPECT_NE(dynamic_cast<NightmareCircus*>(circus.getInstanceHandler().get()), nullptr);
}

/** NightmareCircus.java:56-287: the end of movie 983 starts the circus once - eight walkers a wave every 10 s (fifteen waves), Viloa at 160 s,
 * Reshka at 200 s, Heiramune at 300 s; Viloa leaves at 210 s, Reshka at 310 s (the walkers' DummyAI cannot walk: Java's ClassCastException,
 * logged by the task after the spawn) */
TEST_F(I5InstanceTest, TheCircusStartsOnceAfterItsMovie) {
	world::WorldMapInstance& instance = create(NIGHTMARE_CIRCUS, NightmareCircus_instanceFactory);
	enter(instance, 520.0f, 560.0f, 199.0f);
	at(instance, 831747);
	instance.getInstanceHandler()->onPlayMovieEnd(player(), 982);
	executor().advance(std::chrono::milliseconds(0));
	EXPECT_EQ(count(instance, {233450, 233455}), 0u) << "another movie";
	instance.getInstanceHandler()->onPlayMovieEnd(player(), 983);
	instance.getInstanceHandler()->onPlayMovieEnd(player(), 983);
	executor().advance(std::chrono::milliseconds(0));
	EXPECT_EQ(count(instance, {233455}), 4u) << "left wave 1";
	EXPECT_EQ(count(instance, {233450}), 4u) << "right wave 1";
	executor().advance(std::chrono::seconds(3));
	EXPECT_EQ(count(instance, {831747}), 0u);
	executor().advance(std::chrono::seconds(137));
	EXPECT_EQ(count(instance, {233450, 233455}), 120u) << "fifteen waves of eight";
	EXPECT_EQ(count(instance, {233459}), 0u);
	executor().advance(std::chrono::seconds(20));
	EXPECT_EQ(count(instance, {233459}), 1u) << "Viloa at 160 s";
	executor().advance(std::chrono::seconds(40));
	EXPECT_EQ(count(instance, {233453}), 1u) << "Reshka at 200 s";
	executor().advance(std::chrono::seconds(10));
	EXPECT_EQ(count(instance, {233459}), 0u) << "Viloa leaves at 210 s";
	executor().advance(std::chrono::seconds(90));
	EXPECT_EQ(count(instance, {233467}), 1u) << "Heiramune at 300 s";
	executor().advance(std::chrono::seconds(10));
	EXPECT_EQ(count(instance, {233453}), 0u) << "Reshka leaves at 310 s";
}

/** NightmareCircus.java:356-360, 48-54: destroyed with Viloa and Reshka up, they stay (the despawn tasks are cancelled) */
TEST_F(I5InstanceTest, TheCircusDestroyCancelsTheBossDespawns) {
	world::WorldMapInstance& instance = create(NIGHTMARE_CIRCUS, NightmareCircus_instanceFactory);
	enter(instance, 520.0f, 560.0f, 199.0f);
	instance.getInstanceHandler()->onPlayMovieEnd(player(), 983);
	executor().advance(std::chrono::seconds(200));
	ASSERT_EQ(count(instance, {233459, 233453}), 2u);
	instance.getInstanceHandler()->onInstanceDestroy();
	executor().advance(std::chrono::seconds(120));
	EXPECT_EQ(count(instance, {233459, 233453}), 2u);
}

/** NightmareCircus.java:356-360, 259-287: the destroy stops the waves and the boss despawns */
TEST_F(I5InstanceTest, TheCircusStopsWhenDestroyed) {
	world::WorldMapInstance& instance = create(NIGHTMARE_CIRCUS, NightmareCircus_instanceFactory);
	enter(instance, 520.0f, 560.0f, 199.0f);
	instance.getInstanceHandler()->onPlayMovieEnd(player(), 983);
	executor().advance(std::chrono::milliseconds(0));
	instance.getInstanceHandler()->onInstanceDestroy();
	executor().advance(std::chrono::seconds(400));
	EXPECT_EQ(count(instance, {233450, 233455}), 8u) << "only the first wave";
	EXPECT_EQ(count(instance, {233459, 233453, 233467}), 0u);
}

/** NightmareCircus.java:289-341: the walkers go when they die; Heiramune's death opens the cage, the crates, the exit and Yume's friends */
TEST_F(I5InstanceTest, HeiramunesDeathOpensTheCircus) {
	world::WorldMapInstance& instance = create(NIGHTMARE_CIRCUS, NightmareCircus_instanceFactory);
	enter(instance, 520.0f, 560.0f, 199.0f);
	runtime::Ptr<Npc> walker = dies(instance, 233450);
	EXPECT_FALSE(walker->isSpawned());
	runtime::Ptr<Npc> leftWalker = dies(instance, 233455);
	EXPECT_FALSE(leftWalker->isSpawned());
	at(instance, 831551);
	dies(instance, 233467);
	EXPECT_EQ(countSent(serialized(SM_PLAY_MOVIE(false, 0, 0, 984, true))), 1);
	EXPECT_EQ(count(instance, {831551}), 0u);
	ASSERT_EQ(count(instance, {831598}), 1u);
	EXPECT_EQ(npcs(instance, 831598)[0]->getSpawn()->getStaticId(), 14);
	EXPECT_EQ(count(instance, {831744}), 1u);
	EXPECT_EQ(count(instance, {831745}), 16u);
	EXPECT_EQ(count(instance, {831575}), 6u);
	EXPECT_EQ(count(instance, {831576}), 1u);
	EXPECT_EQ(count(instance, {831559, 831560, 831561, 831562}), 4u);
}

/** DanuarMysticariumInstance.java:46-58: the opened doors release their prisoners */
TEST_F(I5InstanceTest, MysticariumsDoorsReleaseThePrisoners) {
	world::WorldMapInstance& instance = mysticarium();
	instance.getInstanceHandler()->onOpenDoor(6);
	EXPECT_EQ(count(instance, {219964}), 1u);
	instance.getInstanceHandler()->onOpenDoor(13);
	EXPECT_EQ(count(instance, {219969}), 1u);
	instance.getInstanceHandler()->onOpenDoor(101);
	EXPECT_EQ(count(instance, {219963}), 1u);
	instance.getInstanceHandler()->onOpenDoor(9);
	EXPECT_EQ(count(instance, {219963, 219964, 219965, 219969}), 3u) << "door 9 holds no prisoner";
	instance.getInstanceHandler()->onInstanceDestroy(); // no tasks yet (Java: tasks == null)
}

/** SauroSupplyBaseInstance.java:59-82: a grave robber, one Ranodim, 8 of the 23 chest points with the better chest */
TEST_F(I5InstanceTest, SaurosChestsAndRanodim) {
	world::WorldMapInstance& instance = sauro();
	EXPECT_EQ(count(instance, {230846}), 1u);
	EXPECT_EQ(count(instance, {230852, 233316, 233317}), 1u);
	EXPECT_EQ(count(instance, {230847}), 8u);
	EXPECT_EQ(count(instance, {230848}), 15u);
	world::WorldMapInstance& other = sauro();
	EXPECT_EQ(count(other, {230847}), 8u) << "the static list is copied, not consumed";
	EXPECT_EQ(count(other, {230848}), 15u);
}

/** :84-121: the bosses' messages, the boss teleporter and the exits */
TEST_F(I5InstanceTest, SaurosBossesOpenTheWay) {
	world::WorldMapInstance& instance = sauro();
	enter(instance, 300.0f, 300.0f, 300.0f);
	dies(instance, 230837);
	EXPECT_EQ(countSent(serialized(SM_SYSTEM_MESSAGE::STR_MSG_IDVritra_Base_DoorOpen_03())), 1);
	dies(instance, 233316);
	EXPECT_EQ(countSent(serialized(SM_SYSTEM_MESSAGE::STR_MSG_IDVritra_Base_DoorOpen_06())), 1);
	dies(instance, 230853);
	EXPECT_EQ(countSent(serialized(SM_SYSTEM_MESSAGE::STR_MSG_IDVritra_Base_DoorOpen_08())), 1);
	ASSERT_EQ(npcs(instance, 730872).size(), 1u);
	EXPECT_EQ(npcs(instance, 730872)[0]->getSpawn()->getStaticId(), 3);
	dies(instance, 230854);
	dies(instance, 230857);
	EXPECT_EQ(count(instance, {801967}), 2u);
}

/** :138-169: without Guard Captain Ahuradim a generator's death leaves no new generator */
TEST_F(I5InstanceTest, SaurosGeneratorsNeedAhuradim) {
	world::WorldMapInstance& instance = sauro();
	runtime::Ptr<Npc> generator = dies(instance, 284437);
	EXPECT_FALSE(generator->isSpawned());
	EXPECT_EQ(count(instance, {284437, 284445, 284446}), 0u);
	executor().advance(std::chrono::milliseconds(500)); // the skill task finds no generator
}

/** :218-224: the bosses */
TEST_F(I5InstanceTest, SaurosBosses) {
	world::WorldMapInstance& instance = sauro();
	EXPECT_TRUE(dynamic_cast<SauroSupplyBaseInstance&>(*instance.getInstanceHandler()).isBoss(*at(instance, 230857)));
	EXPECT_TRUE(dynamic_cast<SauroSupplyBaseInstance&>(*instance.getInstanceHandler()).isBoss(*at(instance, 233258)));
	EXPECT_FALSE(dynamic_cast<SauroSupplyBaseInstance&>(*instance.getInstanceHandler()).isBoss(*at(instance, 230837)));
}

/** TalocsHollowInstance.java:154-159, 177-185: each ring's movie once */
TEST_F(I5InstanceTest, TalocsRingsPlayTheirMoviesOnce) {
	world::WorldMapInstance& instance = talocs();
	enter(instance, 253.0f, 649.0f, 1171.0f);
	EXPECT_FALSE(instance.getInstanceHandler()->onPassFlyingRing(player(), "TALOCS_1"));
	instance.getInstanceHandler()->onPassFlyingRing(player(), "TALOCS_1");
	instance.getInstanceHandler()->onPassFlyingRing(player(), "TALOCS_2");
	instance.getInstanceHandler()->onPassFlyingRing(player(), "TALOCS_3");
	EXPECT_EQ(countSent(serialized(SM_PLAY_MOVIE(false, 0, 0, 463, true))), 1);
	EXPECT_EQ(countSent(serialized(SM_PLAY_MOVIE(false, 0, 0, 464, true))), 1);
}

/** :91-138: Octanus takes his barrier; Mosqua's death breaks the egg, whose death brings the wind; Celestius' death cleans the fragment */
TEST_F(I5InstanceTest, TalocsBossesChangeTheHollow) {
	world::WorldMapInstance& instance = talocs();
	enter(instance, 300.0f, 300.0f, 300.0f);
	at(instance, 700633);
	dies(instance, 215457);
	EXPECT_EQ(count(instance, {700633}), 0u);
	runtime::Ptr<Npc> egg = spawnStaticNpc(instance, 700738, 310.0f, 320.0f, 300.0f, 1);
	dies(instance, 215480);
	EXPECT_FALSE(egg->isSpawned());
	std::vector<runtime::Ptr<Npc>> cracked = npcs(instance, 700739);
	ASSERT_EQ(cracked.size(), 1u);
	EXPECT_FLOAT_EQ(cracked[0]->getX(), 310.0f);
	EXPECT_EQ(cracked[0]->getSpawn()->getStaticId(), 11);
	EXPECT_EQ(countSent(serialized(SM_SYSTEM_MESSAGE::STR_MSG_IDELIM_EGG_BREAK())), 1);
	instance.getInstanceHandler()->onDie(*cracked[0]);
	ASSERT_EQ(npcs(instance, 281817).size(), 1u);
	EXPECT_EQ(npcs(instance, 281817)[0]->getSpawn()->getStaticId(), 9);
	EXPECT_EQ(countSent(serialized(SM_SYSTEM_MESSAGE::STR_MSG_IDELIM_WIND_INFO())), 1);
	runtime::Ptr<Npc> fragment = spawnStaticNpc(instance, 700740, 330.0f, 320.0f, 300.0f, 2);
	dies(instance, 215488);
	EXPECT_FALSE(fragment->isSpawned());
	ASSERT_EQ(npcs(instance, 700741).size(), 1u);
	EXPECT_EQ(npcs(instance, 700741)[0]->getSpawn()->getStaticId(), 92);
	EXPECT_EQ(count(instance, {799503}), 1u);
}

/** :140-152: the healing objects heal (a full player: no change to assert) and go */
TEST_F(I5InstanceTest, TalocsHealingObjects) {
	world::WorldMapInstance& instance = talocs();
	enter(instance, 300.0f, 300.0f, 300.0f);
	runtime::Ptr<Npc> small = at(instance, 700940);
	use(instance, *small);
	EXPECT_FALSE(small->isSpawned());
	runtime::Ptr<Npc> big = at(instance, 700941);
	use(instance, *big);
	EXPECT_FALSE(big->isSpawned());
}

/** DanuarReliquaryInstance.java:61-74, 141-146, 122-139: the three guardians wake Cursed Modor; 15 minutes later the wipe ends the instance */
TEST_F(I5InstanceTest, ReliquarysGuardiansWakeModorAndTheWipeEndsIt) {
	world::WorldMapInstance& instance = reliquary();
	std::vector<runtime::Ptr<Npc>> guardians{at(instance, 284377), at(instance, 284378), at(instance, 284379)};
	guardians[0]->getController().die();
	guardians[1]->getController().die();
	EXPECT_EQ(count(instance, {231304}), 0u);
	guardians[2]->getController().die();
	EXPECT_EQ(count(instance, {231304}), 1u);
	executor().advance(std::chrono::minutes(15));
	EXPECT_EQ(count(instance, {231304, 284386}), 0u) << "everything cleared";
	EXPECT_EQ(count(instance, {730843}), 1u);
	EXPECT_EQ(count(instance, {701795}), 0u) << "no box after the wipe";
}

/** :75-86, 100-120: the real clone's death removes the fakes; 2 s later new clones, the third time Enraged Modor */
TEST_F(I5InstanceTest, ReliquarysClonesThenEnragedModor) {
	world::WorldMapInstance& instance = reliquary();
	at(instance, 284384);
	dies(instance, 284383);
	EXPECT_EQ(count(instance, {284383, 284384}), 0u);
	executor().advance(std::chrono::milliseconds(2000));
	EXPECT_EQ(count(instance, {284383}), 1u);
	EXPECT_EQ(count(instance, {284384}), 4u);
	instance.getInstanceHandler()->onDie(*npcs(instance, 284383)[0]);
	executor().advance(std::chrono::milliseconds(2000));
	EXPECT_EQ(count(instance, {231305}), 0u);
	instance.getInstanceHandler()->onDie(*npcs(instance, 284383)[0]);
	executor().advance(std::chrono::milliseconds(2000));
	EXPECT_EQ(count(instance, {231305}), 1u);
	EXPECT_EQ(count(instance, {284384}), 0u);
}

/** :87-90, 122-139: Enraged Modor's death: the exit and the treasure box, nothing else */
TEST_F(I5InstanceTest, ReliquarysSuccessLeavesTheBox) {
	world::WorldMapInstance& instance = reliquary();
	at(instance, 900200);
	dies(instance, 231305);
	EXPECT_EQ(count(instance, {900200, 231305}), 0u);
	EXPECT_EQ(count(instance, {730843}), 1u);
	EXPECT_EQ(count(instance, {701795}), 1u);
}

/** DanuarReliquaryInstance_L.java:21-24: the lucky reliquary's box */
TEST_F(I5InstanceTest, TheLuckyReliquarysBox) {
	world::WorldMapInstance& instance = create(LUCKY_DANUAR_RELIQUARY, DanuarReliquaryInstance_L_instanceFactory);
	dies(instance, 231305);
	EXPECT_EQ(count(instance, {802183}), 1u);
	EXPECT_EQ(count(instance, {701795}), 0u);
}

/** InfernalDanuarReliquaryInstance.java:48-72: Crazed Modor stays after her death, no box */
TEST_F(I5InstanceTest, TheInfernalReliquaryKeepsModor) {
	world::WorldMapInstance& instance = create(INFERNAL_DANUAR_RELIQUARY, InfernalDanuarReliquaryInstance_instanceFactory);
	at(instance, 900200);
	dies(instance, 234691);
	EXPECT_EQ(count(instance, {234691}), 1u);
	EXPECT_EQ(count(instance, {900200}), 0u);
	EXPECT_EQ(count(instance, {730843}), 1u);
	EXPECT_EQ(count(instance, {701795}), 0u);
}

/** DanuarReliquaryInstance.java:165-174: Modor back home clears the room; :160-163: the destroy stops the wipe */
TEST_F(I5InstanceTest, ReliquarysModorBackHomeAndDestroy) {
	world::WorldMapInstance& instance = reliquary();
	std::vector<runtime::Ptr<Npc>> guardians{at(instance, 284377), at(instance, 284378), at(instance, 284379)};
	for (runtime::Ptr<Npc>& guardian : guardians)
		guardian->getController().die();
	at(instance, 900200);
	runtime::Ptr<Npc> modor = npcs(instance, 231304)[0];
	instance.getInstanceHandler()->onBackHome(*modor);
	EXPECT_EQ(count(instance, {900200}), 0u);
	EXPECT_TRUE(modor->isSpawned());
	instance.getInstanceHandler()->onInstanceDestroy();
	executor().advance(std::chrono::minutes(15));
	EXPECT_EQ(count(instance, {730843}), 0u) << "no wipe";
}

} // namespace
} // namespace aion::gameserver::handlers::instance::test
