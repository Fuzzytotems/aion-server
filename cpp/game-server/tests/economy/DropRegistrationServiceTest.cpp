// DropRegistrationService (P5-09, m5b3-plan.md L-01, L-05): registerDrop and the global-rule evaluator, against the rows of the shipped data
// (DropTestSupport.h). This file replaces the M5b-1 cases of the AION_PARTIAL that stood in for registerDrop (m5b-plan.md D5, O-05): the body
// is ported, so the partial's reason "npc drops are not registered yet (M5b-3)" is gone with its site, and the first cases below pin what the
// kill does instead - the DropNpc with its looter, the drop set, SM_LOOT_STATUS(LOOT_ENABLE) (DropRegistrationService.java:59-109).
//
// Expectations come from the Java source and the data, never from the port: the entry of each rule for the gate's two monsters is the
// oracle's (`python tools/oracle/oracle.py m5b3-drops --npc 210663|210133 --drop-rate 1000000` lists Buff Food, Power Shards and
// JUNK_SPAKY_MATERIAL, and Kinah, Buff Food, Power Shards and JUNK_CHERUBIM1_MATERIAL among their applicable rules - the fixture holds those
// rules and three that do not apply), the kinah counts are the Java expression evaluated offline (see the case), every chance is the Java
// float expression of the cited line.
//
// "A forced rate" is gameserver.rates.drop = 1000000, the M5b-3 gate's lever (m5b3-plan.md D3): every applicable rule fires, so the entries,
// their indexes and the rule each belongs to are exact; only the pick inside a rule and the counts are random.

#include "DropTestSupport.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <set>
#include <string>
#include <vector>

#include "aion/commons/utils/Rnd.h"
#include "aion/gameserver/model/drop/Drop.h"
#include "aion/gameserver/model/gameobjects/player/PlayerCommonData.h"
#include "aion/gameserver/model/templates/globaldrops/GlobalDropItem.h"
#include "aion/gameserver/network/aion/serverpackets/SM_LOOT_STATUS.h"
#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/services/event/EventService.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::economy::test {
namespace {

using model::drop::DropItem;
using model::gameobjects::Npc;
using model::gameobjects::player::Player;
using runtime::Ptr;
using services::drop::DropRegistrationService;

constexpr int32_t SM_LOOT_STATUS_OPCODE = 205; // ServerPacketsOpcodes.java:223
constexpr float FORCED_RATE = 1000000.0f;

/** SM_LOOT_STATUS.writeImpl (SM_LOOT_STATUS.java:39-43): writeD(target), writeC(status), writeD(lootEffectId); LOOT_ENABLE is status 0 */
std::vector<uint8_t> lootEnable(int32_t target, int32_t lootEffectId) {
	return network::test::PacketWriter().D(target).C(0).D(lootEffectId).data;
}

class DropRegistrationServiceTest : public DropTest {
protected:
	DropRegistrationService& service = DropRegistrationService::getInstance();
};

// ------------------------------------------------------------------------------------------------------------------- registerDrop, the kill

// DropRegistrationService.java:59-109 for a solo killer at gameserver.rates.drop = 0 (the M5b and M5b-2 gates' profile, m5b3-plan.md D4): the
// drop set is registered empty, initDropNpc (:122-165) makes the killer the only allowed looter (:159-163), and SM_LOOT_STATUS(LOOT_ENABLE) goes to
// him (:104-106) with lootEffectId 0 - the packet the rewritten M5b R3 asserts once per kill.
TEST_F(DropRegistrationServiceTest, AKillRegistersTheSoloKillerAsTheOnlyLooterAndSendsHimLootEnable) {
	DROP_REQUIRE_DATABASE();
	setDropRate(0.0f);
	Player& killer = newPlayer(700001, "Killer");
	Player& bystander = newPlayer(700002, "Bystander");
	Npc& npc = spawnNpc(JUVENILE_SPARKIE);

	service.registerDrop(npc, killer, killer.getLevel(), {});

	Ptr<model::gameobjects::DropNpc> dropNpc = service.getDropRegistrationMap().get(npc.getObjectId());
	ASSERT_TRUE(dropNpc) << "initDropNpc registers the DropNpc (:163)";
	EXPECT_EQ(dropNpc->getAllowedLooters()->snapshot(), std::vector<int32_t>{killer.getObjectId()}) << "solo: the killer alone (:159-162)";
	EXPECT_TRUE(dropNpc->isAllowedToLoot(killer));
	EXPECT_FALSE(dropNpc->isAllowedToLoot(bystander));
	Ptr<runtime::RcHashSet<runtime::Ref<DropItem>>> dropItems = service.getCurrentDropMap().get(npc.getObjectId());
	ASSERT_TRUE(dropItems) << "the drop set is registered (:82) even when nothing dropped";
	EXPECT_TRUE(dropItems->isEmpty()) << "rate 0: no rule fires (calculateDropChance multiplies every chance by it)";

	std::vector<SentPacket> sent = takeSent(killer);
	ASSERT_EQ(sent.size(), 1u) << "exactly the LOOT_ENABLE of :104-106";
	EXPECT_EQ(sent[0].opcode, SM_LOOT_STATUS_OPCODE);
	EXPECT_EQ(sent[0].body, lootEnable(npc.getObjectId(), 0));
}

// DropRegistrationService.java:52-54: the overload AIActions.registerDrop calls passes the player's own level as the highest level
TEST_F(DropRegistrationServiceTest, TheOverloadWithoutALevelRegistersTheSameDrop) {
	DROP_REQUIRE_DATABASE();
	setDropRate(0.0f);
	Player& killer = newPlayer(700001, "Killer");
	Npc& npc = spawnNpc(JUVENILE_SPARKIE);

	service.registerDrop(npc, killer, {});

	ASSERT_TRUE(service.getDropRegistrationMap().get(npc.getObjectId()));
	EXPECT_TRUE(service.getDropRegistrationMap().get(npc.getObjectId())->isAllowedToLoot(killer));
	std::vector<SentPacket> sent = takeSent(killer);
	ASSERT_EQ(sent.size(), 1u);
	EXPECT_EQ(sent[0].body, lootEnable(npc.getObjectId(), 0));
}

// The gate's monster at a forced rate. Of the fixture's seven rules, three apply to a level-2 BEAST of group SPAKY rated NORMAL in Poeta
// (drop type ELYSEA) for a level-1 Elyos (the oracle lists the same three among its ten): Buff Food (the two level-10 candidates, diff -8 in
// [-9, 0]), Power Shards (only the level-1 Minor Power Shard, diff 1 in [0, 9]) and JUNK_SPAKY_MATERIAL (only the level-5 fragment, diff -3 in
// [-4, 5]). Kinah has no BEAST in gd_races, Omega wants a Balaurea world, JUNK_CHERUBIM1 another group, Kahruns Symbol two other npcs.
// Each rule adds one entry (max_drop_rule 1) at the next index (addDropItems :251-254, regDropItem :260-267): no looter restriction for a
// solo kill (winnerObj 0), the npc's object id, a Drop of chance 100.
TEST_F(DropRegistrationServiceTest, AForcedRateGivesTheJuvenileSparkieOneEntryPerApplicableRuleInRuleOrder) {
	DROP_REQUIRE_DATABASE();
	setDropRate(FORCED_RATE);
	Player& killer = newPlayer(700001, "Killer");
	Npc& npc = spawnNpc(JUVENILE_SPARKIE);

	service.registerDrop(npc, killer, killer.getLevel(), {});

	std::vector<Ptr<DropItem>> entries = entriesOf(npc);
	ASSERT_EQ(entries.size(), 3u);
	Ptr<DropItem> buffFood = entryAt(npc, 1);
	Ptr<DropItem> powerShards = entryAt(npc, 2);
	Ptr<DropItem> junk = entryAt(npc, 3);
	ASSERT_TRUE(buffFood && powerShards && junk) << "indexes 1, 2, 3 in rule order";
	EXPECT_TRUE(buffFood->getDropTemplate()->getItemId() == MINOR_RALLY_SERUM || buffFood->getDropTemplate()->getItemId() == MINOR_FOCUS_AGENT)
		<< buffFood->getDropTemplate()->getItemId();
	EXPECT_EQ(buffFood->getCount(), 1) << "no min_count/max_count: 1";
	EXPECT_EQ(powerShards->getDropTemplate()->getItemId(), MINOR_POWER_SHARD) << "min_diff 0: the level-10 shard is 8 levels above the npc";
	EXPECT_GE(powerShards->getCount(), 2);
	EXPECT_LE(powerShards->getCount(), 15);
	EXPECT_EQ(junk->getDropTemplate()->getItemId(), SPARKIE_CARAPACE_FRAGMENT);
	EXPECT_EQ(junk->getCount(), 1);
	for (const Ptr<DropItem>& entry : entries) {
		EXPECT_EQ(entry->getNpcObj(), npc.getObjectId());
		EXPECT_TRUE(entry->getPlayerObjIds().isEmpty()) << "regDropItem(index, winnerObj = 0, ...): setPlayerObjId ignores 0";
		EXPECT_EQ(entry->getDropTemplate()->getChance(), 100.0f) << "new Drop(itemId, 1, 1, 100)";
	}
}

// The kinah monster: four rules apply to a level-1 MAGICALMONSTER of group CHERUBIM (the oracle lists them among its ten): Kinah (5-25 x 1 x
// 1.0^6), Buff Food, Power Shards and JUNK_CHERUBIM1_MATERIAL's level-5 fragment (diff -4, the edge of [-4, 5]).
TEST_F(DropRegistrationServiceTest, AForcedRateGivesTheStripedKerubKinahAndItsJunk) {
	DROP_REQUIRE_DATABASE();
	setDropRate(FORCED_RATE);
	Player& killer = newPlayer(700001, "Killer");
	Npc& npc = spawnNpc(STRIPED_KERUB);

	service.registerDrop(npc, killer, killer.getLevel(), {});

	ASSERT_EQ(entriesOf(npc).size(), 4u);
	Ptr<DropItem> kinah = entryAt(npc, 1);
	Ptr<DropItem> buffFood = entryAt(npc, 2);
	Ptr<DropItem> powerShards = entryAt(npc, 3);
	Ptr<DropItem> junk = entryAt(npc, 4);
	ASSERT_TRUE(kinah && buffFood && powerShards && junk) << "indexes 1, 2, 3, 4 in rule order";
	EXPECT_EQ(kinah->getDropTemplate()->getItemId(), KINAH);
	EXPECT_GE(kinah->getCount(), 5);
	EXPECT_LE(kinah->getCount(), 25);
	EXPECT_TRUE(buffFood->getDropTemplate()->getItemId() == MINOR_RALLY_SERUM || buffFood->getDropTemplate()->getItemId() == MINOR_FOCUS_AGENT);
	EXPECT_EQ(powerShards->getDropTemplate()->getItemId(), MINOR_POWER_SHARD);
	EXPECT_EQ(junk->getDropTemplate()->getItemId(), KERUB_SCALE_FRAGMENT);
}

// getItemCount (DropRegistrationService.java:447-453): `long count = Rnd.get(min, max); if kinah: count *= npc.getLevel() * Math.pow(rank * rating,
// 6)` - the rank and rating modifiers multiplied in float (getRankModifier/getRatingModifier return float), widened to double for pow, the
// right-hand side (level * pow) evaluated first and the compound assignment narrowing count * it back to long (JLS 15.26.2).
// The golden counts below are that expression for every roll 5..25, evaluated offline in double with the float product (and checked against
// the exact rational value of the float product: the nearest integer is at least 0.0019 away, far beyond any pow rounding). The last three
// npcs are shipped npcs whose counts only Java's arithmetic gives: a product of the double constants (1.15 * 1.8), a product of the widened
// floats in double, and a factor kept in float each change at least one of their counts by 1 (found by evaluating those variants offline for
// every shipped npc template the Kinah rule applies to; none changes a count of the first three npcs).
// The roll is the one the npc's kill draws: Kinah is the fixture's first rule, so registerDrop's first two draws on this thread are the rule's
// Rnd.chance() (:189) and getItemCount's Rnd.get(5, 25) (:449); the case draws them itself from a seed first, and kills one npc per seed until
// every roll 5..25 has been checked for every npc.
TEST_F(DropRegistrationServiceTest, KinahIsTheRollTimesLevelTimesRankTimesRatingToTheSixth) {
	DROP_REQUIRE_DATABASE();
	setDropRate(FORCED_RATE);
	Player& killer = newPlayer(700001, "Killer");
	// 1.1f * 2.0f = 2.2000000476837158, ^6 = 113.37991874466022, x level 40
	const std::array<int64_t, 21> saendukal{22675, 27211, 31746, 36281, 40816, 45351, 49887, 54422, 58957, 63492, 68027, 72563, 77098, 81633,
		86168, 90703, 95239, 99774, 104309, 108844, 113379};
	// 1.1f * 1.8f = 1.9800000190734863, ^6 = 60.25473304429366, x level 65
	const std::array<int64_t, 21> wreckhelm{19582, 23499, 27415, 31332, 35249, 39165, 43082, 46998, 50915, 54831, 58748, 62664, 66581, 70498,
		74414, 78331, 82247, 86164, 90080, 93997, 97913};
	// 1.15f * 1.8f = 2.069999933242798, ^6 = 78.67232566302019, x level 47: rolls 5, 10, 15, 20 and 25 give x.9965, which the double constants
	// (1.15 * 1.8 = 2.07) carry past the integer
	const std::array<int64_t, 21> yatri{18487, 22185, 25883, 29580, 33278, 36975, 40673, 44371, 48068, 51766, 55463, 59161, 62859, 66556, 70254,
		73951, 77649, 81347, 85044, 88742, 92439};
	// 1.2f * 1.8f = 2.1600000858306885, ^6 = 101.55998088219897, x level 58: roll 23 gives 135481.0145; the widened floats multiplied in double
	// (2.16000002861...) and the double constants (2.16) both give 135480.99..
	const std::array<int64_t, 21> jebal{29452, 35342, 41233, 47123, 53014, 58904, 64795, 70685, 76576, 82466, 88357, 94247, 100138, 106028,
		111919, 117809, 123700, 129590, 135481, 141371, 147261};
	// 1.1f * 1.8f again, x level 44: roll 24 gives 63628.998, which a factor and a product kept in float round to 63629
	const std::array<int64_t, 21> ashutang{13256, 15907, 18558, 21209, 23860, 26512, 29163, 31814, 34465, 37116, 39768, 42419, 45070, 47721,
		50372, 53024, 55675, 58326, 60977, 63628, 66280};
	struct Case {
		int32_t npcId;
		const std::array<int64_t, 21>* golden; // null: level 1 x (1.0 x 1.0)^6, the roll itself
	};
	const Case cases[] = {{STRIPED_KERUB, nullptr}, {SAENDUKAL, &saendukal}, {DATURUM_WRECKHELM, &wreckhelm}, {HIGH_PRIEST_YATRI, &yatri},
		{JEBAL, &jebal}, {COMMANDER_ASHUTANG, &ashutang}};
	for (const Case& c : cases) {
		std::set<int32_t> checkedRolls;
		for (uint64_t seed = 1; checkedRolls.size() < 21 && seed <= 2000; seed++) {
			commons::utils::Rnd::seedCurrentThreadForTests(seed);
			commons::utils::Rnd::chance(); // the Kinah rule's roll
			int32_t roll = commons::utils::Rnd::get(5, 25);
			if (!checkedRolls.insert(roll).second)
				continue;
			Npc& npc = spawnNpc(c.npcId);
			commons::utils::Rnd::seedCurrentThreadForTests(seed);

			service.registerDrop(npc, killer, killer.getLevel(), {});

			Ptr<DropItem> kinah = entryAt(npc, 1);
			ASSERT_TRUE(kinah) << c.npcId;
			ASSERT_EQ(kinah->getDropTemplate()->getItemId(), KINAH) << c.npcId;
			int64_t expected = c.golden == nullptr ? roll : (*c.golden)[static_cast<size_t>(roll - 5)];
			EXPECT_EQ(kinah->getCount(), expected) << "npc " << c.npcId << ", roll " << roll;
		}
		EXPECT_EQ(checkedRolls.size(), 21u) << "npc " << c.npcId << ": every roll 5..25";
	}
}

// isAllowedDefaultGlobalDropNpc (:167-181): a level-1 npc counts as one "with missing stats" and gets no default global drop - except in Poeta
// and Ishalgen, whose monsters are level 1 (:174-176: the kinah monster itself); a chest never, an npc with an abyss type other than DEFENDER
// never (:177-179), a DEFENDER (castle defense special named, placed in Poeta past the level test) still gets them. (The siege and base spawn
// arms need their spawn template classes; the start maps have neither.)
TEST_F(DropRegistrationServiceTest, OnlyPoetaAndIshalgenKeepTheDefaultGlobalDropsOfALevelOneNpc) {
	EXPECT_TRUE(service.isAllowedDefaultGlobalDropNpc(spawnNpc(STRIPED_KERUB, POETA), false));
	EXPECT_TRUE(service.isAllowedDefaultGlobalDropNpc(spawnNpc(STRIPED_KERUB, ISHALGEN), false));
	EXPECT_FALSE(service.isAllowedDefaultGlobalDropNpc(spawnNpc(STRIPED_KERUB, ELTNEN), false)) << "level 1 outside the two start maps";
	EXPECT_TRUE(service.isAllowedDefaultGlobalDropNpc(spawnNpc(JUVENILE_SPARKIE, ELTNEN), false)) << "level 2 anywhere";
	EXPECT_FALSE(service.isAllowedDefaultGlobalDropNpc(spawnNpc(JUVENILE_SPARKIE, POETA), true)) << "a chest, whatever its level or map";
	EXPECT_FALSE(service.isAllowedDefaultGlobalDropNpc(spawnNpc(CASTLE_GUARD, POETA), false)) << "abyss_type GUARD";
	EXPECT_TRUE(service.isAllowedDefaultGlobalDropNpc(spawnNpc(CASTLE_DEFENSE_NAMED, POETA), false)) << "abyss_type DEFENDER (:178)";
}

// The same through registerDrop at a forced rate: the kinah monster placed in Eltnen drops nothing - only rules with gd_npcs are evaluated for a
// disallowed npc (:186-187), and Kahruns Symbol's two npcs are others
TEST_F(DropRegistrationServiceTest, ALevelOneNpcOutsidePoetaAndIshalgenDropsNothing) {
	DROP_REQUIRE_DATABASE();
	setDropRate(FORCED_RATE);
	Player& killer = newPlayer(700001, "Killer");
	Npc& inEltnen = spawnNpc(STRIPED_KERUB, ELTNEN);
	Npc& inIshalgen = spawnNpc(STRIPED_KERUB, ISHALGEN);

	service.registerDrop(inEltnen, killer, killer.getLevel(), {});
	service.registerDrop(inIshalgen, killer, killer.getLevel(), {});

	EXPECT_TRUE(entriesOf(inEltnen).empty());
	EXPECT_EQ(entriesOf(inIshalgen).size(), 4u) << "Ishalgen keeps them (drop type ASMODAE: no fixture rule asks the world)";
}

// hasGlobalNpcExclusions (:287-296) over the shipped global_npc_exclusions.xml, one npc per list that only that list excludes: by npc id (elroco,
// 210338, is listed; its tribe MINX_HZAIF and type are not), by npc type (farm worker: type GENERAL, tribe FARMER_HKERUBIM_LF1), by tribe (siege
// weapon: tribe PET, type ABYSS_GUARD), by abyss type (aetheric field engineer: SHIELDNPC_ON, tribe GUARD, type ABYSS_GUARD); latri is GENERAL
// in both the type and the tribe list. The gate's monster, and jebal (whose name the next case lists), are in none.
TEST_F(DropRegistrationServiceTest, TheShippedGlobalNpcExclusionsMatchByIdTypeTribeAndAbyssType) {
	EXPECT_TRUE(service.hasGlobalNpcExclusions(spawnNpc(ELROCO))) << "npc_ids";
	EXPECT_TRUE(service.hasGlobalNpcExclusions(spawnNpc(FARM_WORKER))) << "npc_types GENERAL";
	EXPECT_TRUE(service.hasGlobalNpcExclusions(spawnNpc(SIEGE_WEAPON))) << "npc_tribes PET";
	EXPECT_TRUE(service.hasGlobalNpcExclusions(spawnNpc(AETHERIC_FIELD_ENGINEER))) << "npc_abyss_types SHIELDNPC_ON";
	EXPECT_TRUE(service.hasGlobalNpcExclusions(spawnNpc(LATRI)));
	EXPECT_FALSE(service.hasGlobalNpcExclusions(spawnNpc(JUVENILE_SPARKIE)));
	EXPECT_FALSE(service.hasGlobalNpcExclusions(spawnNpc(SAENDUKAL)));
	EXPECT_FALSE(service.hasGlobalNpcExclusions(spawnNpc(JEBAL)));
}

// hasGlobalNpcExclusions' name arm (:290, `getNpcNames().contains(npc.getName())`): the shipped file has no <npc_names> (global_npc_exclusions.xml:
// 2-10), so a fixture list stands in for it - the shipped lists plus <npc_names>jebal</npc_names> (the element global_npc_exclusions.xsd:9
// declares) - published for this case only
TEST_F(DropRegistrationServiceTest, AFixtureNameListExcludesAnNpcByItsName) {
	struct FixtureExclusions {
		std::deque<xml::LoadContext> contexts;
		void publish(const std::string& xml) {
			dataholders::DataManager::GLOBAL_EXCLUSION_DATA.resetForTests();
			dataholders::DataManager::GLOBAL_EXCLUSION_DATA.publish(xml::bindString<dataholders::GlobalNpcExclusionData>(contexts.emplace_back(), xml));
		}
		~FixtureExclusions() { publish(GLOBAL_NPC_EXCLUSIONS_XML); }
	} exclusions;
	std::string withNames(GLOBAL_NPC_EXCLUSIONS_XML);
	withNames.insert(withNames.find("<npc_types>"), "<npc_names>jebal</npc_names>");
	Npc& jebal = spawnNpc(JEBAL);
	ASSERT_FALSE(service.hasGlobalNpcExclusions(jebal)) << "the shipped lists";

	exclusions.publish(withNames);

	EXPECT_TRUE(service.hasGlobalNpcExclusions(jebal)) << "npc_names jebal";
	EXPECT_FALSE(service.hasGlobalNpcExclusions(spawnNpc(JUVENILE_SPARKIE))) << "another name";
}

// :92-95: an excluded npc gets no rule of the data at all, even at a forced rate - elroco would otherwise get Buff Food (level 1, the level-10
// candidates are 9 levels above it)
TEST_F(DropRegistrationServiceTest, AnExcludedNpcDropsNothingFromTheData) {
	DROP_REQUIRE_DATABASE();
	setDropRate(FORCED_RATE);
	Player& killer = newPlayer(700001, "Killer");
	Npc& npc = spawnNpc(ELROCO);

	service.registerDrop(npc, killer, killer.getLevel(), {});

	EXPECT_TRUE(entriesOf(npc).empty());
	EXPECT_TRUE(service.getDropRegistrationMap().get(npc.getObjectId())) << "the drop is still registered, empty";
}

// :91-92: "instances with WorldDropType.NONE must not have global drops" - the gate's monster placed in Sanctum (drop_type NONE) drops nothing
TEST_F(DropRegistrationServiceTest, AMapOfDropTypeNoneGivesAnEmptyDrop) {
	DROP_REQUIRE_DATABASE();
	setDropRate(FORCED_RATE);
	Player& killer = newPlayer(700001, "Killer");
	Npc& npc = spawnNpc(JUVENILE_SPARKIE, SANCTUM);

	service.registerDrop(npc, killer, killer.getLevel(), {});

	EXPECT_TRUE(entriesOf(npc).empty());
	std::vector<SentPacket> sent = takeSent(killer);
	ASSERT_EQ(sent.size(), 1u);
	EXPECT_EQ(sent[0].body, lootEnable(npc.getObjectId(), 0)) << "the corpse is lootable (empty) all the same";
}

// The event pass (:96-98): after the data's rules, the rules of the active events (EventService.getActiveEventDropRules) are evaluated the same
// way, numbering on from the index the data's rules reached. The shipped Easter event's rule (NORMAL, ELITE, HERO, LEGENDARY npcs, a level-1
// egg) stands in the active list, as Event.start would put it there (Event::start is AION_UNPORTED; the scenario profiles disable every event,
// m5b3-plan.md D3). At a forced rate: the sparkie gets its three entries and the egg as the fourth; the sparkie placed in Sanctum gets the egg
// alone, at index 1 - drop type NONE skips only the data's rules (:92); elroco gets nothing - an excluded npc that is no chest skips both (:96).
TEST_F(DropRegistrationServiceTest, AnActiveEventRuleIsEvaluatedAfterTheDataRules) {
	DROP_REQUIRE_DATABASE();
	setDropRate(FORCED_RATE);
	Player& killer = newPlayer(700001, "Killer");
	ASSERT_EQ(eventRules().getAllRules().size(), 1u);
	struct ActiveEventRule {
		explicit ActiveEventRule(const model::templates::globaldrops::GlobalRule& rule) {
			services::event::EventService::getInstance().getActiveEventDropRules()->add(&rule);
		}
		~ActiveEventRule() { services::event::EventService::getInstance().getActiveEventDropRules()->clear(); }
	} easter(eventRules().getAllRules().front());
	Npc& sparkie = spawnNpc(JUVENILE_SPARKIE);
	Npc& inSanctum = spawnNpc(JUVENILE_SPARKIE, SANCTUM);
	Npc& elroco = spawnNpc(ELROCO);

	service.registerDrop(sparkie, killer, killer.getLevel(), {});
	service.registerDrop(inSanctum, killer, killer.getLevel(), {});
	service.registerDrop(elroco, killer, killer.getLevel(), {});

	EXPECT_EQ(entriesOf(sparkie).size(), 4u);
	ASSERT_TRUE(entryAt(sparkie, 4)) << "after the data's entries 1-3";
	EXPECT_EQ(entryAt(sparkie, 4)->getDropTemplate()->getItemId(), MOTTLED_EGG);
	EXPECT_EQ(entryAt(sparkie, 4)->getCount(), 1);
	ASSERT_EQ(entriesOf(inSanctum).size(), 1u);
	ASSERT_TRUE(entryAt(inSanctum, 1));
	EXPECT_EQ(entryAt(inSanctum, 1)->getDropTemplate()->getItemId(), MOTTLED_EGG);
	EXPECT_TRUE(entriesOf(elroco).empty());
}

// SM_LOOT_STATUS.getLootEffect (SM_LOOT_STATUS.java:33-36) reads the drop set registerDrop put into currentDropMap (:82) before it sends
// LOOT_ENABLE: grand chieftain saendukal (LEGENDARY) in Inggison (drop type BALAUREA) gets "Omega Enchantment Stone"'s one candidate at a forced
// rate, and DropItem.getLootEffectId answers 1003 for it (DropItem.java:204-205) - the glow of the corpse. (The gate sees a listed godstone in
// 41.5 % of kills only, m5b3-plan.md Y1: this is the case that pins the value.)
TEST_F(DropRegistrationServiceTest, LootEnableCarriesTheLootEffectOfAnOmegaStoneInTheDrop) {
	DROP_REQUIRE_DATABASE();
	setDropRate(FORCED_RATE);
	Player& killer = newPlayer(700001, "Killer");
	Npc& npc = spawnNpc(SAENDUKAL, INGGISON);

	service.registerDrop(npc, killer, killer.getLevel(), {});

	ASSERT_EQ(entriesOf(npc).size(), 3u) << "Kinah, Buff Food (level-40 candidates), Omega Enchantment Stone";
	ASSERT_TRUE(entryAt(npc, 3)) << "index 3";
	EXPECT_EQ(entryAt(npc, 3)->getDropTemplate()->getItemId(), OMEGA_ENCHANTMENT_STONE);
	std::vector<SentPacket> sent = takeSent(killer);
	ASSERT_EQ(sent.size(), 1u);
	EXPECT_EQ(sent[0].body, lootEnable(npc.getObjectId(), 1003));
}

// --------------------------------------------------------------------------------------------------------- the restrictions of a rule

// collectDrops -> collectAllowedDrops (:417-445) -> checkRuleRestrictions (:298-320): every restriction with the npc it lets through and one it
// keeps out, on the shipped rule that has it (the fixture rules only for gd_tribes and gd_excluded_npcs, which no shipped rule uses). A rule
// without the element passes that restriction (the rules of the other rows).
TEST_F(DropRegistrationServiceTest, EveryRestrictionOfARuleIsCheckedAgainstTheNpc) {
	using Ids = std::vector<int32_t>;
	const model::drop::DropModifiers elyos = plainModifiers(model::Race::ELYOS);
	auto collect = [this](std::string_view ruleName, Npc& npc, model::drop::DropModifiers modifiers) {
		const model::templates::globaldrops::GlobalRule* rule = ruleNamed(ruleName);
		EXPECT_NE(rule, nullptr) << ruleName;
		return rule == nullptr ? Ids{} : itemIdsOf(service.collectDrops(rule, npc, modifiers));
	};

	// gd_maps (checkGlobalRuleMaps :331-339): the npc's map id
	EXPECT_EQ(collect("Noble Draconute Armor", spawnNpc(JUVENILE_SPARKIE, DREDGION), elyos), Ids{NOBLE_DRACONUTE_ARMOR});
	EXPECT_EQ(collect("Noble Draconute Armor", spawnNpc(JUVENILE_SPARKIE, POETA), elyos), Ids{}) << "gd_maps: another map";
	// gd_worlds (checkGlobalRuleWorlds :341-349): the drop type of the npc's map (Cygnea BALAUREA_HIGH, Poeta ELYSEA)
	EXPECT_EQ(collect("Ceramium Fragment", spawnNpc(SAENDUKAL, CYGNEA), elyos), Ids{CERAMIUM_FRAGMENT});
	EXPECT_EQ(collect("Ceramium Fragment", spawnNpc(SAENDUKAL, POETA), elyos), Ids{}) << "gd_worlds: another drop type";
	// gd_ratings (checkGlobalRuleRatings :351-359): latri is rated JUNK, which "Omega Enchantment Stone" leaves out
	EXPECT_EQ(collect("Omega Enchantment Stone", spawnNpc(SAENDUKAL, INGGISON), elyos), Ids{OMEGA_ENCHANTMENT_STONE});
	EXPECT_EQ(collect("Omega Enchantment Stone", spawnNpc(LATRI, INGGISON), elyos), Ids{}) << "gd_ratings: JUNK";
	// gd_races (checkGlobalRuleRaces :361-369): the Kinah rule's list has no BEAST
	EXPECT_EQ(collect("Kinah", spawnNpc(STRIPED_KERUB), elyos), Ids{KINAH});
	EXPECT_EQ(collect("Kinah", spawnNpc(JUVENILE_SPARKIE), elyos), Ids{}) << "gd_races: BEAST";
	// gd_tribes (checkGlobalRuleTribes :371-379, fixture rule MONSTER)
	EXPECT_EQ(collect("Kahruns Symbol, fixture gd_tribes", spawnNpc(JUVENILE_SPARKIE), elyos), Ids{KAHRUNS_SYMBOL});
	EXPECT_EQ(collect("Kahruns Symbol, fixture gd_tribes", spawnNpc(SAENDUKAL), elyos), Ids{}) << "gd_tribes: KRALL";
	// gd_zones (checkGlobalRuleZones :381-389): an npc that is not spawned is inside no zone (Creature.isInsideZone), even on the rule's map
	EXPECT_EQ(collect("Junk Taloc's Root Fragment", spawnNpc(JUVENILE_SPARKIE, TALOCS_HOLLOW), elyos), Ids{}) << "gd_zones: outside both zones";
	// gd_npcs (checkGlobalRuleNpcs :391-399)
	EXPECT_EQ(collect("Kahruns Symbol", spawnNpc(DATURUM_WRECKHELM), elyos), Ids{KAHRUNS_SYMBOL});
	EXPECT_EQ(collect("Kahruns Symbol", spawnNpc(JUVENILE_SPARKIE), elyos), Ids{}) << "gd_npcs: another npc";
	// gd_npc_groups (checkGlobalRuleNpcGroups :401-409): the npc template's group_drop
	EXPECT_EQ(collect("JUNK_SPAKY_MATERIAL", spawnNpc(JUVENILE_SPARKIE), elyos), Ids{SPARKIE_CARAPACE_FRAGMENT});
	EXPECT_EQ(collect("JUNK_SPAKY_MATERIAL", spawnNpc(STRIPED_KERUB), elyos), Ids{}) << "gd_npc_groups: CHERUBIM";
	// gd_excluded_npcs (checkGlobalRuleExcludedNpcs :411-415, fixture rule 210663 218080)
	EXPECT_EQ(collect("Kahruns Symbol, fixture gd_excluded_npcs", spawnNpc(STRIPED_KERUB), elyos), Ids{KAHRUNS_SYMBOL});
	EXPECT_EQ(collect("Kahruns Symbol, fixture gd_excluded_npcs", spawnNpc(JUVENILE_SPARKIE), elyos), Ids{}) << "gd_excluded_npcs: listed";
	// restriction_race (checkRestrictionRace :322-329): the killer's race against the rule's, both arms (:324 and :325). The Asmodian twin of
	// "Bronze Coin" (rules_map_morheim.xml) has the same name, so it is found by its restriction; its coin is PC_ALL like the Elyos one, so only
	// the restriction keeps an Elyos killer from it
	EXPECT_EQ(collect("Bronze Coin", spawnNpc(JUVENILE_SPARKIE, ELTNEN), elyos), Ids{BRONZE_COIN});
	EXPECT_EQ(collect("Bronze Coin", spawnNpc(JUVENILE_SPARKIE, ELTNEN), plainModifiers(model::Race::ASMODIANS)), Ids{}) << "restriction_race ELYOS";
	const model::templates::globaldrops::GlobalRule* asmodianCoin = nullptr;
	for (const model::templates::globaldrops::GlobalRule& rule : restrictionRules().getAllRules())
		if (rule.getRuleName() == "Bronze Coin" && rule.getRestrictionRace() == model::templates::globaldrops::GlobalRule::RestrictionRace::ASMODIANS)
			asmodianCoin = &rule;
	ASSERT_NE(asmodianCoin, nullptr);
	model::drop::DropModifiers asmodianKiller = plainModifiers(model::Race::ASMODIANS);
	model::drop::DropModifiers elyosKiller = plainModifiers(model::Race::ELYOS);
	EXPECT_EQ(itemIdsOf(service.collectDrops(asmodianCoin, spawnNpc(JUVENILE_SPARKIE, ELTNEN), asmodianKiller)), Ids{ASMODIAN_BRONZE_COIN});
	EXPECT_EQ(itemIdsOf(service.collectDrops(asmodianCoin, spawnNpc(JUVENILE_SPARKIE, ELTNEN), elyosKiller)), Ids{}) << "restriction_race ASMODIANS";
}

// collectAllowedDrops' candidate filter (:436-443): an item template of the killer's race or PC_ALL (m5b3-plan.md L-05: every candidate of the
// gate's two monsters is PC_ALL, so only this case sees the race test), and min_diff <= npc level - item level <= max_diff
TEST_F(DropRegistrationServiceTest, CandidatesAreFilteredByTheKillersRaceAndTheLevelDifference) {
	const model::templates::globaldrops::GlobalRule* cellatuMeat = ruleNamed("Cellatu Meat");
	const model::templates::globaldrops::GlobalRule* powerShards = ruleNamed("Power Shards");
	ASSERT_TRUE(cellatuMeat && powerShards);
	// Cellatu Meat is an ELYOS item (item_templates.xml:744841) of level 50: the level-48 salt fin cellatu is at diff -2, the rule's minimum
	Npc& cellatu = spawnNpc(SALT_FIN_CELLATU);
	model::drop::DropModifiers elyos = plainModifiers(model::Race::ELYOS);
	model::drop::DropModifiers asmodian = plainModifiers(model::Race::ASMODIANS);
	EXPECT_EQ(itemIdsOf(service.collectDrops(cellatuMeat, cellatu, elyos)), std::vector<int32_t>{CELLATU_MEAT});
	EXPECT_TRUE(service.collectDrops(cellatuMeat, cellatu, asmodian).empty()) << "an Elyos item for an Asmodian";
	// Power Shards has seven candidates of levels 1-60; for the level-2 sparkie only the level-1 shard is in [0, 9], for the level-45 fulla
	// (rated ELITE) only the level-40 one (diff 5; the level-50 shard is at -5)
	EXPECT_EQ(itemIdsOf(service.collectDrops(powerShards, spawnNpc(JUVENILE_SPARKIE), elyos)), std::vector<int32_t>{MINOR_POWER_SHARD});
	EXPECT_EQ(itemIdsOf(service.collectDrops(powerShards, spawnNpc(FULLA), elyos)), std::vector<int32_t>{169000008});
}

// collectDrops (:417-430): more candidates than max_drop_rule (1) -> Chance.selectElement picks that many; latri (level 20) has the two
// level-20 candidates of Buff Food. DropModifiers.maxDropsPerGroup overrides the rule's number (:418).
TEST_F(DropRegistrationServiceTest, MaxDropRuleKeepsThatManyOfTheCandidates) {
	const model::templates::globaldrops::GlobalRule* buffFood = ruleNamed("Buff Food");
	ASSERT_TRUE(buffFood);
	Npc& latri = spawnNpc(LATRI);
	model::drop::DropModifiers one = plainModifiers();
	for (int32_t draw = 0; draw < 8; draw++) {
		std::vector<int32_t> ids = itemIdsOf(service.collectDrops(buffFood, latri, one));
		ASSERT_EQ(ids.size(), 1u);
		EXPECT_TRUE(ids[0] == LESSER_RALLY_SERUM || ids[0] == LESSER_FOCUS_AGENT) << ids[0];
	}
	model::drop::DropModifiers two = plainModifiers();
	two.setMaxDropsPerGroup(2);
	std::vector<int32_t> both = itemIdsOf(service.collectDrops(buffFood, latri, two));
	std::sort(both.begin(), both.end());
	EXPECT_EQ(both, (std::vector<int32_t>{LESSER_RALLY_SERUM, LESSER_FOCUS_AGENT}));
}

// --------------------------------------------------------------------------------------------------------------- the effective chance

// calculateEffectiveChance (:221-227): a dynamic_chance rule's chance times getRankModifier * getRatingModifier (:455-474), both float, then
// DropModifiers.calculateDropChance (x boost); a rule without dynamic_chance keeps its chance. One npc per rank and per rating.
TEST_F(DropRegistrationServiceTest, DynamicChancesScaleByTheNpcsRankAndRating) {
	const model::templates::globaldrops::GlobalRule* kinah = ruleNamed("Kinah");
	const model::templates::globaldrops::GlobalRule* junk = ruleNamed("JUNK_SPAKY_MATERIAL");
	ASSERT_TRUE(kinah && junk);
	model::drop::DropModifiers modifiers = plainModifiers();
	auto chanceFor = [&](int32_t npcId) { return service.calculateEffectiveChance(kinah, spawnNpc(npcId), modifiers); };

	EXPECT_EQ(chanceFor(LATRI), 50.0f * (0.9f * 0.5f)) << "NOVICE, JUNK";
	EXPECT_EQ(chanceFor(JUVENILE_SPARKIE), 50.0f * (1.0f * 1.0f)) << "DISCIPLINED, NORMAL";
	EXPECT_EQ(chanceFor(SIEGE_WEAPON), 50.0f * (1.05f * 1.3f)) << "SEASONED, ELITE";
	EXPECT_EQ(chanceFor(DATURUM_WRECKHELM), 50.0f * (1.1f * 1.8f)) << "EXPERT, HERO";
	EXPECT_EQ(chanceFor(SAENDUKAL), 50.0f * (1.1f * 2.0f)) << "EXPERT, LEGENDARY";
	EXPECT_EQ(chanceFor(FULLA), 50.0f * (1.15f * 1.3f)) << "VETERAN, ELITE";
	EXPECT_EQ(chanceFor(PROTECTOR_DEIMOS), 50.0f * (1.2f * 1.8f)) << "MASTER, HERO";
	EXPECT_EQ(service.calculateEffectiveChance(junk, spawnNpc(SAENDUKAL), modifiers), 40.0f) << "not dynamic: the chance as it is";
}

// createDropModifiers -> getReductionDropRate (:198-201): DropRewardEnum.dropRewardFrom(npc level - highest level) / 100f, null at 100, applied
// by calculateDropChance only to a level_based_chance_reduction rule (DropModifiers.java:53-57) - Omega Enchantment Stone (chance 1) for the
// level-40 chieftain and killers of level 40 .. 50
TEST_F(DropRegistrationServiceTest, LevelBasedRulesShrinkWithTheKillersLevelAdvantage) {
	DROP_REQUIRE_DATABASE();
	setDropRate(1.0f);
	Player& killer = newPlayer(700001, "Killer");
	Npc& npc = spawnNpc(SAENDUKAL, INGGISON);
	const model::templates::globaldrops::GlobalRule* omega = ruleNamed("Omega Enchantment Stone");
	const model::templates::globaldrops::GlobalRule* kinah = ruleNamed("Kinah");
	ASSERT_TRUE(omega && kinah);
	auto omegaChance = [&](int32_t highestLevel) {
		model::drop::DropModifiers modifiers = service.createDropModifiers(npc, killer, highestLevel);
		return service.calculateEffectiveChance(omega, npc, modifiers);
	};

	EXPECT_EQ(omegaChance(40), 1.0f) << "same level: 100 %, no reduction";
	EXPECT_EQ(omegaChance(45), 1.0f) << "-5: MINUS_5, 100 %";
	EXPECT_EQ(omegaChance(46), 1.0f * (80 / 100.0f)) << "-6: MINUS_6, 80 %";
	EXPECT_EQ(omegaChance(48), 1.0f * (60 / 100.0f)) << "-8: MINUS_8, 60 %";
	EXPECT_EQ(omegaChance(49), 1.0f * (40 / 100.0f)) << "-9: MINUS_9, 40 %";
	EXPECT_EQ(omegaChance(50), 0.0f) << "-10: MINUS_10, 0 %";
	model::drop::DropModifiers atFifty = service.createDropModifiers(npc, killer, 50);
	EXPECT_EQ(service.calculateEffectiveChance(kinah, npc, atFifty), 50.0f * (1.1f * 2.0f)) << "not level based: unreduced";
}

// calculateBoostDropRate (:203-219): Rates.get(killer, DROP_RATES) (the membership's value, Rates.java:166-173) * boost / 100f, where boost
// starts at the npc's and the killer's BOOST_DROP_RATE / DR_BOOST stats (100 without effects) and gets +5 for Energy of Repose and +5 for
// Energy of Salvation (a palace house is the third +5; no fixture player owns a house)
TEST_F(DropRegistrationServiceTest, TheDropRateAndTheKillersEnergiesBoostEveryChance) {
	DROP_REQUIRE_DATABASE();
	Player& killer = newPlayer(700001, "Killer");
	Npc& npc = spawnNpc(JUVENILE_SPARKIE);
	auto boost = [&] { return service.createDropModifiers(npc, killer, killer.getLevel()).getBoostDropRate(); };

	setDropRate(1.0f);
	EXPECT_EQ(boost(), 1.0f * 100 / 100.0f);
	configs::main::RatesConfig::DROP_RATES.set(std::vector<float>{1.0f, 2.0f}); // the shipped default "1.0, 2.0"
	EXPECT_EQ(boost(), 1.0f) << "membership 0";
	killer.getAccount()->setMembership(1);
	EXPECT_EQ(boost(), 2.0f * 100 / 100.0f) << "membership 1: the second value";
	killer.getAccount()->setMembership(4);
	EXPECT_EQ(boost(), 2.0f) << "a membership beyond the list: its last value";
	killer.getAccount()->setMembership(0);
	setDropRate(2.5f);
	killer.getCommonData()->setCurrentReposeEnergy(1);
	EXPECT_EQ(boost(), 2.5f * 105 / 100.0f) << "Energy of Repose: +5";
	killer.getCommonData()->setCurrentSalvationPoints(1000);
	EXPECT_EQ(boost(), 2.5f * 110 / 100.0f) << "and Energy of Salvation (1 %): +5";
}

// createDropModifiers (:111-120): a chest is an npc with the "chest" AI or a group_drop starting with "treasure" or ending with "box" (lower
// case); the drop race is the looter's
TEST_F(DropRegistrationServiceTest, ModifiersKnowAChestByItsDropGroupAndTheLootersRace) {
	DROP_REQUIRE_DATABASE();
	setDropRate(1.0f);
	Player& elyos = newPlayer(700001, "Elyos");
	Player& asmodian = newPlayer(700002, "Asmodian", model::Race::ASMODIANS);

	EXPECT_TRUE(service.createDropModifiers(spawnNpc(BIG_CARGO_BOX), elyos, 1).isDropNpcChest()) << "group_drop FAKEBOX ends with box";
	EXPECT_FALSE(service.createDropModifiers(spawnNpc(JUVENILE_SPARKIE), elyos, 1).isDropNpcChest());
	EXPECT_EQ(service.createDropModifiers(spawnNpc(JUVENILE_SPARKIE), elyos, 1).getDropRace(), model::Race::ELYOS);
	EXPECT_EQ(service.createDropModifiers(spawnNpc(JUVENILE_SPARKIE), asmodian, 1).getDropRace(), model::Race::ASMODIANS);
}

} // namespace
} // namespace aion::gameserver::economy::test
