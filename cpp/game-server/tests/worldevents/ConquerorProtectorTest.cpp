// P5-12b ConquerorAndProtectorService's PvP bodies and CPBuff (m5j-plan.md §18.1 stage 1 CP4, item S-12), with CM_SHOW_MAP's intruder scan:
// a kill raises the killer's conqueror rank, its stat buff and the packets to him and to those who see him; the level and map limits; the
// kills decrease task; the slayer announcement of a rank-3 conqueror's death; the protector's intruder scan and its cooldown. Real Players with
// recording connections from the party fixture (tests/team/P5-10b, by relative path), on Poeta (210010000, an Elyos map: an Elyos is a
// protector there, an Asmodian a conqueror).
//
// Expectations are derived by hand from ConquerorAndProtectorService.java:146-252, CPBuff.java:14-27, CM_SHOW_MAP.java:30-43 and
// conqueror_protector_ranks.xml (the shipped rows below). Not driven: the Legion Dominion arms (lane B's legion work owns onLeaveLegion and
// resetLegionDominionRank; isOccupiedLegionDominionZone needs a legion with an occupied dominion).

#include "../team/P5-10b/TeamTestSupport.h"

#include <cstdint>
#include <unordered_set>

#include "aion/gameserver/configs/main/CustomConfig.h"
#include "aion/gameserver/dataholders/ConquerorAndProtectorData.bind.h"
#include "aion/gameserver/dataholders/ConquerorAndProtectorData.h"
#include "aion/gameserver/model/stats/calc/Stat2.h"
#include "aion/gameserver/model/stats/container/PlayerGameStats.h"
#include "aion/gameserver/model/stats/container/StatEnum.h"
#include "aion/gameserver/model/templates/cp/CPType.h"
#include "aion/gameserver/network/aion/clientpackets/CM_SHOW_MAP.h"
#include "aion/gameserver/network/aion/serverpackets/SM_CONQUEROR_PROTECTOR.h"
#include "aion/gameserver/services/conquerorAndProtectorSystem/CPInfo.h"
#include "aion/gameserver/services/conquerorAndProtectorSystem/ConquerorAndProtectorService.h"
#include "aion/gameserver/world/World.h"
#include "aion/gameserver/world/WorldMapInstance.h"
#include "aion/gameserver/world/knownlist/KnownList.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets::testing::team {
namespace {

using network::test::PacketWriter;
using serverpackets::SM_CONQUEROR_PROTECTOR;
using services::conquerorAndProtectorSystem::ConquerorAndProtectorService;
using StatEnum = model::stats::container::StatEnum;

// conqueror_protector_ranks.xml, verbatim
constexpr std::string_view RANKS_XML = R"xml(<conqueror_protector_ranks>
	<rank type="CONQUEROR" rank_num="1"><modifiers><add name="PVP_ATTACK_RATIO" value="10" bonus="true"/></modifiers></rank>
	<rank type="CONQUEROR" rank_num="2"><modifiers><add name="PVP_ATTACK_RATIO" value="20" bonus="true"/></modifiers></rank>
	<rank type="CONQUEROR" rank_num="3"><modifiers><add name="PVP_ATTACK_RATIO" value="30" bonus="true"/></modifiers></rank>
	<rank type="PROTECTOR" rank_num="1" visible_intruder_min_rank="3"><modifiers><add name="PVP_DEFEND_RATIO" value="20" bonus="true"/></modifiers></rank>
	<rank type="PROTECTOR" rank_num="2" visible_intruder_min_rank="2"><modifiers><add name="PVP_DEFEND_RATIO" value="40" bonus="true"/></modifiers></rank>
	<rank type="PROTECTOR" rank_num="3" visible_intruder_min_rank="1"><modifiers><add name="PVP_DEFEND_RATIO" value="60" bonus="true"/></modifiers></rank>
</conqueror_protector_ranks>)xml";

/** Exposes the protected static KnownList::addPair */
struct Pairing : world::knownlist::KnownList {
	static void pair(model::gameobjects::VisibleObject& a, model::gameobjects::VisibleObject& b) { addPair(a, b); }
};

class ConquerorProtectorTest : public TeamTest {
protected:
	void SetUp() override {
		TeamTest::SetUp();
		dataholders::DataManager::CONQUEROR_AND_PROTECTOR_DATA.publish(
			xml::bindString<dataholders::ConquerorAndProtectorData>(context, std::string(RANKS_XML)));
		configs::main::CustomConfig::CONQUEROR_AND_PROTECTOR_WORLDS.set(std::unordered_set<int32_t>{210010000});
		ConquerorAndProtectorService::getInstance().init(); // the process's single init: ctest runs every case in its own process
	}

	void TearDown() override {
		for (Member& m : members) {
			mapInstance->removeObject(*m.f.player);
			world::World::getInstance().removeObject(*m.f.player);
		}
		TeamTest::TearDown();
		dataholders::DataManager::CONQUEROR_AND_PROTECTOR_DATA.resetForTests();
		configs::main::CustomConfig::CONQUEROR_AND_PROTECTOR_WORLDS.set({});
	}

	/** a member of the race on the map instance (World-wide lookups and map broadcasts find him) */
	Member& at(std::string_view name, model::Race race, float x) {
		Member& m = addMember(name, x);
		m.f.commonData->setRace(race);
		world::World::getInstance().storeObject(*m.f.player);
		mapInstance->addObject(*m.f.player);
		return m;
	}

	int32_t pvpAttack(Member& m) { return m.player().getGameStats()->getStat(StatEnum::PVP_ATTACK_RATIO, 0)->getCurrent(); }

	ConquerorAndProtectorService& cp = ConquerorAndProtectorService::getInstance();
	xml::LoadContext context;
	ConfigScope<bool> enabled{configs::main::CustomConfig::CONQUEROR_AND_PROTECTOR_SYSTEM_ENABLED, true};
	ConfigScope<int32_t> levelDiff{configs::main::CustomConfig::CONQUEROR_AND_PROTECTOR_LEVEL_DIFF, 5};
	ConfigScope<int32_t> interval{configs::main::CustomConfig::CONQUEROR_AND_PROTECTOR_KILLS_DECREASE_INTERVAL, 1};
	ConfigScope<int32_t> decrease{configs::main::CustomConfig::CONQUEROR_AND_PROTECTOR_KILLS_DECREASE_COUNT, 1};
	ConfigScope<int32_t> rank1{configs::main::CustomConfig::CONQUEROR_AND_PROTECTOR_KILLS_RANK1, 1};
	ConfigScope<int32_t> rank2{configs::main::CustomConfig::CONQUEROR_AND_PROTECTOR_KILLS_RANK2, 3};
	ConfigScope<int32_t> rank3{configs::main::CustomConfig::CONQUEROR_AND_PROTECTOR_KILLS_RANK3, 5};
};

/**
 * ConquerorAndProtectorService.java:146-163, :172-196, CPBuff.java:14-27: an Asmodian's kill on the Elyos map makes him a rank-1 conqueror
 * (one kill): the buff of rank 1 (PVP_ATTACK_RATIO 10), SM_CONQUEROR_PROTECTOR(1, 1) to him and (6, him) to those who see him. Three kills are
 * rank 2 (20), five rank 3 (30), and more stay at five
 */
TEST_F(ConquerorProtectorTest, KillsRaiseTheConquerorRankAndItsBuff) {
	Member& killer = at("Killer", model::Race::ASMODIANS, 100.0f);
	Member& victim = at("Victim", model::Race::ELYOS, 101.0f);
	Member& seer = at("Seer", model::Race::ELYOS, 102.0f);
	Pairing::pair(killer.player(), seer.player());
	clearAll();

	cp.onKill(killer.player(), victim.player());
	runtime::Ptr<services::conquerorAndProtectorSystem::CPInfo> info = cp.getCPInfoForCurrentMap(killer.player());
	ASSERT_NE(info, nullptr);
	EXPECT_EQ(info->getType(), model::templates::cp::CPType::CONQUEROR);
	EXPECT_EQ(info->getVictims(), 1);
	EXPECT_EQ(info->getRank(), 1);
	EXPECT_EQ(pvpAttack(killer), 10);
	EXPECT_EQ(killer.count(SM_CONQUEROR_PROTECTOR(1, 1)), 1);
	EXPECT_EQ(seer.count(SM_CONQUEROR_PROTECTOR(6, killer.player())), 1);

	cp.onKill(killer.player(), victim.player());
	EXPECT_EQ(info->getRank(), 1) << "two kills";
	cp.onKill(killer.player(), victim.player());
	EXPECT_EQ(info->getRank(), 2);
	EXPECT_EQ(pvpAttack(killer), 20) << "the rank-1 buff ended first";
	for (int i = 0; i < 4; ++i)
		cp.onKill(killer.player(), victim.player());
	EXPECT_EQ(info->getVictims(), 5) << "capped at the rank-3 kills";
	EXPECT_EQ(info->getRank(), 3);
	EXPECT_EQ(pvpAttack(killer), 30);
}

/** ConquerorAndProtectorService.java:150-151: a killer more than LEVEL_DIFF levels above his victim counts nothing; nor a kill elsewhere */
TEST_F(ConquerorProtectorTest, AKillOfAMuchLowerVictimCountsNothing) {
	Member& killer = at("Killer", model::Race::ASMODIANS, 100.0f);
	Member& victim = at("Victim", model::Race::ELYOS, 101.0f);
	killer.f.commonData->setLevel(12);
	cp.onKill(killer.player(), victim.player());
	EXPECT_EQ(cp.getCPInfoForCurrentMap(killer.player()), nullptr) << "11 levels above (victim level 1)";
	killer.f.commonData->setLevel(6);
	cp.onKill(killer.player(), victim.player());
	EXPECT_NE(cp.getCPInfoForCurrentMap(killer.player()), nullptr) << "5 levels above";
}

/** ConquerorAndProtectorService.java:155-160: the death of a rank-3 conqueror is announced on the map (to everybody but him) */
TEST_F(ConquerorProtectorTest, TheDeathOfARankThreeConquerorIsAnnounced) {
	Member& conqueror = at("Slayer", model::Race::ASMODIANS, 100.0f);
	Member& protector = at("Guard", model::Race::ELYOS, 101.0f);
	Member& victim = at("Victim", model::Race::ELYOS, 102.0f);
	for (int i = 0; i < 5; ++i)
		cp.onKill(conqueror.player(), victim.player());
	clearAll();
	cp.onKill(protector.player(), conqueror.player());
	EXPECT_EQ(victim.count(SM_SYSTEM_MESSAGE::STR_MSG_SLAYER_DARK_DEATH_TO_B("Guard", "Slayer")), 1) << "an Elyos killer: DARK";
	EXPECT_EQ(conqueror.count(SM_SYSTEM_MESSAGE::STR_MSG_SLAYER_DARK_DEATH_TO_B("Guard", "Slayer")), 0) << "not the dead conqueror";
	runtime::Ptr<services::conquerorAndProtectorSystem::CPInfo> guard = cp.getCPInfoForCurrentMap(protector.player());
	ASSERT_NE(guard, nullptr);
	EXPECT_EQ(guard->getType(), model::templates::cp::CPType::PROTECTOR);
	EXPECT_EQ(guard->getRank(), 1);
}

/** ConquerorAndProtectorService.java:58-65 (init's task), :172-185: every interval the kills decrease; at rank 0 the info is removed */
TEST_F(ConquerorProtectorTest, TheKillsDecreaseEveryInterval) {
	Member& killer = at("Killer", model::Race::ASMODIANS, 100.0f);
	Member& victim = at("Victim", model::Race::ELYOS, 101.0f);
	cp.onKill(killer.player(), victim.player());
	cp.onKill(killer.player(), victim.player());
	cp.onKill(killer.player(), victim.player());
	ASSERT_EQ(cp.getCPInfoForCurrentMap(killer.player())->getRank(), 2);
	executor->advance(std::chrono::minutes(1));
	EXPECT_EQ(cp.getCPInfoForCurrentMap(killer.player())->getVictims(), 2);
	EXPECT_EQ(cp.getCPInfoForCurrentMap(killer.player())->getRank(), 1);
	EXPECT_EQ(pvpAttack(killer), 10);
	executor->advance(std::chrono::minutes(2));
	EXPECT_EQ(cp.getCPInfoForCurrentMap(killer.player()), nullptr) << "rank 0: removed";
	EXPECT_EQ(pvpAttack(killer), 0) << "the buff ended";
}

/**
 * ConquerorAndProtectorService.java:198-242: a protector's scan lists the conquerors his rank sees within 500 m (a rank-1 protector sees rank 3
 * and below: visible_intruder_min_rank 3), then a 180 s cooldown; a conqueror cannot scan
 */
TEST_F(ConquerorProtectorTest, AProtectorScansForIntruders) {
	Member& guard = at("Guard", model::Race::ELYOS, 100.0f);
	Member& intruder = at("Intruder", model::Race::ASMODIANS, 300.0f);
	Member& victim = at("Victim", model::Race::ELYOS, 101.0f);
	cp.onKill(intruder.player(), victim.player()); // a rank-1 conqueror
	cp.onKill(guard.player(), intruder.player());  // a rank-1 protector
	clearAll();

	cp.intruderScan(intruder.player());
	EXPECT_EQ(intruder.count(opcodeOf<SM_CONQUEROR_PROTECTOR>), 0) << "a conqueror does not scan";

	cp.intruderScan(guard.player());
	EXPECT_EQ(guard.count(SM_CONQUEROR_PROTECTOR(std::vector<runtime::Ptr<model::gameobjects::player::Player>>{runtime::Ptr<model::gameobjects::player::Player>(intruder.player())}, true)), 1);
	cp.intruderScan(guard.player());
	EXPECT_EQ(guard.count(opcodeOf<SM_CONQUEROR_PROTECTOR>), 1) << "the cooldown";
	cp.sendDetectCooldown(guard.player());
	EXPECT_EQ(guard.count(opcodeOf<SM_CONQUEROR_PROTECTOR>), 2) << "sendDetectCooldown";
}

/** CM_SHOW_MAP.java:30-43: action 0 is the intruder scan, 1 nothing, any other a warning */
TEST_F(ConquerorProtectorTest, TheShowMapPacketScans) {
	Member& guard = at("Guard", model::Race::ELYOS, 100.0f);
	Member& intruder = at("Intruder", model::Race::ASMODIANS, 300.0f);
	Member& victim = at("Victim", model::Race::ELYOS, 101.0f);
	cp.onKill(intruder.player(), victim.player());
	cp.onKill(guard.player(), intruder.player());
	clearAll();
	network::test::LogCapture capture({"com.aionemu.gameserver.network.aion.clientpackets.CM_SHOW_MAP"});
	auto run = [&guard](int8_t action) {
		Driver<CM_SHOW_MAP> packet(196); // ClientPacketInfo.gen.inc: C_REQUEST_SERIAL_KILLER_LIST
		packet.readAndRun(PacketWriter().C(action).data, guard.client->get());
	};
	run(1);
	EXPECT_EQ(guard.count(opcodeOf<SM_CONQUEROR_PROTECTOR>), 0);
	run(5);
	EXPECT_EQ(capture.count("sent unknown show map action type: 5"), 1) << capture.dump();
	run(0);
	EXPECT_EQ(guard.count(opcodeOf<SM_CONQUEROR_PROTECTOR>), 1);
}

} // namespace
} // namespace aion::gameserver::network::aion::clientpackets::testing::team
