#pragma once

// P6-Q ascension route (lane route-gen, 2026-09-29): the generated quest handlers the golden trace harness drives (phase6-inventory.md §7.6
// item 3). Each AION_QUEST_HANDLER marker defines `<Class>_questFactory()` in the namespace of its directory (HandlerRegistry.h); this table
// names them. The poeta handlers come from Q05's own library (aion_gs_handlers_quest_q05, linked into this executable); a handler target's
// tests link only their own library, so the ishalgen (Q09) and ascension (Q06) handlers are compiled into this executable by #include
// (GoldenIshalgenHandlers.cpp, GoldenAscensionHandlers.cpp), the way P5-05's tests compile the two quest npc AIs (chunks.cmake, the P5-05 LEASE
// row), and so are the altgard and pandaemonium handlers of P6-Q slice 2 (Q10: GoldenAltgardHandlers.cpp, GoldenPandaemoniumHandlers.cpp; the
// five hand ports and the held-back 4212 are not generated, docs/deviations/Q10.md). The generated files themselves are never edited here.
//
// P6-Q slice 2, chunk Q03 (2026-09-29): the 72 generated verteron and heiron handlers in the tree (the zones the route's dispatches end in),
// compiled into this executable by #include as well (GoldenQ03Handlers.cpp); Q03's hand ports 1643 and 3200 have their own unit cases
// (tests/quest_handlers_q03, docs/deviations/Q03.md).
//
// Held back (GOLDEN_HELD_BACK): 1000, 2000, 1100 and 2100 register onEnterWorld and start their quest at a character's first enter world
// (Java behaviour), which turns gs.scenario.m5a, m5b and m5b2 red (docs/deviations/Q05.md, "Gate impact"). Their generated files are not in
// the handler tree until the gates' owners decide; the harness passed all 30 of their cases with them in (the bytes the generator emits).
// Q03's 14010, 1131, 1146 and 1152 are held back the same way (the review of 2026-09-29): gs.scenario.travel's level-10 Elyos Daeva arrives
// in Verteron, where 14010's onEnterWorldEvent starts its quest and the start npcs of 1131, 1146 and 1152 add their quests (min level 11-12,
// grey) to the arrival's SM_NEARBY_QUESTS (docs/deviations/Q03.md, "Held back"); the harness passed all of their cases with them in.
// The integration of slice 2 held 16 of Q10's generated handlers back the same way (docs/design/p6q-ascension-route.md, "Slice 2"):
// gs.scenario.travel's level-10 Asmodian Daeva logs in beside Doman in Pandaemonium, where 2953, 29004, 29048 and 4973 are in his
// SM_NEARBY_QUESTS, and arrives in Altgard, where 24010's onEnterWorldEvent starts its quest and 2207, 2209, 2213, 2221, 2223, 2231, 2232,
// 2239, 2288 and 2917 are in the arrival's SM_NEARBY_QUESTS; 2911 (after 2009, start npc in Pandaemonium) for gs.scenario.ascension's
// Asmodian after the ceremony. The harness passed all of their cases with them in (docs/deviations/Q10.md, "Held back").

#include <cstdint>
#include <memory>
#include <string_view>
#include <vector>

#include "aion/gameserver/questEngine/handlers/AbstractQuestHandler.h"

// clang-format off
#define AION_GOLDEN_GENERATED_HANDLERS(X) \
	X(poeta, _1001TheKerubThreat, 1001) \
	X(poeta, _1003IllegalLogging, 1003) \
	X(poeta, _1004NeutralizingOdium, 1004) \
	X(poeta, _1005BarringtheGate, 1005) \
	X(poeta, _1107TheLostAxe, 1107) \
	X(poeta, _1111InsomniaMedicine, 1111) \
	X(poeta, _1122DeliveringPernossRobe, 1122) \
	X(poeta, _1123WheresTutty, 1123) \
	X(poeta, _1205ANewSkill, 1205) \
	X(ishalgen, _2001ThinkingAhead, 2001) \
	X(ishalgen, _2003TreasureOfTheDeceased, 2003) \
	X(ishalgen, _2005TeachingaLesson, 2005) \
	X(ishalgen, _2006HitThemWhereitHurts, 2006) \
	X(ishalgen, _2106VanarsFlattery, 2106) \
	X(ishalgen, _2114TheInsectProblem, 2114) \
	X(ishalgen, _2122AshesToAshes, 2122) \
	X(ishalgen, _2123TheImprisonedGourmet, 2123) \
	X(ishalgen, _2125TheRobberyPlot, 2125) \
	X(ishalgen, _2132ANewSkill, 2132) \
	X(ishalgen, _2135ForLoveofNegi, 2135) \
	X(ascension, _19070ADispatchtoVerteron, 19070) \
	X(ascension, _19071ADispatchtoVerteron, 19071) \
	X(ascension, _1913DispatchtoVerteron, 1913) \
	X(ascension, _1914DispatchtoVerteron, 1914) \
	X(ascension, _1915DispatchtoVerteron, 1915) \
	X(ascension, _1916DispatchtoVerteron, 1916) \
	X(ascension, _2901DispatchtoAltgard, 2901) \
	X(ascension, _2902DispatchtoAltgard, 2902) \
	X(ascension, _2903DispatchtoAltgard, 2903) \
	X(ascension, _2904DispatchtoAltgard, 2904) \
	X(ascension, _29070ADispatchtoAltgard, 29070) \
	X(ascension, _29071ADispatchtoAltgard, 29071) \
	X(verteron, _1141BelbuasTreasure, 1141) \
	X(verteron, _1149MissingPoppy, 1149) \
	X(verteron, _1156StolenVillageSeal, 1156) \
	X(verteron, _1157GaphyrksLove, 1157) \
	X(verteron, _1158VillageSealFound, 1158) \
	X(verteron, _1162AltenosWeddingRing, 1162) \
	X(verteron, _1163ArachnaAntidote, 1163) \
	X(verteron, _1169LightningfootTuka, 1169) \
	X(verteron, _1170HeadlessStoneStatue, 1170) \
	X(verteron, _1183SpiritOfNature, 1183) \
	X(verteron, _1192VerteronReinforcements, 1192) \
	X(verteron, _1194ReducingTursinStrength, 1194) \
	X(verteron, _1197KrallBook, 1197) \
	X(verteron, _1198TheWritingOnTheWall, 1198) \
	X(verteron, _1218NumonerksDemandNote, 1218) \
	X(verteron, _1220ASecretDelivery, 1220) \
	X(verteron, _14011FragmentsInTheSky, 14011) \
	X(verteron, _14012DukakiMischief, 14012) \
	X(verteron, _14013AFrillOfAFuss, 14013) \
	X(verteron, _14014TurningTheIde, 14014) \
	X(verteron, _14015NotBlindedByVengeance, 14015) \
	X(verteron, _14016AGateAgape, 14016) \
	X(heiron, _14050OrdersFromHeironFortress, 14050) \
	X(heiron, _14051RootOfTheProblem, 14051) \
	X(heiron, _14052RestlessSouls, 14052) \
	X(heiron, _14053DangerCubed, 14053) \
	X(heiron, _14054KrallIngToKralltumagna, 14054) \
	X(heiron, _1527RottenRotrons, 1527) \
	X(heiron, _1528StrangeLeather, 1528) \
	X(heiron, _1535TheColdColdGround, 1535) \
	X(heiron, _1537FishOnTheLine, 1537) \
	X(heiron, _1540BaittheHooks, 1540) \
	X(heiron, _1548KlawControl, 1548) \
	X(heiron, _1553MirrorMirror, 1553) \
	X(heiron, _1559WhatsintheBox, 1559) \
	X(heiron, _1560AJobForPobinerk, 1560) \
	X(heiron, _1561TheMisersMap, 1561) \
	X(heiron, _1562CrossedDestiny, 1562) \
	X(heiron, _1563TheLegendofVindachinerk, 1563) \
	X(heiron, _1573SomeTastyMushrooms, 1573) \
	X(heiron, _1574AFeatForAVillage, 1574) \
	X(heiron, _1578WhereDoRotronsComeFrom, 1578) \
	X(heiron, _1582ThePriestsNightmare, 1582) \
	X(heiron, _1604ToCatchASpy, 1604) \
	X(heiron, _1605TheLepharistSituation, 1605) \
	X(heiron, _1607MappingTheRevolutionaries, 1607) \
	X(heiron, _1609MessageToArbolusHaven, 1609) \
	X(heiron, _1612LepharistSecrets, 1612) \
	X(heiron, _1614WheresBelbua, 1614) \
	X(heiron, _1620StartSpreadingTheNews, 1620) \
	X(heiron, _1626LightThePath, 1626) \
	X(heiron, _1628MeteriasRegret, 1628) \
	X(heiron, _1634TheWreckOfTheArgos, 1634) \
	X(heiron, _1636AFluteForTheFixing, 1636) \
	X(heiron, _1640TeleporterRepairs, 1640) \
	X(heiron, _1644AVeryOldLetter, 1644) \
	X(heiron, _1647DressingUpForBollvig, 1647) \
	X(heiron, _1648UndeadWarAlert, 1648) \
	X(heiron, _1661FindingTheForges, 1661) \
	X(heiron, _1670InvisibleBridges, 1670) \
	X(heiron, _1687TheTigrakiAgreement, 1687) \
	X(heiron, _1691TheLittleLeatherSlipper, 1691) \
	X(heiron, _1692ADayOlderAndDeeperInDebt, 1692) \
	X(heiron, _1693AreYouMyFather, 1693) \
	X(heiron, _18600ScoringSomeBadStigma, 18600) \
	X(heiron, _18601NightmareonMyStreets, 18601) \
	X(heiron, _18602NightmareinShiningArmor, 18602) \
	X(heiron, _3502NereusNeedsYou, 3502) \
	X(heiron, _80217ToDarkPoeta, 80217) \
	X(heiron, _80218DarkPoetaEncore, 80218) \
	X(heiron, _80219DarkPoetaFinale, 80219) \
	X(heiron, _80220DarkPoetaFinale, 80220) \
	X(altgard, _2216MuMuGrassKnot, 2216) \
	X(altgard, _2222ManirsMessage, 2222) \
	X(altgard, _2228AThornInItsSide, 2228) \
	X(altgard, _2247TheGergersDisguise, 2247) \
	X(altgard, _2263ShugoPotion, 2263) \
	X(altgard, _2266ATrustworthyMessenger, 2266) \
	X(altgard, _2271AurtrisLetter, 2271) \
	X(altgard, _2278ASecretProposal, 2278) \
	X(altgard, _2279SolidProof, 2279) \
	X(altgard, _2284EscapingAsmodae, 2284) \
	X(altgard, _2289RampagingMosbears, 2289) \
	X(altgard, _2290GrokensEscape, 2290) \
	X(altgard, _24011FunnyFloatingFungus, 24011) \
	X(altgard, _24012AnOminousCrop, 24012) \
	X(altgard, _24014StompOutThePlot, 24014) \
	X(altgard, _24015TotemPlowed, 24015) \
	X(altgard, _24016AStrangeNewThread, 24016) \
	X(altgard, _24112NoLaissezFaireForLepharists, 24112) \
	X(pandaemonium, _2912FollowtheRibbon, 2912) \
	X(pandaemonium, _2913AChainofDebt, 2913) \
	X(pandaemonium, _2914ATokenofLostLove, 2914) \
	X(pandaemonium, _2916ManInTheLongBlackRobe, 2916) \
	X(pandaemonium, _2918DeepMaternalLove, 2918) \
	X(pandaemonium, _2919BookOfOblivion, 2919) \
	X(pandaemonium, _2920ElementaryMyDearDaeva, 2920) \
	X(pandaemonium, _2921LoveAtFirstSight, 2921) \
	X(pandaemonium, _2922FascinatingGift, 2922) \
	X(pandaemonium, _2925AHeartfeltConfession, 2925) \
	X(pandaemonium, _2928PowerofLove, 2928) \
	X(pandaemonium, _2937UnexpectedReward, 2937) \
	X(pandaemonium, _2938SecretLibraryAccess, 2938) \
	X(pandaemonium, _2948HuronsLetter, 2948) \
	X(pandaemonium, _2952WinningVindachinerksFavor, 2952) \
	X(pandaemonium, _2954DeliveringOdellaJuice, 2954) \
	X(pandaemonium, _2957FlowersForTheBanquet, 2957) \
	X(pandaemonium, _2958LastMinuteWorries, 2958) \
	X(pandaemonium, _2962JafnharWhereabouts, 2962) \
	X(pandaemonium, _2963OnBehalfOfAFriend, 2963) \
	X(pandaemonium, _2965AncientWeapons, 2965) \
	X(pandaemonium, _2985AnExpertsReward, 2985) \
	X(pandaemonium, _4210MissingHaorunerk, 4210) \
	X(pandaemonium, _4905InterviewingTheVeterans, 4905) \
	X(pandaemonium, _4906TalesOfHeroes, 4906) \
	X(pandaemonium, _4920MakingTheActivatedSurkana, 4920) \
	X(pandaemonium, _4966GrowthNinissFirstCharm, 4966) \
	X(pandaemonium, _4967GrowthNinissSecondCharm, 4967) \
	X(pandaemonium, _4968GrowthNinissThirdCharm, 4968) \
	X(pandaemonium, _4969GrowthNinissFourthCharm, 4969) \
	X(pandaemonium, _4970TheFashionistas, 4970) \
	X(pandaemonium, _4971ProjectRunway, 4971) \
	X(pandaemonium, _4972JudgeNot, 4972) \
	X(pandaemonium, _4974TheSecretOfHisSuccess, 4974) \
	X(pandaemonium, _4976ASettlerAmbition, 4976)
// clang-format on

#define AION_GOLDEN_DECLARE_FACTORY(dir, Class, questId)                                                                                       \
	namespace aion::gameserver::handlers::quest::dir {                                                                                           \
	::std::unique_ptr<::aion::gameserver::questEngine::handlers::AbstractQuestHandler> Class##_questFactory();                                  \
	}
AION_GOLDEN_GENERATED_HANDLERS(AION_GOLDEN_DECLARE_FACTORY)
#undef AION_GOLDEN_DECLARE_FACTORY

namespace aion::gameserver::questEngine::handlers::test::golden {

/** The route's generated handlers kept out of the handler tree for their gate impact (see above) */
inline constexpr int32_t GOLDEN_HELD_BACK[] = {1000, 1100, 2000, 2100, 1131, 1146, 1152, 14010, 2207, 2209, 2213, 2221, 2223, 2231, 2232,
	2239, 2288, 24010, 2911, 2917, 2953, 29004, 29048, 4973};

struct GeneratedHandler {
	std::string_view directory;
	std::string_view javaClass;
	int32_t questId;
	std::unique_ptr<AbstractQuestHandler> (*factory)();
};

/** Every generated handler of the table, in the table's order (poeta, ishalgen, ascension, verteron, heiron, altgard, pandaemonium; file name
 * order inside a directory) */
inline const std::vector<GeneratedHandler>& generatedHandlers() {
#define AION_GOLDEN_ENTRY(dir, Class, questId) GeneratedHandler{#dir, #Class, questId, &::aion::gameserver::handlers::quest::dir::Class##_questFactory},
	static const std::vector<GeneratedHandler> table{AION_GOLDEN_GENERATED_HANDLERS(AION_GOLDEN_ENTRY)};
#undef AION_GOLDEN_ENTRY
	return table;
}

inline const GeneratedHandler* generatedHandler(int32_t questId) {
	for (const GeneratedHandler& handler : generatedHandlers()) {
		if (handler.questId == questId)
			return &handler;
	}
	return nullptr;
}

} // namespace aion::gameserver::questEngine::handlers::test::golden
