// PrologueSupport's pure checks (P6-Q prologue, the owner's answers 3 and 4 of 2026-09-29) on hand-built packets: the Java bursts of a new
// character's first enter world pass with no failure, and every assertion of expectPrologueMissionLocked, expectPrologueStarted and
// expectPrologueMovieEndAnswer fails, alone, on a burst that differs from Java in exactly the field that assertion checks. The gates only see
// what the server sends, so this is where each check is shown to be live (the Java review of 2026-09-29: no gate-level mutant reached the
// action type, quest and vars of the locked mission, the start's fields, the movie's object and skippable flag, the reward's exp, STR_GET_EXP2
// or the UPDATE COMPLETE fields).
//
// The bodies are written from the Java writeImpl as decoders/QuestDecodersTest.cpp writes them (m5a-plan.md D9); the quest, movie and mission
// ids are decoders::Prologue's, which QuestDecodersTest.ThePrologueQuestsAreTheJavaHandlers pins to the Java handlers.

#include <gtest/gtest-spi.h>
#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "AsyncAllowed.h"
#include "GameSession.h"
#include "PrologueSupport.h"
#include "decoders/PacketDecoders.h"
#include "decoders/QuestDecoders.h"

#include "NetworkTestSupport.h"

namespace aion::gameserver::scenario {
namespace {

using Packet = GameSession::Packet;
using network::test::PacketWriter;
using decoders::ASMODIAN_PROLOGUE;
using decoders::ELYOS_PROLOGUE;
using decoders::QUEST_STATUS_COMPLETE;
using decoders::QUEST_STATUS_LOCKED;
using decoders::QUEST_STATUS_REWARD;
using decoders::QUEST_STATUS_START;

Packet packet(std::string name, std::vector<uint8_t> data = {}) {
	Packet out;
	out.name = std::move(name);
	out.data = std::move(data);
	return out;
}

/** SM_QUEST_ACTION ADD (SM_QUEST_ACTION.java:72-80): action, quest, status, 0, vars, 0 short, 0 byte - 14 bytes */
Packet questAdd(int32_t questId, uint8_t status, int32_t vars = 0) {
	PacketWriter w;
	w.C(decoders::QUEST_ACTION_ADD).D(questId).C(status).C(0).D(vars).H(0).C(0);
	return packet("SM_QUEST_ACTION", w.data);
}

/** SM_QUEST_ACTION UPDATE (:72-73, :83-86): action, quest, status, 0, vars, 0 short - 13 bytes */
Packet questUpdate(int32_t questId, uint8_t status, int32_t vars = 0) {
	PacketWriter w;
	w.C(decoders::QUEST_ACTION_UPDATE).D(questId).C(status).C(0).D(vars).H(0);
	return packet("SM_QUEST_ACTION", w.data);
}

/** SM_QUEST_LIST (as VisibilityDecodersTest.cpp's questListBytes): 1, -count, then quest, status, vars, complete count per entry */
Packet questList(const std::vector<decoders::QuestEntry>& quests) {
	PacketWriter w;
	w.H(1);
	w.H(static_cast<int32_t>((0x10000u - quests.size()) & 0xFFFFu));
	for (const decoders::QuestEntry& quest : quests)
		w.D(quest.questId).C(quest.status).D(quest.questVarsAndFlags).C(quest.completeCount);
	return packet("SM_QUEST_LIST", w.data);
}

/** SM_PLAY_MOVIE (SM_PLAY_MOVIE.java:29-34) */
Packet playMovie(const decoders::PlayMovie& movie) {
	PacketWriter w;
	w.C(movie.cutsceneMovie ? 1 : 0).D(movie.objectId).D(movie.questId).D(movie.movieId).C(0).C(movie.canSkip ? 0 : 1);
	return packet("SM_PLAY_MOVIE", w.data);
}

/** SM_STATUPDATE_EXP (SM_STATUPDATE_EXP.java:36-40); level 1 needs 400 (player_experience_table.xml:4) */
Packet statUpdateExp(int64_t currentExp) {
	PacketWriter w;
	w.Q(currentExp).Q(0).Q(400).Q(0).Q(0);
	return packet("SM_STATUPDATE_EXP", w.data);
}

/** SM_SYSTEM_MESSAGE with one parameter, as PacketDecodersTest.cpp writes it */
Packet systemMessage(int32_t id) {
	PacketWriter w;
	w.C(0).C(0).D(0).D(id).C(1).S("1").C(0);
	return packet("SM_SYSTEM_MESSAGE", w.data);
}

/** SM_NEARBY_QUESTS with no marker (SM_NEARBY_QUESTS.java:22-31); the checks read only its position */
Packet nearbyQuests() {
	PacketWriter w;
	w.C(0).H(0);
	return packet("SM_NEARBY_QUESTS", w.data);
}

/** the Java movie of `prologue`: a skippable CutSceneMovie with no target (AbstractQuestHandler.java:665-667) */
decoders::PlayMovie javaMovie(const decoders::Prologue& prologue) {
	return decoders::PlayMovie{true, 0, prologue.quest, prologue.movie, true};
}

/** the first CM_ENTER_WORLD's burst around the level change and the quest list, as the gates' patterns have it (PrologueSupport.h) */
std::vector<Packet> enterBurst(Packet action, std::vector<Packet> lists) {
	std::vector<Packet> burst{packet("SM_STATS_INFO"), packet("SM_ACTION_ANIMATION"), std::move(action), nearbyQuests()};
	for (Packet& list : lists)
		burst.push_back(std::move(list));
	return burst;
}

std::vector<Packet> javaEnterBurst(const decoders::Prologue& prologue) {
	return enterBurst(questAdd(prologue.mission, QUEST_STATUS_LOCKED), {questList({{prologue.mission, QUEST_STATUS_LOCKED, 0, 0}})});
}

/** the first CM_LEVEL_READY's prologue part after the weather (PROLOGUE_LEVEL_READY) */
std::vector<Packet> levelReadyBurst(std::vector<Packet> actions, std::vector<decoders::PlayMovie> movies) {
	std::vector<Packet> burst{packet("SM_WEATHER")};
	for (Packet& action : actions)
		burst.push_back(std::move(action));
	burst.push_back(nearbyQuests());
	for (const decoders::PlayMovie& movie : movies)
		burst.push_back(playMovie(movie));
	burst.push_back(packet("SM_ABNORMAL_STATE"));
	return burst;
}

std::vector<Packet> javaLevelReadyBurst(const decoders::Prologue& prologue) {
	return levelReadyBurst({questAdd(prologue.quest, QUEST_STATUS_START)}, {javaMovie(prologue)});
}

constexpr int64_t EXP_BEFORE = 41;

/** CM_PLAY_MOVIE_END's answer (PROLOGUE_MOVIE_END) */
std::vector<Packet> javaMovieEndAnswer(const decoders::Prologue& prologue) {
	return {statUpdateExp(EXP_BEFORE + decoders::PROLOGUE_EXP), systemMessage(decoders::PROLOGUE_EXP_MESSAGE),
		questUpdate(prologue.quest, QUEST_STATUS_COMPLETE), nearbyQuests()};
}

/** runs `check` with gtest's reporter intercepted and returns the message of every failure it reported, fatal or not */
std::vector<std::string> failuresOf(const std::function<void()>& check) {
	testing::TestPartResultArray results;
	{
		testing::ScopedFakeTestPartResultReporter reporter(testing::ScopedFakeTestPartResultReporter::INTERCEPT_ONLY_CURRENT_THREAD, &results);
		check();
	}
	std::vector<std::string> messages;
	for (int i = 0; i < results.size(); i++)
		messages.emplace_back(results.GetTestPartResult(i).message());
	return messages;
}

std::string joined(const std::vector<std::string>& failures) {
	std::string out;
	for (const std::string& failure : failures)
		out += "\n  [" + failure + "]";
	return out.empty() ? " (none)" : out;
}

/** exactly one failure, and it is the check of `what` (the expression gtest prints, or the message of an ADD_FAILURE) */
void expectOnlyFailure(const std::vector<std::string>& failures, std::string_view what, std::string_view deviation) {
	ASSERT_EQ(failures.size(), 1u) << deviation << ": the failures were" << joined(failures);
	EXPECT_NE(failures[0].find(what), std::string::npos) << deviation << ": expected the check of " << what << ", got" << joined(failures);
}

// ---- expectPrologueMissionLocked ---------------------------------------------------------------------------------------------------------

TEST(PrologueSupportTest, TheJavaFirstEnterWorldHasTheMissionLocked) {
	for (const decoders::Prologue& prologue : {ELYOS_PROLOGUE, ASMODIAN_PROLOGUE}) {
		const std::vector<Packet> burst = javaEnterBurst(prologue);
		EXPECT_EQ(failuresOf([&] { expectPrologueMissionLocked(burst, prologue, "java"); }), std::vector<std::string>{}) << prologue.mission;
	}
}

TEST(PrologueSupportTest, EachMissionCheckFailsAloneOnItsField) {
	const decoders::Prologue& p = ELYOS_PROLOGUE;
	const Packet locked = questAdd(p.mission, QUEST_STATUS_LOCKED);
	const Packet list = questList({{p.mission, QUEST_STATUS_LOCKED, 0, 0}});
	const struct {
		std::string_view deviation;
		std::vector<Packet> burst;
		std::string_view what;
	} cases[] = {
		{"no SM_QUEST_ACTION", {packet("SM_STATS_INFO"), packet("SM_ACTION_ANIMATION"), nearbyQuests(), list}, "actions.size()"},
		{"two SM_QUEST_ACTION", {locked, locked, nearbyQuests(), list}, "actions.size()"},
		{"UPDATE instead of ADD", enterBurst(questUpdate(p.mission, QUEST_STATUS_LOCKED), {list}), "locked.actionType"},
		{"the prologue quest instead of the mission", enterBurst(questAdd(p.quest, QUEST_STATUS_LOCKED), {list}), "locked.questId"},
		{"START instead of LOCKED", enterBurst(questAdd(p.mission, QUEST_STATUS_START), {list}), "locked.status"},
		{"var 0 is 1", enterBurst(questAdd(p.mission, QUEST_STATUS_LOCKED, 1), {list}), "locked.questVarsAndFlags"},
		{"no SM_QUEST_LIST", enterBurst(locked, {}), "lists.size()"},
		{"two SM_QUEST_LIST", enterBurst(locked, {list, list}), "lists.size()"},
		{"an empty quest list", enterBurst(locked, {questList({})}), "decodeQuestList"},
		{"the listed mission START", enterBurst(locked, {questList({{p.mission, QUEST_STATUS_START, 0, 0}})}), "decodeQuestList"},
		{"the listed mission with var 0 = 1", enterBurst(locked, {questList({{p.mission, QUEST_STATUS_LOCKED, 1, 0}})}), "decodeQuestList"},
		{"the list holds the prologue quest too",
			enterBurst(locked, {questList({{p.mission, QUEST_STATUS_LOCKED, 0, 0}, {p.quest, QUEST_STATUS_START, 0, 0}})}), "decodeQuestList"},
	};
	for (const auto& c : cases)
		expectOnlyFailure(failuresOf([&] { expectPrologueMissionLocked(c.burst, p, "case"); }), c.what, c.deviation);
}

// ---- expectPrologueStarted ---------------------------------------------------------------------------------------------------------------

TEST(PrologueSupportTest, TheJavaFirstLevelReadyStartsTheQuestAndPlaysItsMovie) {
	for (const decoders::Prologue& prologue : {ELYOS_PROLOGUE, ASMODIAN_PROLOGUE}) {
		const std::vector<Packet> burst = javaLevelReadyBurst(prologue);
		std::optional<decoders::PlayMovie> movie;
		EXPECT_EQ(failuresOf([&] { movie = expectPrologueStarted(burst, prologue, "java"); }), std::vector<std::string>{}) << prologue.quest;
		EXPECT_EQ(movie, javaMovie(prologue)) << "endPrologue echoes this movie in CM_PLAY_MOVIE_END";
		// a weather change broadcast to the map (WeatherService.java:62, :178) may land anywhere in the burst too
		std::vector<Packet> withChange = burst;
		withChange.push_back(packet("SM_WEATHER"));
		EXPECT_EQ(failuresOf([&] { expectPrologueStarted(withChange, prologue, "java"); }), std::vector<std::string>{})
		  << prologue.quest << " with a weather change";
	}
}

TEST(PrologueSupportTest, EachStartCheckFailsAloneOnItsField) {
	const decoders::Prologue& p = ELYOS_PROLOGUE;
	const Packet started = questAdd(p.quest, QUEST_STATUS_START);
	const decoders::PlayMovie movie = javaMovie(p);
	const auto changed = [&movie](const std::function<void(decoders::PlayMovie&)>& change) {
		decoders::PlayMovie out = movie;
		change(out);
		return out;
	};
	// the Java burst with its SM_WEATHER (levelReadyBurst's first packet) moved to `at`, or dropped
	const auto weatherAt = [&](std::optional<size_t> at) {
		std::vector<Packet> burst = levelReadyBurst({started}, {movie});
		burst.erase(burst.begin());
		if (at)
			burst.insert(burst.begin() + static_cast<std::ptrdiff_t>(*at), packet("SM_WEATHER"));
		return burst;
	};
	const struct {
		std::string_view deviation;
		std::vector<Packet> burst;
		std::string_view what;
	} cases[] = {
		{"no SM_QUEST_ACTION", levelReadyBurst({}, {movie}), "actions.size()"},
		{"two SM_QUEST_ACTION", levelReadyBurst({started, started}, {movie}), "actions.size()"},
		{"UPDATE instead of ADD", levelReadyBurst({questUpdate(p.quest, QUEST_STATUS_START)}, {movie}), "started.actionType"},
		{"the mission instead of the prologue quest", levelReadyBurst({questAdd(p.mission, QUEST_STATUS_START)}, {movie}), "started.questId"},
		{"LOCKED instead of START", levelReadyBurst({questAdd(p.quest, QUEST_STATUS_LOCKED)}, {movie}), "started.status"},
		{"var 0 is 1", levelReadyBurst({questAdd(p.quest, QUEST_STATUS_START, 1)}, {movie}), "started.questVarsAndFlags"},
		{"the weather right after the start", weatherAt(1), "weather < start"},
		{"the weather after the movie", weatherAt(3), "weather < start"},
		{"no SM_WEATHER", weatherAt(std::nullopt), "weather < start"},
		{"no SM_PLAY_MOVIE", levelReadyBurst({started}, {}), "SM_PLAY_MOVIE in the level-ready burst, not one"},
		{"two SM_PLAY_MOVIE", levelReadyBurst({started}, {movie, movie}), "SM_PLAY_MOVIE in the level-ready burst, not one"},
		{"a CutScene", levelReadyBurst({started}, {changed([](auto& m) { m.cutsceneMovie = false; })}), "PlayMovie{"},
		{"a target", levelReadyBurst({started}, {changed([](auto& m) { m.objectId = 5; })}), "PlayMovie{"},
		{"the mission's movie", levelReadyBurst({started}, {changed([&p](auto& m) { m.questId = p.mission; })}), "PlayMovie{"},
		{"the other race's movie", levelReadyBurst({started}, {changed([](auto& m) { m.movieId = ASMODIAN_PROLOGUE.movie; })}), "PlayMovie{"},
		{"not skippable", levelReadyBurst({started}, {changed([](auto& m) { m.canSkip = false; })}), "PlayMovie{"},
	};
	for (const auto& c : cases)
		expectOnlyFailure(failuresOf([&] { expectPrologueStarted(c.burst, p, "case"); }), c.what, c.deviation);
}

// ---- expectPrologueMovieEndAnswer --------------------------------------------------------------------------------------------------------

TEST(PrologueSupportTest, TheJavaMovieEndPaysTheExpAndCompletesTheQuest) {
	const AsyncAllowed async = AsyncAllowed::m5aDefault();
	for (const decoders::Prologue& prologue : {ELYOS_PROLOGUE, ASMODIAN_PROLOGUE}) {
		const std::vector<Packet> answer = javaMovieEndAnswer(prologue);
		EXPECT_EQ(failuresOf([&] { expectPrologueMovieEndAnswer(answer, prologue, EXP_BEFORE, async, "java"); }), std::vector<std::string>{})
		  << prologue.quest;
		// the unconditional async packets of §5.9 may arrive anywhere in the burst
		std::vector<Packet> withPong = answer;
		withPong.insert(withPong.begin() + 2, packet("SM_PONG"));
		EXPECT_EQ(failuresOf([&] { expectPrologueMovieEndAnswer(withPong, prologue, EXP_BEFORE, async, "java"); }), std::vector<std::string>{})
		  << prologue.quest << " with an SM_PONG";
		// an object coming into view (a respawn: gs.scenario.m5b2 S6, 2026-09-30) is announced anywhere in the burst, also after its end
		for (const size_t at : {size_t{0}, size_t{2}, answer.size()}) {
			for (const std::string_view announcement : {"SM_NPC_INFO", "SM_GATHERABLE_INFO"}) {
				std::vector<Packet> withObject = answer;
				withObject.insert(withObject.begin() + static_cast<std::ptrdiff_t>(at), packet(std::string(announcement)));
				EXPECT_EQ(failuresOf([&] { expectPrologueMovieEndAnswer(withObject, prologue, EXP_BEFORE, async, "java"); }),
					std::vector<std::string>{})
				  << prologue.quest << " with an " << announcement << " at " << at;
			}
		}
	}
}

TEST(PrologueSupportTest, EachMovieEndCheckFailsAloneOnItsField) {
	const decoders::Prologue& p = ELYOS_PROLOGUE;
	const AsyncAllowed async = AsyncAllowed::m5aDefault();
	const std::vector<Packet> java = javaMovieEndAnswer(p);
	const auto with = [&java](size_t at, Packet replacement) {
		std::vector<Packet> out = java;
		out[at] = std::move(replacement);
		return out;
	};
	const struct {
		std::string_view deviation;
		std::vector<Packet> answer;
		std::string_view what;
	} cases[] = {
		{"no SM_NEARBY_QUESTS", {java[0], java[1], java[2]}, "result.matched"},
		{"the quest completes before the message", {java[0], java[2], java[1], java[3]}, "result.matched"},
		{"a packet no async rule allows", {java[0], java[1], packet("SM_STATS_INFO"), java[2], java[3]}, "result.matched"},
		{"the reward paid twice", with(0, statUpdateExp(EXP_BEFORE + 2 * decoders::PROLOGUE_EXP)), "decodeStatUpdateExp"},
		{"no exp paid", with(0, statUpdateExp(EXP_BEFORE)), "decodeStatUpdateExp"},
		{"STR_GET_EXP, the message with an npc name (SM_SYSTEM_MESSAGE.java:16349-16351)", with(1, systemMessage(1370000)),
			"decodeSystemMessageId"},
		{"ADD instead of UPDATE", with(2, questAdd(p.quest, QUEST_STATUS_COMPLETE)), "complete.actionType"},
		{"the mission completes", with(2, questUpdate(p.mission, QUEST_STATUS_COMPLETE)), "complete.questId"},
		{"REWARD instead of COMPLETE", with(2, questUpdate(p.quest, QUEST_STATUS_REWARD)), "complete.status"},
		{"var 0 is 1", with(2, questUpdate(p.quest, QUEST_STATUS_COMPLETE, 1)), "complete.questVarsAndFlags"},
	};
	for (const auto& c : cases)
		expectOnlyFailure(failuresOf([&] { expectPrologueMovieEndAnswer(c.answer, p, EXP_BEFORE, async, "case"); }), c.what, c.deviation);
}

} // namespace
} // namespace aion::gameserver::scenario
