#pragma once

// P6-Q ascension route (lane route-gen, 2026-09-29): the generated quest handlers the golden trace harness drives (phase6-inventory.md §7.6
// item 3). Each AION_QUEST_HANDLER marker defines `<Class>_questFactory()` in the namespace of its directory (HandlerRegistry.h); this table
// names them. The poeta handlers come from Q05's own library (aion_gs_handlers_quest_q05, linked into this executable); a handler target's
// tests link only their own library, so the ishalgen (Q09) and ascension (Q06) handlers are compiled into this executable by #include
// (GoldenIshalgenHandlers.cpp, GoldenAscensionHandlers.cpp), the way P5-05's tests compile the two quest npc AIs (chunks.cmake, the P5-05 LEASE
// row), and so are the altgard and pandaemonium handlers of P6-Q slice 2 (Q10: GoldenAltgardHandlers.cpp, GoldenPandaemoniumHandlers.cpp; the
// five hand ports and the held-back 4212 are not generated, docs/deviations/Q10.md). The generated files themselves are never edited here.
//
// Phase 6 step 2, chunk Q08 (lane C, 2026-10-05): the 64 generated gelkmaros and enshar handlers, compiled in by #include as well
// (GoldenQ08Handlers.cpp; docs/deviations/Q08.md); chunk Q01 (the same lane and day): the 81 generated reshanta handlers
// (GoldenQ01Handlers.cpp; docs/deviations/Q01.md). Q01 is the library aion_gs_handlers_quest_reshanta, which this executable does not link.
// Chunk Q02 (the same lane and day): the 59 generated inggison handlers (GoldenQ02Handlers.cpp; docs/deviations/Q02.md).
// Chunk Q14 (the same lane and day): the 74 generated handlers of the instance directories K-W (GoldenQ14Handlers.cpp; docs/deviations/Q14.md),
// 91 since the owner's decisions of 2026-10-07 (the 15 mentor dailies and pangaea 14220/24220).
//
// P6-Q slice 2, chunk Q03 (2026-09-29): the 72 generated verteron and heiron handlers in the tree (the zones the route's dispatches end in),
// compiled into this executable by #include as well (GoldenQ03Handlers.cpp); Q03's hand ports 1643 and 3200 have their own unit cases
// (tests/quest_handlers_q03, docs/deviations/Q03.md).
//
// 1000, 1100, 2000 and 2100 register onEnterWorld and start their quest at a character's first enter world (Java behaviour). They were held
// back for their gate impact until the owner's answer of 2026-09-29 (owner-decisions.md: "A", land them and let gs.scenario.m5a, m5b and m5b2
// expect the prologue traffic); they are in the table since (docs/deviations/Q05.md, "Gate impact").
//
// Held back (GOLDEN_HELD_BACK): Q03's 14010, 1131, 1146 and 1152 (the review of 2026-09-29): gs.scenario.travel's level-10 Elyos Daeva arrives
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
	X(poeta, _1000Prologue, 1000) \
	X(poeta, _1001TheKerubThreat, 1001) \
	X(poeta, _1003IllegalLogging, 1003) \
	X(poeta, _1004NeutralizingOdium, 1004) \
	X(poeta, _1005BarringtheGate, 1005) \
	X(poeta, _1100KaliosCall, 1100) \
	X(poeta, _1107TheLostAxe, 1107) \
	X(poeta, _1111InsomniaMedicine, 1111) \
	X(poeta, _1122DeliveringPernossRobe, 1122) \
	X(poeta, _1123WheresTutty, 1123) \
	X(poeta, _1205ANewSkill, 1205) \
	X(ishalgen, _2000Prologue, 2000) \
	X(ishalgen, _2001ThinkingAhead, 2001) \
	X(ishalgen, _2003TreasureOfTheDeceased, 2003) \
	X(ishalgen, _2005TeachingaLesson, 2005) \
	X(ishalgen, _2006HitThemWhereitHurts, 2006) \
	X(ishalgen, _2100OrderoftheCaptain, 2100) \
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
	X(pandaemonium, _4976ASettlerAmbition, 4976) \
	X(gelkmaros, _20031GotoGelkmaros, 20031) \
	X(gelkmaros, _20032AllAboutAbnormalAether, 20032) \
	X(gelkmaros, _20033DranaSolution, 20033) \
	X(gelkmaros, _20034RescuetheReians, 20034) \
	X(gelkmaros, _20035SilenteraSupport, 20035) \
	X(gelkmaros, _21004VillageStatusReport, 21004) \
	X(gelkmaros, _21027FearlessKantele, 21027) \
	X(gelkmaros, _21033ExorcisingInfisto, 21033) \
	X(gelkmaros, _21036DeliveryofAetherSample, 21036) \
	X(gelkmaros, _21051TroubleinStone, 21051) \
	X(gelkmaros, _21052DragonHuntin, 21052) \
	X(gelkmaros, _21053DramataDrama, 21053) \
	X(gelkmaros, _21054MissionofDestiny, 21054) \
	X(gelkmaros, _21056FundinOrders, 21056) \
	X(gelkmaros, _21057FundinOldGrudge, 21057) \
	X(gelkmaros, _21058KirhuaSpecialOrder, 21058) \
	X(gelkmaros, _21059ShiningScroll, 21059) \
	X(gelkmaros, _21060EliminatePadmarashka, 21060) \
	X(gelkmaros, _21061NewOrder, 21061) \
	X(gelkmaros, _21062TheDramataWrath, 21062) \
	X(gelkmaros, _21063VanquishVeille, 21063) \
	X(gelkmaros, _21068TheGameIsAfoot, 21068) \
	X(gelkmaros, _21070TheSummation, 21070) \
	X(gelkmaros, _21071MissingBard, 21071) \
	X(gelkmaros, _21073ListentoMySongStrigiks, 21073) \
	X(gelkmaros, _21075FatedHeartbreak, 21075) \
	X(gelkmaros, _21080MessageInAWindstream, 21080) \
	X(gelkmaros, _21081A_Helping_Hand, 21081) \
	X(gelkmaros, _21105CoweringRefugee, 21105) \
	X(gelkmaros, _21106TheRealRhonnam, 21106) \
	X(gelkmaros, _21111TestYourMight, 21111) \
	X(gelkmaros, _21114PoisonedFungi, 21114) \
	X(gelkmaros, _21125MysteryBlueprint, 21125) \
	X(gelkmaros, _21135VellunRequest, 21135) \
	X(gelkmaros, _21136InSearchOfAWitness, 21136) \
	X(gelkmaros, _21137BerokinImageMarble, 21137) \
	X(gelkmaros, _21138OddStrigik, 21138) \
	X(gelkmaros, _21217NewResearchPlan, 21217) \
	X(gelkmaros, _21221RustyRelic, 21221) \
	X(gelkmaros, _21244SearchForTheBiolab, 21244) \
	X(gelkmaros, _21249TheInvincibleStarket, 21249) \
	X(gelkmaros, _21296PadmarashkaLegacy, 21296) \
	X(gelkmaros, _21455IngredientsForTheAntidote, 21455) \
	X(gelkmaros, _21458PracticalResearch, 21458) \
	X(gelkmaros, _21460AShulacksStory, 21460) \
	X(enshar, _20500EnsharExpedition, 20500) \
	X(enshar, _20501WhattheRuinsSay, 20501) \
	X(enshar, _20502EvolvingMysteries, 20502) \
	X(enshar, _20503AncientEvilPlans, 20503) \
	X(enshar, _20504TiamatsShadow, 20504) \
	X(enshar, _20505AncientCrystal, 20505) \
	X(enshar, _20506MuscleOverMind, 20506) \
	X(enshar, _20507ItsWorseThanWeThought, 20507) \
	X(enshar, _25022SoupDeCure, 25022) \
	X(enshar, _25023SproutingDevelopments, 25023) \
	X(enshar, _25030CluesFromTheUndead, 25030) \
	X(enshar, _25031TheTejhiGhost, 25031) \
	X(enshar, _25032AvengeVarnur, 25032) \
	X(enshar, _25050TreasureInTheDeepSea, 25050) \
	X(enshar, _25051TreasureOfAncientKings, 25051) \
	X(enshar, _25052AnOfferingPeace, 25052) \
	X(enshar, _25062OminousAdvice, 25062) \
	X(enshar, _25070TruthOfTheCrystal, 25070) \
	X(enshar, _25073NoRevivalForTheBalaur, 25073) \
	X(reshanta, _14040OrdersFromReshanta, 14040) \
	X(reshanta, _14041AbyssalAbilities, 14041) \
	X(reshanta, _14042ARescueOperation, 14042) \
	X(reshanta, _14043DrawlingBalaur, 14043) \
	X(reshanta, _14044ShardsOfMemory, 14044) \
	X(reshanta, _14045RumorsOnWings, 14045) \
	X(reshanta, _14046PiecingTheMemory, 14046) \
	X(reshanta, _14047ChainingMemories, 14047) \
	X(reshanta, _1701GovernorsDirective, 1701) \
	X(reshanta, _1702Defeat9thRankAsmodianSoldiers, 1702) \
	X(reshanta, _1703Defeat8thRankAsmodianSoldiers, 1703) \
	X(reshanta, _1704Defeat7thRankAsmodianSoldiers, 1704) \
	X(reshanta, _1705Defeat6thRankAsmodianSoldiers, 1705) \
	X(reshanta, _1706Defeat5thRankAsmodianSoldiers, 1706) \
	X(reshanta, _1707Defeat4thRankAsmodianSoldiers, 1707) \
	X(reshanta, _1708Defeat3thRankAsmodianSoldiers, 1708) \
	X(reshanta, _1709Defeat2thRankAsmodianSoldiers, 1709) \
	X(reshanta, _1710Defeat1thRankAsmodianSoldiers, 1710) \
	X(reshanta, _1718TradingDown, 1718) \
	X(reshanta, _1719ConfrontAsmodianOfficers, 1719) \
	X(reshanta, _1720ConfrontAsmodianGenerals, 1720) \
	X(reshanta, _1721MeetingwiththeBrigadeGeneral, 1721) \
	X(reshanta, _1722RastinsHomesickness, 1722) \
	X(reshanta, _1724ReaperExpertise, 1724) \
	X(reshanta, _1725CenturionsForgetfulness, 1725) \
	X(reshanta, _1726ScoutingtheLake, 1726) \
	X(reshanta, _1727RecruitsforNezekansShield, 1727) \
	X(reshanta, _1761SohonerkWish, 1761) \
	X(reshanta, _1777CalloftheGovernor, 1777) \
	X(reshanta, _1798JakurerksShotattheBigTime, 1798) \
	X(reshanta, _1799PupilsDiary, 1799) \
	X(reshanta, _1800JaiorunerksTombstone, 1800) \
	X(reshanta, _1845OpeningDoors, 1845) \
	X(reshanta, _1846PaperTrail, 1846) \
	X(reshanta, _1847AStrangeSoul, 1847) \
	X(reshanta, _1851UnchartedIslands, 1851) \
	X(reshanta, _1853OfficerOusting, 1853) \
	X(reshanta, _1854GeneralPurge, 1854) \
	X(reshanta, _24040VotansOrders, 24040) \
	X(reshanta, _24041TrainingInTheAbyss, 24041) \
	X(reshanta, _24042AReadyRescue, 24042) \
	X(reshanta, _24043LazyLanguageLessons, 24043) \
	X(reshanta, _24044ChangeTheFuture, 24044) \
	X(reshanta, _24045ASpeedyErrand, 24045) \
	X(reshanta, _24046TheShadowCalls, 24046) \
	X(reshanta, _2701TheGovernorsSummons, 2701) \
	X(reshanta, _2702Defeat9thRankElyosSoldiers, 2702) \
	X(reshanta, _2703Defeat8thRankElyosSoldiers, 2703) \
	X(reshanta, _2704Defeat7thRankElyosSoldiers, 2704) \
	X(reshanta, _2705Defeat6thRankElyosSoldiers, 2705) \
	X(reshanta, _2706Defeat5thRankElyosSoldiers, 2706) \
	X(reshanta, _2707Defeat4thRankElyosSoldiers, 2707) \
	X(reshanta, _2708Defeat3thRankElyosSoldiers, 2708) \
	X(reshanta, _2709Defeat2thRankElyosSoldiers, 2709) \
	X(reshanta, _2710Defeat1thRankElyosSoldiers, 2710) \
	X(reshanta, _2718TradingDown, 2718) \
	X(reshanta, _2719ChallengeElyosOfficers, 2719) \
	X(reshanta, _2720ChallengeElyosGenerals, 2720) \
	X(reshanta, _2721MeetingWithTheBrigadeGeneral, 2721) \
	X(reshanta, _2722TheComfortsofHome, 2722) \
	X(reshanta, _2724MissingInAction, 2724) \
	X(reshanta, _2727TransparentMotives, 2727) \
	X(reshanta, _2758CarryTheFlame, 2758) \
	X(reshanta, _2767AFruitfulPartnership, 2767) \
	X(reshanta, _2798SignontheDottedLine, 2798) \
	X(reshanta, _2841CleansingtheAsteriaChamber, 2841) \
	X(reshanta, _2842BalaurintheUndergroundFortress, 2842) \
	X(reshanta, _2843OperationAnnihilate, 2843) \
	X(reshanta, _2850OfficerObliteration, 2850) \
	X(reshanta, _2851GeneralMassacre, 2851) \
	X(reshanta, _3205FortheBlackCloudTraders, 3205) \
	X(reshanta, _3701TeachThemaLesson, 3701) \
	X(reshanta, _3702GeneralDestruction, 3702) \
	X(reshanta, _3711ToKillACaptain, 3711) \
	X(reshanta, _3712DredgionPrisonBreak, 3712) \
	X(reshanta, _3718DredgingTheDredgion, 3718) \
	X(reshanta, _4205SmackTheShulack, 4205) \
	X(reshanta, _4702GeneralDeath, 4702) \
	X(reshanta, _4711TheDredgionCaptain, 4711) \
	X(reshanta, _4712EscapeFromTheDredgion, 4712) \
	X(reshanta, _4718PressingTheAttack, 4718) \
	X(inggison, _10031ARiskfortheObelisk, 10031) \
	X(inggison, _10032HelpintheHollow, 10032) \
	X(inggison, _10033PetrifiedSubside, 10033) \
	X(inggison, _10034FoundUnderground, 10034) \
	X(inggison, _10035SoartotheCorridor, 10035) \
	X(inggison, _11000WisplightMoralTour, 11000) \
	X(inggison, _11001KindMeira, 11001) \
	X(inggison, _11003MaintainingtheIllusion, 11003) \
	X(inggison, _11005TheLimitsofGenius, 11005) \
	X(inggison, _11006TestingTheWaters, 11006) \
	X(inggison, _11008LetterOfEncouragement, 11008) \
	X(inggison, _11009MeiriaFriendlySuggestion, 11009) \
	X(inggison, _11010AngelToTheWounded, 11010) \
	X(inggison, _11012PracticalNursing, 11012) \
	X(inggison, _11026SolidEvidence, 11026) \
	X(inggison, _11031CanIEatIt, 11031) \
	X(inggison, _11032EverythingsBetterWithTentacles, 11032) \
	X(inggison, _11033YouMakeMeSick, 11033) \
	X(inggison, _11036UncommonRecipe, 11036) \
	X(inggison, _11040SquampOnTheCookingPlate, 11040) \
	X(inggison, _11046BoxPickedUpInTheForest, 11046) \
	X(inggison, _11053TheseShoesAreMadeForStalking, 11053) \
	X(inggison, _11056EliminationOrder, 11056) \
	X(inggison, _11057StanisSecretOrder, 11057) \
	X(inggison, _11058TemenosSecretOrder, 11058) \
	X(inggison, _11060TheOrbsOrders, 11060) \
	X(inggison, _11061TwilightOfRagnarok, 11061) \
	X(inggison, _11062PadmarashkaWrath, 11062) \
	X(inggison, _11063QuellMastarius, 11063) \
	X(inggison, _11068AMysteriousWind, 11068) \
	X(inggison, _11069MookieTravelTips, 11069) \
	X(inggison, _11070CraftyMessenger, 11070) \
	X(inggison, _11072DelusCulinaryVictim, 11072) \
	X(inggison, _11076ProofOfTalent, 11076) \
	X(inggison, _11077AWeaponOfWorth, 11077) \
	X(inggison, _11103FiniteWalk, 11103) \
	X(inggison, _11105WifesNagging, 11105) \
	X(inggison, _11106RewritingHistory, 11106) \
	X(inggison, _11107ComfortisaBox, 11107) \
	X(inggison, _11109TheNegotiators, 11109) \
	X(inggison, _11110KillingTime, 11110) \
	X(inggison, _11116MunchingMookiePickles, 11116) \
	X(inggison, _11117MedicationforSetzkiki, 11117) \
	X(inggison, _11118MakingSetzkikiLaugh, 11118) \
	X(inggison, _11123SuspiciousBook, 11123) \
	X(inggison, _11139TheBadNews, 11139) \
	X(inggison, _11143BabyShulackJourney, 11143) \
	X(inggison, _11147CuteBeadyEyes, 11147) \
	X(inggison, _11149TheLadyLayout, 11149) \
	X(inggison, _11212BalaurRecords, 11212) \
	X(inggison, _11227EasyAs, 11227) \
	X(inggison, _11228HeNeverReturned, 11228) \
	X(inggison, _11233SuleionTreasure, 11233) \
	X(inggison, _11289VeillesGift, 11289) \
	X(inggison, _11294SpawningInvestigation, 11294) \
	X(inggison, _11304TheRemainingFaithful, 11304) \
	X(inggison, _11455WhentheTimeisRipe, 11455) \
	X(inggison, _11458AdiassReport, 11458) \
	X(inggison, _11460TheShulackofTaloc, 11460) \
	X(kaldor, _13817TheFuryWithin, 13817) \
	X(kaldor, _23817WeeklyFreeSpirit, 23817) \
	X(kromedes_trial, _18604MeetingWithRotan, 18604) \
	X(kromedes_trial, _28604RecoveringRotan, 28604) \
	X(levinshor, _13704FonasQuickFix, 13704) \
	X(levinshor, _13708ProximityProtect, 13708) \
	X(levinshor, _13745EljersRequest, 13745) \
	X(levinshor, _23704LoudNoises, 23704) \
	X(levinshor, _23708SoundtheAlarm, 23708) \
	X(levinshor, _23745NoMoreinLevinshor, 23745) \
	X(linkgate_foundry, _16940DiarySecrets, 16940) \
	X(linkgate_foundry, _26940RaidtheLinkgateFoundry, 26940) \
	X(marchutan_priory, _47000AltgardOrbIt, 47000) \
	X(marchutan_priory, _47003AGlobalProblem, 47003) \
	X(marchutan_priory, _47006AmplifiersWithIssues, 47006) \
	X(marchutan_priory, _48006TheMarchutanPrioryBeckons, 48006) \
	X(miragent_holy_templar, _19064TemplarOfConstruction, 19064) \
	X(miragent_holy_templar, _3933ClassPreceptorConsent, 3933) \
	X(miragent_holy_templar, _3934TheQuestForTemplars, 3934) \
	X(miragent_holy_templar, _3935ShoulderTheBurden, 3935) \
	X(miragent_holy_templar, _3936DecorationsOfSanctum, 3936) \
	X(miragent_holy_templar, _3937GroupTheDecorationsofSanctum, 3937) \
	X(miragent_holy_templar, _3938WellRounded, 3938) \
	X(miragent_holy_templar, _3939PersistenceAndLuck, 3939) \
	X(miragent_holy_templar, _3940Loyalty, 3940) \
	X(nightmare_circus, _80341EventAHallowedEve, 80341) \
	X(orichalcum_key, _37100MutantNinjaIninas, 37100) \
	X(orichalcum_key, _37103CamoAndCarnage, 37103) \
	X(orichalcum_key, _37106AsmoHunt, 37106) \
	X(orichalcum_key, _37107CoolBlueWater, 37107) \
	X(orichalcum_key, _37110MyYoungApprentice, 37110) \
	X(orichalcum_key, _37113AsmoICU, 37113) \
	X(orichalcum_key, _38007AKeyMessage, 38007) \
	X(pangaea, _14220NewZoneNewRules, 14220) \
	X(pangaea, _24220WelcometoPanesterra, 24220) \
	X(radiant_ops, _38001RadiantOpsRecruitment, 38001) \
	X(rentus_base, _30500Desperation, 30500) \
	X(rentus_base, _30503RodelionRescue, 30503) \
	X(rentus_base, _30504TheSearchforPaios, 30504) \
	X(rentus_base, _30550MomentOfCrisis, 30550) \
	X(rentus_base, _30553ComradesInArms, 30553) \
	X(rentus_base, _30554SavingPrivatePaios, 30554) \
	X(sauro_supply_base, _18910TheSauroSupplyBase, 18910) \
	X(sauro_supply_base, _28910AStabbingInSauro, 28910) \
	X(shugo_imperial_tomb, _80275EventEmpiresPast, 80275) \
	X(steel_rake, _3208ThePuzzlingBlueprint, 3208) \
	X(steel_rake, _3217ImprisonedGuardian, 3217) \
	X(steel_rake, _3219KeyItemHiddenQuest01, 3219) \
	X(steel_rake, _3220KeyItemHiddenQuest02, 3220) \
	X(steel_rake, _4208TruthOfTheBookmark, 4208) \
	X(steel_rake, _4217TheImprisonedExecutor, 4217) \
	X(steel_rake, _4219KeyItemHiddenQuest01, 4219) \
	X(steel_rake, _4220KeyItemHiddenQuest02, 4220) \
	X(talocs_hollow, _11465MysteriousSeed, 11465) \
	X(talocs_hollow, _11466AHardSeedtoCrack, 11466) \
	X(talocs_hollow, _11467DeathToTheQueen, 11467) \
	X(talocs_hollow, _11468WithFriendsLikeThese, 11468) \
	X(talocs_hollow, _21465MysteriousSeed, 21465) \
	X(talocs_hollow, _21467SpawningTheSapSuckers, 21467) \
	X(talocs_hollow, _21468TheStruggleWithin, 21468) \
	X(terath_dredgion, _30600FightOfTheNavigators, 30600) \
	X(terath_dredgion, _30610TheGoodNewsAndBad, 30610) \
	X(the_circle, _47100WardsAndWardOrbs, 47100) \
	X(the_circle, _47103AGlobeTrottingLesson, 47103) \
	X(the_circle, _47106TurningUpTheAmplifiers, 47106) \
	X(the_circle, _47107WardsAndWardOrbs, 47107) \
	X(the_circle, _47110AGlobeTrottingLesson, 47110) \
	X(the_circle, _47113TurningUpTheAmplifiers, 47113) \
	X(the_circle, _48007JoiningTheCircle, 48007) \
	X(the_eternal_bastion, _18035ShebasSurveillance, 18035) \
	X(the_eternal_bastion, _18036BastionsAreEternal, 18036) \
	X(the_eternal_bastion, _28035TrustInNoneButVerify, 28035) \
	X(the_eternal_bastion, _28036InterrogateKvash, 28036) \
	X(tiamat_stronghold, _30700RaceForTheRelics, 30700) \
	X(tiamat_stronghold, _30701TheLordOfIllusion, 30701) \
	X(tiamat_stronghold, _30708SuramaTheBetrayer, 30708) \
	X(tiamat_stronghold, _30709SoulSearching, 30709) \
	X(tiamat_stronghold, _30710TheGreatRelease, 30710) \
	X(tiamat_stronghold, _30722CheckTheGate, 30722) \
	X(tiamat_stronghold, _30750AttackOnTiamatStronghold, 30750) \
	X(tiamat_stronghold, _30751DeathToTheDragonLord, 30751) \
	X(tiamat_stronghold, _30758SuramaTheBitter, 30758) \
	X(tiamat_stronghold, _30759CountingStatues, 30759) \
	X(tiamat_stronghold, _30760PetrifiedHeroOfTheAsmodians, 30760) \
	X(tiamat_stronghold, _30772InvestigateTheGate, 30772) \
	X(udas_temple, _30003SecretOfTheUdasTemple, 30003) \
	X(udas_temple, _30005HealMeKillMe, 30005) \
	X(udas_temple, _30011Arachnophobia, 30011) \
	X(udas_temple, _30103LairOfTheDragonbound, 30103) \
	X(udas_temple, _30111CenterOfTheWeb, 30111) \
	X(wisplight_abbey, _19600WelcometoWisplightAbbey, 19600)
// clang-format on

// Lane C, phase 6 step 1 (2026-10-05): the out-of-tree golden sample (tools/gen/questgen/goldensample.py, phase6-transliterator.md §7) compiles
// this harness with AION_GOLDEN_SAMPLE_TABLE naming a header that defines AION_GOLDEN_SAMPLE_HANDLERS(X) with the staged generated handlers it
// adds, and AION_GOLDEN_EXPECTED_DIR naming a directory with their oracle documents. The tree's build defines neither: the table is the one
// above.
#ifdef AION_GOLDEN_SAMPLE_TABLE
#include AION_GOLDEN_SAMPLE_TABLE
#else
#define AION_GOLDEN_SAMPLE_HANDLERS(X)
#endif

#define AION_GOLDEN_DECLARE_FACTORY(dir, Class, questId)                                                                                       \
	namespace aion::gameserver::handlers::quest::dir {                                                                                           \
	::std::unique_ptr<::aion::gameserver::questEngine::handlers::AbstractQuestHandler> Class##_questFactory();                                  \
	}
AION_GOLDEN_GENERATED_HANDLERS(AION_GOLDEN_DECLARE_FACTORY)
AION_GOLDEN_SAMPLE_HANDLERS(AION_GOLDEN_DECLARE_FACTORY)
#undef AION_GOLDEN_DECLARE_FACTORY

namespace aion::gameserver::questEngine::handlers::test::golden {

/** The route's generated handlers kept out of the handler tree for their gate impact (see above) */
inline constexpr int32_t GOLDEN_HELD_BACK[] = {1131, 1146, 1152, 14010, 2207, 2209, 2213, 2221, 2223, 2231, 2232, 2239, 2288, 24010, 2911,
	2917, 2953, 29004, 29048, 4973};

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
	static const std::vector<GeneratedHandler> table{
		AION_GOLDEN_GENERATED_HANDLERS(AION_GOLDEN_ENTRY) AION_GOLDEN_SAMPLE_HANDLERS(AION_GOLDEN_ENTRY)};
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
