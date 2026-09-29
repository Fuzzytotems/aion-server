#pragma once

// P6-Q slice 2, chunk Q10 (2026-09-29): the vacuous cases of the generated altgard and pandaemonium handlers (GoldenQuestTraceTest.cpp,
// knownVacuous: a case with effects whose runs, under every overlay, change nothing observable). Each is a helper the path calls in a state the
// path itself fixes, in which the Java helper does nothing; the reason of each kind is spelled out in knownVacuous(). Written from the harness's
// own report of the first run with the 69 traces, each entry classified by its case's effects, status and dialog action (docs/deviations/Q10.md).
// The review of 2026-09-29 made 122 of the 162 observable (the free-dialog overlays, GoldenQuestTraceTest.cpp overlaysFor): every
// sendQuestStartDialog at USE_OBJECT (105, the kind IDLE_START, gone) and 17 sendQuestEndDialog cases whose dialog action the path leaves free.
// The integration of slice 2 held 16 of the chunk's generated handlers back (GoldenHandlers.h): their 11 rows left the list
// (2207 #9 #12 #16 #20, 2209 #18, 2231 #22 #23, 2911 #11, 2917 #8 #26 #27), measured with the files in; measure them again when they land.

#include <string_view>
#include <utility>

namespace aion::gameserver::questEngine::handlers::test::golden {

enum class Vacuous {
	IDLE_END,           // sendQuestEndDialog outside REWARD with a reward, select or SET_SUCCEED action
	STEP_NOT_MET,       // defaultCloseDialog at a var the path read that is not the helper's step
	KILLS_ASSUMED_FALSE // every kill helper of the path assumed false
};

// clang-format off
inline constexpr std::pair<std::string_view, Vacuous> Q10_VACUOUS[] = {
	{"2216 onDialogEvent#4", Vacuous::IDLE_END},
	{"2216 onDialogEvent#5", Vacuous::IDLE_END},
	{"2222 onDialogEvent#13", Vacuous::IDLE_END},
	{"2222 onDialogEvent#14", Vacuous::IDLE_END},
	{"2228 onDialogEvent#4", Vacuous::IDLE_END},
	{"2228 onDialogEvent#5", Vacuous::IDLE_END},
	{"2271 onDialogEvent#20", Vacuous::IDLE_END},
	{"2271 onDialogEvent#21", Vacuous::IDLE_END},
	{"2278 onDialogEvent#13", Vacuous::IDLE_END},
	{"2279 onDialogEvent#20", Vacuous::IDLE_END},
	{"2912 onDialogEvent#8", Vacuous::IDLE_END},
	{"2912 onDialogEvent#32", Vacuous::IDLE_END},
	{"2912 onDialogEvent#33", Vacuous::IDLE_END},
	{"2913 onDialogEvent#8", Vacuous::IDLE_END},
	{"2913 onDialogEvent#26", Vacuous::IDLE_END},
	{"2913 onDialogEvent#27", Vacuous::IDLE_END},
	{"2914 onDialogEvent#8", Vacuous::IDLE_END},
	{"2918 onDialogEvent#3", Vacuous::IDLE_END},
	{"2918 onDialogEvent#5", Vacuous::IDLE_END},
	{"2918 onDialogEvent#7", Vacuous::IDLE_END},
	{"2918 onDialogEvent#9", Vacuous::IDLE_END},
	{"2918 onDialogEvent#14", Vacuous::IDLE_END},
	{"2919 onDialogEvent#30", Vacuous::STEP_NOT_MET},
	{"2919 onDialogEvent#35", Vacuous::STEP_NOT_MET},
	{"2928 onDialogEvent#8", Vacuous::IDLE_END},
	{"2965 onDialogEvent#8", Vacuous::IDLE_END},
	{"2965 onDialogEvent#26", Vacuous::IDLE_END},
	{"2965 onDialogEvent#27", Vacuous::IDLE_END},
	{"4210 onKillEvent#3", Vacuous::KILLS_ASSUMED_FALSE},
};
// clang-format on

} // namespace aion::gameserver::questEngine::handlers::test::golden
