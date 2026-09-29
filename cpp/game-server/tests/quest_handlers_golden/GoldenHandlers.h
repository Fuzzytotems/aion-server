#pragma once

// P6-Q ascension route (lane route-gen, 2026-09-29): the generated quest handlers the golden trace harness drives (phase6-inventory.md §7.6
// item 3). Each AION_QUEST_HANDLER marker defines `<Class>_questFactory()` in the namespace of its directory (HandlerRegistry.h); this table
// names them. The poeta handlers come from Q05's own library (aion_gs_handlers_quest_q05, linked into this executable); a handler target's
// tests link only their own library, so the ishalgen (Q09) and ascension (Q06) handlers are compiled into this executable by #include
// (GoldenIshalgenHandlers.cpp, GoldenAscensionHandlers.cpp), the way P5-05's tests compile the two quest npc AIs (chunks.cmake, the P5-05 LEASE
// row), and so are the altgard and pandaemonium handlers of P6-Q slice 2 (Q10: GoldenAltgardHandlers.cpp, GoldenPandaemoniumHandlers.cpp; the
// five hand ports and the held-back 4212 are not generated, docs/deviations/Q10.md). The generated files themselves are never edited here.
//
// Held back (GOLDEN_HELD_BACK): 1000, 2000, 1100 and 2100 register onEnterWorld and start their quest at a character's first enter world
// (Java behaviour), which turns gs.scenario.m5a, m5b and m5b2 red (docs/deviations/Q05.md, "Gate impact"). Their generated files are not in
// the handler tree until the gates' owners decide; the harness passed all 30 of their cases with them in (the bytes the generator emits).

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
	X(altgard, _2207ConversingWithaSkurv, 2207) \
	X(altgard, _2209TheScribbler, 2209) \
	X(altgard, _2213PoisonRootPotentFruit, 2213) \
	X(altgard, _2216MuMuGrassKnot, 2216) \
	X(altgard, _2221ManirsUncle, 2221) \
	X(altgard, _2222ManirsMessage, 2222) \
	X(altgard, _2223AMythicalMonster, 2223) \
	X(altgard, _2228AThornInItsSide, 2228) \
	X(altgard, _2231SiblingRivalry, 2231) \
	X(altgard, _2232TheBrokenHoneyJar, 2232) \
	X(altgard, _2239MalodorAntidote, 2239) \
	X(altgard, _2247TheGergersDisguise, 2247) \
	X(altgard, _2263ShugoPotion, 2263) \
	X(altgard, _2266ATrustworthyMessenger, 2266) \
	X(altgard, _2271AurtrisLetter, 2271) \
	X(altgard, _2278ASecretProposal, 2278) \
	X(altgard, _2279SolidProof, 2279) \
	X(altgard, _2284EscapingAsmodae, 2284) \
	X(altgard, _2288MoneyWhereYourMouthIs, 2288) \
	X(altgard, _2289RampagingMosbears, 2289) \
	X(altgard, _2290GrokensEscape, 2290) \
	X(altgard, _24010SuthransOrders, 24010) \
	X(altgard, _24011FunnyFloatingFungus, 24011) \
	X(altgard, _24012AnOminousCrop, 24012) \
	X(altgard, _24014StompOutThePlot, 24014) \
	X(altgard, _24015TotemPlowed, 24015) \
	X(altgard, _24016AStrangeNewThread, 24016) \
	X(altgard, _24112NoLaissezFaireForLepharists, 24112) \
	X(pandaemonium, _29004VeldinaCall, 29004) \
	X(pandaemonium, _29048SeriphimTeachings, 29048) \
	X(pandaemonium, _2911SongOfBlessing, 2911) \
	X(pandaemonium, _2912FollowtheRibbon, 2912) \
	X(pandaemonium, _2913AChainofDebt, 2913) \
	X(pandaemonium, _2914ATokenofLostLove, 2914) \
	X(pandaemonium, _2916ManInTheLongBlackRobe, 2916) \
	X(pandaemonium, _2917ArekedilsHeritage, 2917) \
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
	X(pandaemonium, _2953DeliveringSupplyRequest, 2953) \
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
	X(pandaemonium, _4973MarraWorry, 4973) \
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
inline constexpr int32_t GOLDEN_HELD_BACK[] = {1000, 1100, 2000, 2100};

struct GeneratedHandler {
	std::string_view directory;
	std::string_view javaClass;
	int32_t questId;
	std::unique_ptr<AbstractQuestHandler> (*factory)();
};

/** Every generated handler of the table, in the table's order (poeta, ishalgen, ascension, altgard, pandaemonium; file name order inside a
 * directory) */
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
