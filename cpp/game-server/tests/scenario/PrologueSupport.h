#pragma once

// P6-Q prologue (the owner's answers 3 and 4 of 2026-09-29: land the four enter-world quest handlers and let the phase-5 gates expect their
// Java-faithful traffic): what a new character's first enter world in its start map brings since _1000Prologue, _1100KaliosCall,
// _2000Prologue and _2100OrderoftheCaptain register, checked the same way by every gate that creates one (decoders::Prologue has the Java):
//
// - the first CM_ENTER_WORLD: onLevelChange(0, 1) adds the start map's mission LOCKED (one SM_QUEST_ACTION between SM_ACTION_ANIMATION and
//   SM_NEARBY_QUESTS), and SM_QUEST_LIST holds it: expectPrologueMissionLocked;
// - the first CM_LEVEL_READY: the prologue quest starts and its movie plays (SM_QUEST_ACTION, SM_NEARBY_QUESTS and SM_PLAY_MOVIE between
//   the weather and SM_ABNORMAL_STATE); the client ends the movie as a real one does, which finishes the quest with its exp: endPrologue.
//   Until CM_PLAY_MOVIE_END the server drops every CM_MOVE (GameSession::buildCM_PLAY_MOVIE_END), so a gate that walks must end it first.
//
// Written from the Java only (m5a-plan.md D9): the decoders of decoders/QuestDecoders.h, the packet names of the §5.8 notation.

#include <cstdint>
#include <functional>
#include <optional>
#include <string_view>
#include <vector>

#include "AsyncAllowed.h"
#include "GameSession.h"
#include "decoders/QuestDecoders.h"

namespace aion::gameserver::scenario {

/** the §5.8 notation of the first enter world's level change, with the mission's SM_QUEST_ACTION */
inline constexpr std::string_view PROLOGUE_FIRST_ENTER_LEVEL_CHANGE = "SM_STATS_INFO, SM_ACTION_ANIMATION, SM_QUEST_ACTION, SM_NEARBY_QUESTS, ";
/** the §5.8 notation of the prologue's start in the first level-ready burst, after the weather */
inline constexpr std::string_view PROLOGUE_LEVEL_READY = "SM_QUEST_ACTION, SM_NEARBY_QUESTS, SM_PLAY_MOVIE, ";
/** the §5.8 notation of the burst CM_PLAY_MOVIE_END answers */
inline constexpr std::string_view PROLOGUE_MOVIE_END = "SM_STATUPDATE_EXP, SM_SYSTEM_MESSAGE, SM_QUEST_ACTION, SM_NEARBY_QUESTS";

/**
 * The quest half of a new character's first CM_ENTER_WORLD in its start map (_1100KaliosCall.java:64-67 / _2100OrderoftheCaptain.java:
 * 62-65, AbstractQuestHandler.java:988-1030): the burst's only SM_QUEST_ACTION adds the mission LOCKED with no var, and SM_QUEST_LIST
 * (PlayerEnterWorldService.java:238, the quests that are not COMPLETE) holds exactly that quest, never completed.
 */
void expectPrologueMissionLocked(const std::vector<GameSession::Packet>& enterBurst, const decoders::Prologue& prologue, std::string_view label);

/**
 * The quest half of the first CM_LEVEL_READY (_1000Prologue.java:26-36 / _2000Prologue.java:26-36): the level-ready burst's only
 * SM_QUEST_ACTION adds the prologue quest START with no var (QuestService.java:441), and its only SM_PLAY_MOVIE plays the movie as a
 * skippable CutSceneMovie with no target (AbstractQuestHandler.java:665-667). Returns that movie, or nothing when the burst holds no single
 * SM_PLAY_MOVIE (a failure already).
 */
std::optional<decoders::PlayMovie> expectPrologueStarted(const std::vector<GameSession::Packet>& levelReadyBurst,
	const decoders::Prologue& prologue, std::string_view label);

/**
 * The answer to CM_PLAY_MOVIE_END (_1000Prologue.java:38-48, QuestService.finishQuest, QuestService.java:77-118): SM_STATUPDATE_EXP with
 * `expBefore` plus the reward's exp, STR_GET_EXP2 (PlayerCommonData.addExp with no npc name), SM_QUEST_ACTION UPDATE COMPLETE with no var and
 * SM_NEARBY_QUESTS, in that order, with nothing else but what `async` allows.
 */
void expectPrologueMovieEndAnswer(const std::vector<GameSession::Packet>& ended, const decoders::Prologue& prologue, int64_t expBefore,
	const AsyncAllowed& async, std::string_view label);

/**
 * The quest half of the first CM_LEVEL_READY and the end of the prologue's movie (_1000Prologue.java:26-48 / _2000Prologue.java:26-48):
 * expectPrologueStarted on the level-ready burst; then the client ends the movie as a real client does, echoing the movie's fields in
 * CM_PLAY_MOVIE_END, `collect` reads the answer and expectPrologueMovieEndAnswer checks it. Returns that burst (nothing when there was no
 * movie to end). The two checks are pure, so PrologueSupportTest proves each of their assertions on hand-built packets.
 */
std::vector<GameSession::Packet> endPrologue(GameSession& session, const std::vector<GameSession::Packet>& levelReadyBurst,
	const decoders::Prologue& prologue, int64_t expBefore, const AsyncAllowed& async,
	const std::function<std::vector<GameSession::Packet>()>& collect, std::string_view label);

} // namespace aion::gameserver::scenario
