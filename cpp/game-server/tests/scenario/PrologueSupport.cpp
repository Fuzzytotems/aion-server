#include "PrologueSupport.h"

#include <string>

#include <gtest/gtest.h>

#include "PacketSequence.h"
#include "decoders/PacketDecoders.h"

namespace aion::gameserver::scenario {
namespace {

using Packet = GameSession::Packet;

std::vector<Packet> named(const std::vector<Packet>& packets, std::string_view name) {
	std::vector<Packet> out;
	for (const Packet& packet : packets)
		if (packet.name == name)
			out.push_back(packet);
	return out;
}

std::string namesOf(const std::vector<Packet>& packets) {
	std::string out;
	for (const Packet& packet : packets)
		out += (out.empty() ? "" : ", ") + packet.name;
	return out;
}

} // namespace

void expectPrologueMissionLocked(const std::vector<Packet>& enterBurst, const decoders::Prologue& prologue, std::string_view label) {
	const std::vector<Packet> actions = named(enterBurst, "SM_QUEST_ACTION");
	ASSERT_EQ(actions.size(), 1u) << label << ": the enter-world burst's SM_QUEST_ACTION: " << namesOf(enterBurst);
	const decoders::QuestAction locked = decoders::decodeQuestAction(actions[0].data);
	EXPECT_EQ(locked.actionType, decoders::QUEST_ACTION_ADD) << label << ": QuestService.addOrUpdateQuest of a new quest is an ADD";
	EXPECT_EQ(locked.questId, prologue.mission) << label;
	EXPECT_EQ(locked.status, decoders::QUEST_STATUS_LOCKED) << label << ": minlevel_permitted 3 is within two levels of level 1";
	EXPECT_EQ(locked.questVarsAndFlags, 0) << label;
	const std::vector<Packet> lists = named(enterBurst, "SM_QUEST_LIST");
	ASSERT_EQ(lists.size(), 1u) << label;
	const std::vector<decoders::QuestEntry> expected{{prologue.mission, decoders::QUEST_STATUS_LOCKED, 0, 0}};
	EXPECT_EQ(decoders::decodeQuestList(lists[0].data).quests, expected) << label << ": SM_QUEST_LIST holds the locked mission alone";
}

std::optional<decoders::PlayMovie> expectPrologueStarted(const std::vector<Packet>& levelReadyBurst, const decoders::Prologue& prologue,
	std::string_view label) {
	const std::vector<Packet> actions = named(levelReadyBurst, "SM_QUEST_ACTION");
	EXPECT_EQ(actions.size(), 1u) << label << ": the level-ready burst's SM_QUEST_ACTION: " << namesOf(levelReadyBurst);
	if (!actions.empty()) {
		const decoders::QuestAction started = decoders::decodeQuestAction(actions[0].data);
		EXPECT_EQ(started.actionType, decoders::QUEST_ACTION_ADD) << label;
		EXPECT_EQ(started.questId, prologue.quest) << label;
		EXPECT_EQ(started.status, decoders::QUEST_STATUS_START) << label;
		EXPECT_EQ(started.questVarsAndFlags, 0) << label;
	}
	const std::vector<Packet> movies = named(levelReadyBurst, "SM_PLAY_MOVIE");
	if (movies.size() != 1u) {
		ADD_FAILURE() << label << ": " << movies.size() << " SM_PLAY_MOVIE in the level-ready burst, not one: " << namesOf(levelReadyBurst);
		return std::nullopt;
	}
	const decoders::PlayMovie movie = decoders::decodePlayMovie(movies[0].data);
	EXPECT_EQ(movie, (decoders::PlayMovie{true, 0, prologue.quest, prologue.movie, true}))
	  << label << ": movie " << movie.movieId << " of quest " << movie.questId << " (type " << movie.cutsceneMovie << ", object " << movie.objectId
	  << ", skippable " << movie.canSkip << ")";
	return movie;
}

void expectPrologueMovieEndAnswer(const std::vector<Packet>& ended, const decoders::Prologue& prologue, int64_t expBefore,
	const AsyncAllowed& async, std::string_view label) {
	std::vector<std::string> names;
	for (const Packet& packet : ended)
		names.push_back(packet.name);
	const PacketSequence sequence = PacketSequence::parse(PROLOGUE_MOVIE_END);
	const PacketSequence::Result result = sequence.match(names, async.predicate(ended));
	EXPECT_TRUE(result.matched) << label << ": " << result.message << "\n  expected: " << sequence.toString() << "\n  got (" << names.size()
	                            << "): " << namesOf(ended);
	const std::vector<Packet> exp = named(ended, "SM_STATUPDATE_EXP");
	if (!exp.empty())
		EXPECT_EQ(decoders::decodeStatUpdateExp(exp[0].data).currentExp, expBefore + decoders::PROLOGUE_EXP) << label << ": the reward's exp";
	const std::vector<Packet> messages = named(ended, "SM_SYSTEM_MESSAGE");
	if (!messages.empty())
		EXPECT_EQ(decoders::decodeSystemMessageId(messages[0].data), decoders::PROLOGUE_EXP_MESSAGE) << label;
	const std::vector<Packet> finished = named(ended, "SM_QUEST_ACTION");
	if (!finished.empty()) {
		const decoders::QuestAction complete = decoders::decodeQuestAction(finished[0].data);
		EXPECT_EQ(complete.actionType, decoders::QUEST_ACTION_UPDATE) << label;
		EXPECT_EQ(complete.questId, prologue.quest) << label;
		EXPECT_EQ(complete.status, decoders::QUEST_STATUS_COMPLETE) << label;
		EXPECT_EQ(complete.questVarsAndFlags, 0) << label;
	}
}

std::vector<Packet> endPrologue(GameSession& session, const std::vector<Packet>& levelReadyBurst, const decoders::Prologue& prologue,
	int64_t expBefore, const AsyncAllowed& async, const std::function<std::vector<Packet>()>& collect, std::string_view label) {
	const std::optional<decoders::PlayMovie> movie = expectPrologueStarted(levelReadyBurst, prologue, label);
	if (!movie)
		return {};
	session.send(GameSession::CM_PLAY_MOVIE_END,
		GameSession::buildCM_PLAY_MOVIE_END(movie->cutsceneMovie ? 1 : 0, movie->objectId, movie->questId, movie->movieId));
	std::vector<Packet> ended = collect();
	expectPrologueMovieEndAnswer(ended, prologue, expBefore, async, label);
	return ended;
}

} // namespace aion::gameserver::scenario
