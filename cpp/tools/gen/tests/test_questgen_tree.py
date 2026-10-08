"""The generated quest handlers committed to the handler tree (P6-Q ascension route, 2026-09-29, lane route-gen).

Every .cpp below cpp/game-server/handlers/aion/gameserver/handlers/quest that carries questgen's banner (emit.banner) is regenerated from the
Java file its banner names, with the driver's rules (emit.ALL_RULES), and must equal the committed file byte for byte: a hand edit, a stale
file after a generator change or a file emitted with other rules shows up here. The route's files are pinned by name, so a file that loses
its banner or goes missing fails too. Nothing is compiled; the C++ side is game-server/tests/quest_handlers_golden.
"""
from __future__ import annotations

import os
import re
import sys
import unittest
from pathlib import Path

sys.dont_write_bytecode = True
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from questgen import emit, paths  # noqa: E402

HANDLER_QUEST_DIR = paths.CPP_GAME_SERVER / 'handlers' / 'aion' / 'gameserver' / 'handlers' / 'quest'
HAVE_JAVA = paths.JAVA_QUEST_DIR.is_dir()

# the ascension route's generated files in the tree (docs/deviations/Q05.md, Q09.md, Q06.md "Generated dispatches"): poeta 11, ishalgen 13,
# ascension 12
ROUTE = {
    'poeta': ('_1000Prologue', '_1001TheKerubThreat', '_1003IllegalLogging', '_1004NeutralizingOdium', '_1005BarringtheGate',
              '_1100KaliosCall', '_1107TheLostAxe', '_1111InsomniaMedicine', '_1122DeliveringPernossRobe', '_1123WheresTutty',
              '_1205ANewSkill'),
    'ishalgen': ('_2000Prologue', '_2001ThinkingAhead', '_2003TreasureOfTheDeceased', '_2005TeachingaLesson', '_2006HitThemWhereitHurts',
                 '_2100OrderoftheCaptain', '_2106VanarsFlattery', '_2114TheInsectProblem', '_2122AshesToAshes',
                 '_2123TheImprisonedGourmet', '_2125TheRobberyPlot', '_2132ANewSkill', '_2135ForLoveofNegi'),
    'ascension': ('_1913DispatchtoVerteron', '_1914DispatchtoVerteron', '_1915DispatchtoVerteron', '_1916DispatchtoVerteron',
                  '_19070ADispatchtoVerteron', '_19071ADispatchtoVerteron', '_2901DispatchtoAltgard', '_2902DispatchtoAltgard',
                  '_2903DispatchtoAltgard', '_2904DispatchtoAltgard', '_29070ADispatchtoAltgard', '_29071ADispatchtoAltgard'),
}
# P6-Q slice 2, chunk Q10 (docs/deviations/Q10.md): the altgard and pandaemonium files questgen transliterates that are in the tree (18 + 35;
# questgen transliterates 28 + 41 of the 75, and the other six are hand ports, or held back, and carry no banner)
Q10 = {
    'altgard': (
        '_2216MuMuGrassKnot', '_2222ManirsMessage', '_2228AThornInItsSide', '_2247TheGergersDisguise', '_2263ShugoPotion',
        '_2266ATrustworthyMessenger', '_2271AurtrisLetter', '_2278ASecretProposal', '_2279SolidProof', '_2284EscapingAsmodae',
        '_2289RampagingMosbears', '_2290GrokensEscape', '_24011FunnyFloatingFungus', '_24012AnOminousCrop', '_24014StompOutThePlot',
        '_24015TotemPlowed', '_24016AStrangeNewThread', '_24112NoLaissezFaireForLepharists'),
    'pandaemonium': (
        '_2912FollowtheRibbon', '_2913AChainofDebt', '_2914ATokenofLostLove', '_2916ManInTheLongBlackRobe', '_2918DeepMaternalLove',
        '_2919BookOfOblivion', '_2920ElementaryMyDearDaeva', '_2921LoveAtFirstSight', '_2922FascinatingGift', '_2925AHeartfeltConfession',
        '_2928PowerofLove', '_2937UnexpectedReward', '_2938SecretLibraryAccess', '_2948HuronsLetter', '_2952WinningVindachinerksFavor',
        '_2954DeliveringOdellaJuice', '_2957FlowersForTheBanquet', '_2958LastMinuteWorries', '_2962JafnharWhereabouts', '_2963OnBehalfOfAFriend',
        '_2965AncientWeapons', '_2985AnExpertsReward', '_4210MissingHaorunerk', '_4905InterviewingTheVeterans', '_4906TalesOfHeroes',
        '_4920MakingTheActivatedSurkana', '_4966GrowthNinissFirstCharm', '_4967GrowthNinissSecondCharm', '_4968GrowthNinissThirdCharm',
        '_4969GrowthNinissFourthCharm', '_4970TheFashionistas', '_4971ProjectRunway', '_4972JudgeNot', '_4974TheSecretOfHisSuccess',
        '_4976ASettlerAmbition'),
}
# Phase 6 step 2, chunk Q08 (lane C, 2026-10-05; docs/deviations/Q08.md): the gelkmaros and enshar files questgen transliterates, all in the
# tree (45 + 19; gelkmaros/_20034RescuetheReians since the Q08 follow-up, row B39)
Q08 = {
    'gelkmaros': (
        '_20031GotoGelkmaros', '_20032AllAboutAbnormalAether', '_20033DranaSolution', '_20034RescuetheReians', '_20035SilenteraSupport',
        '_21004VillageStatusReport', '_21027FearlessKantele', '_21033ExorcisingInfisto', '_21036DeliveryofAetherSample',
        '_21051TroubleinStone', '_21052DragonHuntin', '_21053DramataDrama', '_21054MissionofDestiny', '_21056FundinOrders',
        '_21057FundinOldGrudge', '_21058KirhuaSpecialOrder', '_21059ShiningScroll', '_21060EliminatePadmarashka', '_21061NewOrder',
        '_21062TheDramataWrath', '_21063VanquishVeille', '_21068TheGameIsAfoot', '_21070TheSummation', '_21071MissingBard',
        '_21073ListentoMySongStrigiks', '_21075FatedHeartbreak', '_21080MessageInAWindstream', '_21081A_Helping_Hand',
        '_21105CoweringRefugee', '_21106TheRealRhonnam', '_21111TestYourMight', '_21114PoisonedFungi', '_21125MysteryBlueprint',
        '_21135VellunRequest', '_21136InSearchOfAWitness', '_21137BerokinImageMarble', '_21138OddStrigik', '_21217NewResearchPlan',
        '_21221RustyRelic', '_21244SearchForTheBiolab', '_21249TheInvincibleStarket', '_21296PadmarashkaLegacy',
        '_21455IngredientsForTheAntidote', '_21458PracticalResearch', '_21460AShulacksStory',),
    'enshar': (
        '_20500EnsharExpedition', '_20501WhattheRuinsSay', '_20502EvolvingMysteries', '_20503AncientEvilPlans', '_20504TiamatsShadow',
        '_20505AncientCrystal', '_20506MuscleOverMind', '_20507ItsWorseThanWeThought', '_25022SoupDeCure', '_25023SproutingDevelopments',
        '_25030CluesFromTheUndead', '_25031TheTejhiGhost', '_25032AvengeVarnur', '_25050TreasureInTheDeepSea',
        '_25051TreasureOfAncientKings', '_25052AnOfferingPeace', '_25062OminousAdvice', '_25070TruthOfTheCrystal',
        '_25073NoRevivalForTheBalaur',),
}
# Phase 6 step 2, chunk Q01 (lane C, 2026-10-05; docs/deviations/Q01.md): the reshanta files questgen transliterates, all in the tree (81 of
# 82: _2759TenaciousGuardian is refused; a hand port in the tree since the owner's decision of 2026-10-05)
Q01 = {
    'reshanta': (
        '_14040OrdersFromReshanta', '_14041AbyssalAbilities', '_14042ARescueOperation', '_14043DrawlingBalaur', '_14044ShardsOfMemory',
        '_14045RumorsOnWings', '_14046PiecingTheMemory', '_14047ChainingMemories', '_1701GovernorsDirective',
        '_1702Defeat9thRankAsmodianSoldiers', '_1703Defeat8thRankAsmodianSoldiers', '_1704Defeat7thRankAsmodianSoldiers',
        '_1705Defeat6thRankAsmodianSoldiers', '_1706Defeat5thRankAsmodianSoldiers', '_1707Defeat4thRankAsmodianSoldiers',
        '_1708Defeat3thRankAsmodianSoldiers', '_1709Defeat2thRankAsmodianSoldiers', '_1710Defeat1thRankAsmodianSoldiers',
        '_1718TradingDown', '_1719ConfrontAsmodianOfficers', '_1720ConfrontAsmodianGenerals', '_1721MeetingwiththeBrigadeGeneral',
        '_1722RastinsHomesickness', '_1724ReaperExpertise', '_1725CenturionsForgetfulness', '_1726ScoutingtheLake',
        '_1727RecruitsforNezekansShield', '_1761SohonerkWish', '_1777CalloftheGovernor', '_1798JakurerksShotattheBigTime',
        '_1799PupilsDiary', '_1800JaiorunerksTombstone', '_1845OpeningDoors', '_1846PaperTrail', '_1847AStrangeSoul',
        '_1851UnchartedIslands', '_1853OfficerOusting', '_1854GeneralPurge', '_24040VotansOrders', '_24041TrainingInTheAbyss',
        '_24042AReadyRescue', '_24043LazyLanguageLessons', '_24044ChangeTheFuture', '_24045ASpeedyErrand', '_24046TheShadowCalls', '_2701TheGovernorsSummons',
        '_2702Defeat9thRankElyosSoldiers', '_2703Defeat8thRankElyosSoldiers', '_2704Defeat7thRankElyosSoldiers',
        '_2705Defeat6thRankElyosSoldiers', '_2706Defeat5thRankElyosSoldiers', '_2707Defeat4thRankElyosSoldiers',
        '_2708Defeat3thRankElyosSoldiers', '_2709Defeat2thRankElyosSoldiers', '_2710Defeat1thRankElyosSoldiers', '_2718TradingDown',
        '_2719ChallengeElyosOfficers', '_2720ChallengeElyosGenerals', '_2721MeetingWithTheBrigadeGeneral', '_2722TheComfortsofHome',
        '_2724MissingInAction', '_2727TransparentMotives', '_2758CarryTheFlame', '_2767AFruitfulPartnership', '_2798SignontheDottedLine',
        '_2841CleansingtheAsteriaChamber', '_2842BalaurintheUndergroundFortress', '_2843OperationAnnihilate', '_2850OfficerObliteration',
        '_2851GeneralMassacre', '_3205FortheBlackCloudTraders', '_3701TeachThemaLesson', '_3702GeneralDestruction', '_3711ToKillACaptain',
        '_3712DredgionPrisonBreak', '_3718DredgingTheDredgion', '_4205SmackTheShulack', '_4702GeneralDeath', '_4711TheDredgionCaptain',
        '_4712EscapeFromTheDredgion', '_4718PressingTheAttack'),
}
# Phase 6 step 2, chunk Q02 (lane C, 2026-10-05; docs/deviations/Q02.md): the 59 inggison files, all transliterated and in the tree
Q02 = {
    'inggison': (
        '_10031ARiskfortheObelisk', '_10032HelpintheHollow', '_10033PetrifiedSubside', '_10034FoundUnderground', '_10035SoartotheCorridor',
        '_11000WisplightMoralTour', '_11001KindMeira', '_11003MaintainingtheIllusion', '_11005TheLimitsofGenius', '_11006TestingTheWaters',
        '_11008LetterOfEncouragement', '_11009MeiriaFriendlySuggestion', '_11010AngelToTheWounded', '_11012PracticalNursing',
        '_11026SolidEvidence', '_11031CanIEatIt', '_11032EverythingsBetterWithTentacles', '_11033YouMakeMeSick', '_11036UncommonRecipe',
        '_11040SquampOnTheCookingPlate', '_11046BoxPickedUpInTheForest', '_11053TheseShoesAreMadeForStalking', '_11056EliminationOrder',
        '_11057StanisSecretOrder', '_11058TemenosSecretOrder', '_11060TheOrbsOrders', '_11061TwilightOfRagnarok', '_11062PadmarashkaWrath',
        '_11063QuellMastarius', '_11068AMysteriousWind', '_11069MookieTravelTips', '_11070CraftyMessenger', '_11072DelusCulinaryVictim',
        '_11076ProofOfTalent', '_11077AWeaponOfWorth', '_11103FiniteWalk', '_11105WifesNagging', '_11106RewritingHistory',
        '_11107ComfortisaBox', '_11109TheNegotiators', '_11110KillingTime', '_11116MunchingMookiePickles', '_11117MedicationforSetzkiki',
        '_11118MakingSetzkikiLaugh', '_11123SuspiciousBook', '_11139TheBadNews', '_11143BabyShulackJourney', '_11147CuteBeadyEyes',
        '_11149TheLadyLayout', '_11212BalaurRecords', '_11227EasyAs', '_11228HeNeverReturned', '_11233SuleionTreasure',
        '_11289VeillesGift', '_11294SpawningInvestigation', '_11304TheRemainingFaithful', '_11455WhentheTimeisRipe', '_11458AdiassReport',
        '_11460TheShulackofTaloc'),
}
# Phase 6 step 2, chunk Q11 (lane C, 2026-10-08; docs/deviations/Q11.md): the files of daevanion and sanctum that questgen
# transliterates, in the tree but for the escort _3212 (71 of 74; 1990 and 1929 are refused for API gaps; docs/deviations/Q11.md)
Q11 = {
    'daevanion': (
        '_19631CoastalCrush', '_19632CascadeCritters', '_19633AlisaryAssistance', '_19634FurtherAidforAlisary', '_19635SouthernQuell',
        '_19636FinalStabilization', '_19637OnboardforOne', '_19638TroublewithTwos', '_19639TreesandThrees', '_19640FlyingthroughFour',
        '_19641FidgetyFives', '_19642SuccessforSix', '_1988AMeetingWithASage', '_1989ASagesTeachings', '_1993AnotherBeginning',
        '_1994ANewChoice', '_29631GlugGlugGlug', '_29632SweepingNahorLake', '_29633StabilizetheSaplands', '_29634ScaredSkurvs',
        '_29635BeachDay', '_29636BacktoSurt', '_29637TroubleNotTrivial', '_29638NotSoSweet', '_29639MonstersUnholy',
        '_29640FinalKrugClearing', '_29641MoveAlongNow', '_29642GoodOnGelkmaros', '_2988TheWiseInDisguise', '_2989CeremonyOfTheWise',
        '_2990MakingTheDaevanionWeapon', '_2993AnotherBeginning', '_2994ANewChoice', '_80291DurableDaevanionWeapon',
        '_80295DurableDaevanionWeapon',),
    'sanctum': (
        '_19004PeriklessInsight', '_1900RingImbuedAether', '_1901KrallicPotion', '_19047JustBetweenMeAndFasimedes',
        '_19048AndreasTeachings', '_1908UlaguruSpeaks', '_1909ASongOfPraise', '_1917ALingeringMystery', '_1918AnAxForNamus',
        '_1926SecretLibraryAccess', '_1928ChasingaCriminal', '_1932AMatterOfReputation', '_1935TissueIDontEvenKnowYou',
        '_1936WhatNerisonSaw', '_1937ALepharistMonstrosity', '_1938BlackCloudFakery', '_1940WingsofMastery', '_1947ALuckyDay',
        '_1948WheresVindachinerk', '_1963DeliveryfortheOuterPort', '_1964ASouvenirForNoris', '_1987ABiggerWarehouse',
        '_3210RescueHaorunerk', '_3908ToMastertheDragon', '_3913ASecretSummons',
        '_3920TheSecretOfSurkana', '_3961GrowthFlorasFirstCharm', '_3962GrowthFlorasSecondCharm', '_3963GrowthFlorasThirdCharm',
        '_3964GrowthFlorasFourthCharm', '_3965TotheGalleriaofGrandeur', '_3966SaluteANewUniform', '_3967AndusDyeBox',
        '_3968PalentinesRequest', '_3969SexiestManAlive', '_3970KinahDiggingDaughter',),
}
# Phase 6 step 2, chunk Q13 (lane C, 2026-10-07; docs/deviations/Q13.md): the files of the instance directories A-K that questgen
# transliterates, all in the tree (87 of 87; kaisinel_academy's three mentor dailies by rule stream-any-match)
Q13 = {
    'abyssal_splinter': (
        '_30255TheLastCrusade', '_30261WeirdFragment', '_30263DaevasFearToTread', '_30264ANecklacewithHistory',
        '_30265APolearmWalksintoaBar', '_30355TheProtectorsMadness', '_30361StrangeFragment', '_30363FoolsRushIn',
        '_30364RemembranceOfSpiritsPast', '_30365ARayOfHope',),
    'alabaster_order': (
        '_38000CallOfTheAlabasterOrder',),
    'aturam_sky_fortress': (
        '_18300FloatingDeath', '_18301MyPrecHious', '_18302FirstPriority', '_18303MakingASurCantA', '_28300FloatingDoom', '_28301PowerOn',
        '_28302DocumentSaved', '_28303JustAnIsland',),
    'bare_truth': (
        '_14030RetrievedMemory', '_14031AHyperVention',),
    'black_cloud_traders': (
        '_39505BackbitingBotheration', '_39510ZorinerkVersusTheShulacks', '_39515UntruthUpset', '_39520VilmanerkVersusDragonbound',),
    'blood_crusade': (
        '_48001CallOfTheCrusade',),
    'chantra_dredgion': (
        '_3721DisarmTheChantraDredgion', '_3722MyNewToy', '_3725MyLuckyNumber', '_4721RiseOfChantraDredgion', '_4722NewWeaponTest',
        '_4725CeaselessAttack',),
    'charlirunerks_daemons': (
        '_48002CharlirunerksDaemonsWantYou',),
    'clash_of_destiny': (
        '_24030ShowdownWithDestiny', '_24031EnemyAtTheDoorstep',),
    'danuar_sanctuary': (
        '_16985ChirTreasureRobbers', '_16987SeekOuttheCorridor', '_26985GraveyardTreasure', '_26987ExploretheElyosCorridor',),
    'empyrean_crucible': (
        '_18208IllusionOrInfiltration', '_18209ARiftInTheSpaceTwineContinuum', '_18212FirstBlood', '_18213TheChillingTruth',
        '_28208ARiftAdrift', '_28209CatchingTheRift', '_28212ATestOfBlood', '_28213TheColiseumSecret',),
    'esoterrace': (
        '_18400TheVanishings', '_18402GroupRootingOutCorruption', '_18405MemoriesInTheCornerOfHisMind', '_18406PlayingToTheHilt',
        '_18407GroupDrakanJournalism', '_18409GroupTiamatsPowerUnleashed', '_18410PursuingthePrisoners', '_28400InspecttheInspectors',
        '_28402GroupSavingDalia', '_28405KexkrasPast', '_28406FindersFee', '_28407GroupTheGathering', '_28409GroupMaketheBladeComplete',
        '_28410FortressUnsecured',),
    'fatebound_abbey': (
        '_29600WelcomeBack',),
    'fenris_fang': (
        '_29064FangOfConstruction', '_4937RecognitionOfThePreceptors', '_4938WorkOfTheFenrisFangs', '_4939ProvingGround',
        '_4940DecorationsofPandaemonium', '_4941GroupPandaemoniumHonors', '_4942ProvingProficiency', '_4943LuckandPersistence',
        '_4944LoyaltyAndAffableness',),
    'field_wardens': (
        '_48000SummonsFromTheWardens',),
    'fortuneers': (
        '_38002FortuneersCallToArms',),
    'greater_stigma': (
        '_30217GroupStigmasScars', '_30317GroupSpiritsandStigmaSlots',),
    'haramel': (
        '_18500BigKinah', '_18510MurderMyShugo', '_18511OutOfThePast', '_28500OdellaOdellaWhereArtThou',
        '_28510DestroytheHaramelFacilities', '_28511TheSoupNutsy',),
    'iron_wall_warfront': (
        '_16960FacetheCommander', '_26960FacetheCommander',),
    'kaisinel_academy': (
        '_37000ToxicInstruction', '_37003CamouflageKillers', '_37006NowYouSeeThem', '_38006MatriculationDay',),
}
# Phase 6 step 2, chunk Q14 (lane C, 2026-10-05; docs/deviations/Q14.md): the files of the instance directories K-W that questgen
# transliterates, all in the tree (74 of 95 at the landing; the owner's decisions of 2026-10-07 added the 15 mentor dailies, rule
# stream-any-match, and pangaea 14220/24220, rule constant-list: 91; the 4 still refused are not)
Q14 = {
    'kaldor': (
        '_13817TheFuryWithin', '_23817WeeklyFreeSpirit',),
    'kromedes_trial': (
        '_18604MeetingWithRotan', '_28604RecoveringRotan',),
    'levinshor': (
        '_13704FonasQuickFix', '_13708ProximityProtect', '_13745EljersRequest', '_23704LoudNoises', '_23708SoundtheAlarm',
        '_23745NoMoreinLevinshor',),
    'linkgate_foundry': (
        '_16940DiarySecrets', '_26940RaidtheLinkgateFoundry',),
    'marchutan_priory': (
        '_47000AltgardOrbIt', '_47003AGlobalProblem', '_47006AmplifiersWithIssues', '_48006TheMarchutanPrioryBeckons',),
    'miragent_holy_templar': (
        '_19064TemplarOfConstruction', '_3933ClassPreceptorConsent', '_3934TheQuestForTemplars', '_3935ShoulderTheBurden',
        '_3936DecorationsOfSanctum', '_3937GroupTheDecorationsofSanctum', '_3938WellRounded', '_3939PersistenceAndLuck', '_3940Loyalty',),
    'nightmare_circus': (
        '_80341EventAHallowedEve',),
    'orichalcum_key': (
        '_37100MutantNinjaIninas', '_37103CamoAndCarnage', '_37106AsmoHunt', '_37107CoolBlueWater', '_37110MyYoungApprentice',
        '_37113AsmoICU', '_38007AKeyMessage',),
    'pangaea': (
        '_14220NewZoneNewRules', '_24220WelcometoPanesterra',),
    'radiant_ops': (
        '_38001RadiantOpsRecruitment',),
    'rentus_base': (
        '_30500Desperation', '_30503RodelionRescue', '_30504TheSearchforPaios', '_30550MomentOfCrisis', '_30553ComradesInArms',
        '_30554SavingPrivatePaios',),
    'sauro_supply_base': (
        '_18910TheSauroSupplyBase', '_28910AStabbingInSauro',),
    'shugo_imperial_tomb': (
        '_80275EventEmpiresPast',),
    'steel_rake': (
        '_3208ThePuzzlingBlueprint', '_3217ImprisonedGuardian', '_3219KeyItemHiddenQuest01', '_3220KeyItemHiddenQuest02',
        '_4208TruthOfTheBookmark', '_4217TheImprisonedExecutor', '_4219KeyItemHiddenQuest01', '_4220KeyItemHiddenQuest02',),
    'talocs_hollow': (
        '_11465MysteriousSeed', '_11466AHardSeedtoCrack', '_11467DeathToTheQueen', '_11468WithFriendsLikeThese', '_21465MysteriousSeed',
        '_21467SpawningTheSapSuckers', '_21468TheStruggleWithin',),
    'terath_dredgion': (
        '_30600FightOfTheNavigators', '_30610TheGoodNewsAndBad',),
    'the_circle': (
        '_47100WardsAndWardOrbs', '_47103AGlobeTrottingLesson', '_47106TurningUpTheAmplifiers', '_47107WardsAndWardOrbs',
        '_47110AGlobeTrottingLesson', '_47113TurningUpTheAmplifiers', '_48007JoiningTheCircle',),
    'the_eternal_bastion': (
        '_18035ShebasSurveillance', '_18036BastionsAreEternal', '_28035TrustInNoneButVerify', '_28036InterrogateKvash',),
    'tiamat_stronghold': (
        '_30700RaceForTheRelics', '_30701TheLordOfIllusion', '_30708SuramaTheBetrayer', '_30709SoulSearching', '_30710TheGreatRelease',
        '_30722CheckTheGate', '_30750AttackOnTiamatStronghold', '_30751DeathToTheDragonLord', '_30758SuramaTheBitter',
        '_30759CountingStatues', '_30760PetrifiedHeroOfTheAsmodians', '_30772InvestigateTheGate',),
    'udas_temple': (
        '_30003SecretOfTheUdasTemple', '_30005HealMeKillMe', '_30011Arachnophobia', '_30103LairOfTheDragonbound', '_30111CenterOfTheWeb',),
    'wisplight_abbey': (
        '_19600WelcometoWisplightAbbey',),
}
# held back at the integration of slice 2 (docs/deviations/Q10.md, "Held back"): transliterated like the others, but kept out of the
# tree because gs.scenario.travel's (and gs.scenario.ascension's) Asmodian would see them: 24010's onEnterWorldEvent starts it at the
# Altgard arrival, and the others' start npcs put them in his SM_NEARBY_QUESTS in Pandaemonium or Altgard
Q10_HELD_BACK = (
    'altgard/_2207ConversingWithaSkurv.java', 'altgard/_2209TheScribbler.java', 'altgard/_2213PoisonRootPotentFruit.java',
    'altgard/_2221ManirsUncle.java', 'altgard/_2223AMythicalMonster.java', 'altgard/_2231SiblingRivalry.java', 'altgard/_2232TheBrokenHoneyJar.java',
    'altgard/_2239MalodorAntidote.java', 'altgard/_2288MoneyWhereYourMouthIs.java', 'altgard/_24010SuthransOrders.java',
    'pandaemonium/_2911SongOfBlessing.java', 'pandaemonium/_2917ArekedilsHeritage.java', 'pandaemonium/_2953DeliveringSupplyRequest.java',
    'pandaemonium/_29004VeldinaCall.java', 'pandaemonium/_29048SeriphimTeachings.java', 'pandaemonium/_4973MarraWorry.java',
)
# the four that start their quest at a character's first enter world (Java behaviour): held back for their gate impact until the owner's answer
# of 2026-09-29 ("A": land them and let gs.scenario.m5a, m5b and m5b2 expect the prologue traffic; docs/design/owner-decisions.md)
ENTER_WORLD = ('poeta/_1000Prologue.java', 'poeta/_1100KaliosCall.java', 'ishalgen/_2000Prologue.java', 'ishalgen/_2100OrderoftheCaptain.java')


def generated_files():
    """(tree file, Java file relative to data/handlers/quest) for every tree file with questgen's banner"""
    out = []
    for f in sorted(HANDLER_QUEST_DIR.rglob('*.cpp')):
        for line in f.read_text(encoding='utf-8').split('\n'):
            if line.startswith(emit.BANNER_FIRST_LINE):
                java = line[len(emit.BANNER_FIRST_LINE):].rstrip('.')
                out.append((f, java.removeprefix('game-server/data/handlers/quest/')))
                break
    return out


class Banner(unittest.TestCase):
    def test_the_banner_says_generated_and_compiled(self):
        lines = emit.banner('game-server/data/handlers/quest/poeta/_1001TheKerubThreat.java')
        self.assertEqual(lines[0], '// Generated by cpp/tools/gen/questgen from game-server/data/handlers/quest/poeta/_1001TheKerubThreat.java.')
        self.assertIn("Compiled into its Q chunk's handler library", lines[1])
        self.assertTrue(all(len(ln) <= 150 for ln in lines))          # .clang-format ColumnLimit
        self.assertNotIn('Not compiled', '\n'.join(lines))
        self.assertNotIn('(prototype)', '\n'.join(lines))


@unittest.skipUnless(HAVE_JAVA, 'the Java tree is not available')
class CommittedTree(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.files = generated_files()
        cls.tr = emit.Transliterator(rules=emit.ALL_RULES)

    def test_the_route_files_are_committed_with_the_banner(self):
        found = {(f.parent.name, f.stem) for f, _ in self.files}
        for directory, classes in ROUTE.items():
            for klass in classes:
                with self.subTest(file=f'{directory}/{klass}'):
                    self.assertIn((directory, klass), found)
        self.assertEqual(sum(len(v) for v in ROUTE.values()), 36)
        for directory, classes in Q10.items():
            for klass in classes:
                with self.subTest(file=f'{directory}/{klass}'):
                    self.assertIn((directory, klass), found)
        self.assertEqual((len(Q10['altgard']), len(Q10['pandaemonium'])), (18, 35))
        for directory, classes in Q08.items():
            for klass in classes:
                with self.subTest(file=f'{directory}/{klass}'):
                    self.assertIn((directory, klass), found)
        self.assertEqual((len(Q08['gelkmaros']), len(Q08['enshar'])), (45, 19))
        self.assertTrue((HANDLER_QUEST_DIR / 'gelkmaros' / '_20034RescuetheReians.cpp').exists())
        for klass in Q01['reshanta']:
            with self.subTest(file=f'reshanta/{klass}'):
                self.assertIn(('reshanta', klass), found)
        self.assertEqual(len(Q01['reshanta']), 81)
        # _2759 is a hand port since the owner's decision of 2026-10-05 (docs/deviations/Q01.md): in the tree, without questgen's banner
        hand_port = (HANDLER_QUEST_DIR / 'reshanta' / '_2759TenaciousGuardian.cpp').read_text(encoding='utf-8')
        self.assertNotIn(emit.banner('game-server/data/handlers/quest/reshanta/_2759TenaciousGuardian.java')[0], hand_port)
        self.assertNotIn(('reshanta', '_2759TenaciousGuardian'), found)
        for klass in Q02['inggison']:
            with self.subTest(file=f'inggison/{klass}'):
                self.assertIn(('inggison', klass), found)
        self.assertEqual(len(Q02['inggison']), 59)
        for directory, classes in Q14.items():
            for klass in classes:
                with self.subTest(file=f'{directory}/{klass}'):
                    self.assertIn((directory, klass), found)
        self.assertEqual(sum(len(classes) for classes in Q14.values()), 91)
        for directory, classes in Q13.items():
            for klass in classes:
                with self.subTest(file=f'{directory}/{klass}'):
                    self.assertIn((directory, klass), found)
        self.assertEqual(sum(len(classes) for classes in Q13.values()), 87)
        for directory, classes in Q11.items():
            for klass in classes:
                with self.subTest(file=f'{directory}/{klass}'):
                    self.assertIn((directory, klass), found)
        self.assertEqual(sum(len(classes) for classes in Q11.values()), 71)
        self.assertEqual(len(Q10_HELD_BACK), 16)

    def test_the_enter_world_files_are_in_the_tree(self):
        for rel in ENTER_WORLD:
            with self.subTest(file=rel):
                r = self.tr.transliterate(paths.JAVA_QUEST_DIR / rel)
                self.assertEqual(r.status, 'ok', r.reasons)
                self.assertIn('qe.registerOnEnterWorld(questId);', r.cpp)
                self.assertEqual((HANDLER_QUEST_DIR / rel).with_suffix('.cpp').read_bytes(), r.cpp.encode('utf-8'))

    def test_the_q10_held_back_files_transliterate_and_stay_out_of_the_tree(self):
        # the integration of slice 2: each is questgen's output (the regenerate command lands it), none is in the tree or the table
        found = {(f.parent.name, f.stem) for f, _ in self.files}
        for rel in Q10_HELD_BACK:
            with self.subTest(file=rel):
                r = self.tr.transliterate(paths.JAVA_QUEST_DIR / rel)
                self.assertEqual(r.status, 'ok', r.reasons)
                self.assertFalse((HANDLER_QUEST_DIR / rel).with_suffix('.cpp').exists())
                directory, klass = rel.removesuffix('.java').split('/')
                self.assertNotIn((directory, klass), found)
                self.assertNotIn(klass, Q10[directory])
        r = self.tr.transliterate(paths.JAVA_QUEST_DIR / 'altgard/_24010SuthransOrders.java')
        self.assertIn('qe.registerOnEnterWorld(questId);', r.cpp)

    def test_the_route_java_bugs_are_kept_and_marked(self):
        # phase6-inventory.md §11, the rows P6-Q added (the route-gen review): the marker sits right before the statement of the Java line
        # KNOWN_JAVA_BUGS names, and the code is kept as Java wrote it
        cases = (('poeta/_1004NeutralizingOdium.java', 69, 'var 4',
                  'else if (targetId == 700030 && var == 1 || var == 4) { // The Cauldron\n\t\t\t\t',
                  '\t\t\t\tswitch (dialogActionId) {\n'),
                 ('poeta/_1111InsomniaMedicine.java', 59, 'null-checked',
                  'else if (targetId == 203061) {\n\t\t\t',
                  '\t\t\tif (env.getDialogActionId() == QUEST_SELECT) {\n\t\t\t\tif (qs->getQuestVarById(0) == 0)'))
        for rel, line, words, before, after in cases:
            with self.subTest(file=rel):
                r = self.tr.transliterate(paths.JAVA_QUEST_DIR / rel)
                self.assertEqual(r.status, 'ok', r.reasons)
                self.assertEqual(len(r.java_bugs), 1)
                self.assertTrue(r.java_bugs[0].startswith(f'line {line}: ') and words in r.java_bugs[0], r.java_bugs)
                self.assertRegex(r.cpp, re.escape(before + emit.JAVA_BUG_MARK) + '[^\n]*\n' + re.escape(after))
                self.assertTrue(all(len(ln.expandtabs(2)) <= 150 for ln in r.cpp.split('\n') if emit.JAVA_BUG_MARK in ln))

    def test_the_slice_two_include_rules(self):
        # P6-Q slice 2 (Q10, docs/deviations/Q10.md "Generator changes"): the compile fixes of the altgard files. A Ref result the Java discards
        # is still destroyed by the caller, which needs the complete type (api.OWNING_RETURN_HEADERS); a pointer dereferenced for a T& parameter
        # needs its class's header (emit.convert)
        r = self.tr.transliterate(paths.JAVA_QUEST_DIR / 'altgard/_2213PoisonRootPotentFruit.java')
        self.assertEqual(r.status, 'ok', r.reasons)
        self.assertIn('\t\tSkillEngine::getInstance().applyEffectDirectly(255, *player, *player);', r.cpp)
        self.assertIn('#include "aion/gameserver/skillengine/model/Effect.h"\n', r.cpp)
        self.assertTrue((paths.CPP_GAME_SERVER / 'src' / 'aion/gameserver/skillengine/model/Effect.h').is_file())
        r = self.tr.transliterate(paths.JAVA_QUEST_DIR / 'altgard/_2223AMythicalMonster.java')
        self.assertEqual(r.status, 'ok', r.reasons)
        self.assertIn('spawnForFiveMinutes(211621, *env.getPlayer()->getWorldMapInstance(), ', r.cpp)
        self.assertIn('#include "aion/gameserver/world/WorldMapInstance.h"\n', r.cpp)
        # neither rule adds an include a file does not need: 2207 dereferences nothing and calls nothing that returns a Ref
        r = self.tr.transliterate(paths.JAVA_QUEST_DIR / 'altgard/_2207ConversingWithaSkurv.java')
        self.assertEqual(r.status, 'ok', r.reasons)
        self.assertNotIn('Effect.h', r.cpp)
        self.assertNotIn('WorldMapInstance.h', r.cpp)

    def test_every_generated_file_regenerates_byte_for_byte(self):
        self.assertTrue(self.files)
        for f, rel in self.files:
            with self.subTest(file=rel):
                r = self.tr.transliterate(paths.JAVA_QUEST_DIR / rel)
                self.assertEqual(r.status, 'ok', r.reasons)
                self.assertEqual(f.read_bytes(), r.cpp.encode('utf-8'), f'{f} differs from questgen output: regenerate it')
                self.assertTrue(r.cpp.rstrip('\n').endswith(f'}} // namespace aion::gameserver::handlers::quest::{f.parent.name}'))
                self.assertIn(f'AION_QUEST_HANDLER({f.stem}, ', r.cpp)


if __name__ == '__main__':
    unittest.main()
