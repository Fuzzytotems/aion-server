// GameSession::talk (m5d-plan.md G-02) against a fake server socket (FakeServerSocket.h), like GameSessionFightTest.cpp: what goes over the
// wire - one CM_DIALOG_SELECT with the npc, the action and the quest in CM_DIALOG_SELECT.readImpl's order - and which server packets the call
// returns: the burst that follows it, up to the first gap of `quiet`, and nothing recorded before it. No game server is involved.

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <optional>
#include <string>
#include <thread>
#include <vector>

#include "FakeServerSocket.h"
#include "GameSession.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::scenario {
namespace {

using namespace std::chrono_literals;
using fake::ClientPacket;
using network::test::PacketReader;
using network::test::PacketWriter;
using TalkFixture = fake::SessionFixture;

/** the Java opcodes of the answer to a quest accept (ServerPacketsOpcodes.java:78, 142, 145); GameSessionTest.ServerPacketNames pins the names */
constexpr int32_t SM_DIALOG_WINDOW = 60;
constexpr int32_t SM_QUEST_ACTION = 124;
constexpr int32_t SM_NEARBY_QUESTS = 127;

/** DialogAction.QUEST_ACCEPT_1 and SELECTED_QUEST_AUTO_REWARD (DialogAction.java:143, 123) */
constexpr uint16_t QUEST_ACCEPT_1 = 1002;
constexpr uint16_t SELECTED_QUEST_AUTO_REWARD = 108;

TEST(GameSessionTalkTest, TalkSendsOneDialogSelectAndReturnsTheBurstThatAnswersIt) {
	TalkFixture fixture;
	// a packet the session read before the talk is not part of its answer
	fixture.server.sendServerPacket(SM_NEARBY_QUESTS, PacketWriter().C(0).H(0).data);
	ASSERT_TRUE(fixture.session->next(5s).has_value());
	const size_t before = fixture.session->recorded().size();

	// the server answers after it read the CM_DIALOG_SELECT: two packets at once, one 150 ms later (inside the quiet period), one after a gap
	// longer than the quiet period (the next burst)
	std::optional<ClientPacket> received;
	std::thread server([&] {
		received = fixture.server.receive(5s);
		fixture.server.sendServerPacket(SM_QUEST_ACTION, PacketWriter().D(1).data);
		fixture.server.sendServerPacket(SM_NEARBY_QUESTS, PacketWriter().D(2).data);
		std::this_thread::sleep_for(150ms);
		fixture.server.sendServerPacket(SM_DIALOG_WINDOW, PacketWriter().D(3).data);
		std::this_thread::sleep_for(1200ms);
		fixture.server.sendServerPacket(SM_DIALOG_WINDOW, PacketWriter().D(4).data);
	});
	const auto start = std::chrono::steady_clock::now();
	const GameSession::TalkOutcome outcome = fixture.session->talk(0x400B0010, QUEST_ACCEPT_1, 1101, 400ms, 10s);
	const auto elapsed = std::chrono::steady_clock::now() - start;
	server.join();

	ASSERT_TRUE(received.has_value()) << "talk sent nothing";
	EXPECT_EQ(received->opcode, GameSession::CM_DIALOG_SELECT);
	PacketReader body(received->data);
	EXPECT_EQ(body.D(), 0x400B0010) << "targetObjectId";
	EXPECT_EQ(static_cast<uint16_t>(body.H()), QUEST_ACCEPT_1) << "dialogActionId";
	EXPECT_EQ(body.H(), 0) << "extendedRewardIndex";
	EXPECT_EQ(body.H(), 0) << "lastPage";
	EXPECT_EQ(body.D(), 1101) << "questId";
	EXPECT_EQ(body.H(), 0) << "the 4.7 short";
	EXPECT_EQ(body.remaining(), 0u);

	EXPECT_FALSE(outcome.closed);
	EXPECT_EQ(outcome.firstPacket, before);
	ASSERT_EQ(outcome.packets.size(), 3u) << "the burst ends at the first gap of `quiet`";
	EXPECT_EQ(outcome.packets[0].name, "SM_QUEST_ACTION");
	EXPECT_EQ(outcome.packets[1].name, "SM_NEARBY_QUESTS");
	EXPECT_EQ(outcome.packets[2].name, "SM_DIALOG_WINDOW");
	EXPECT_EQ(outcome.packets[2].data, PacketWriter().D(3).data);
	ASSERT_EQ(fixture.session->recorded().size(), before + 3) << "the packets are recorded like every other server packet";
	EXPECT_EQ(fixture.session->recorded()[before].data, outcome.packets[0].data);
	EXPECT_GE(elapsed, 150ms + 400ms - 50ms) << "talk waited out the quiet period after the last packet";
	EXPECT_LT(elapsed, 1200ms) << "the packet after the gap is not waited for";

	const std::optional<GameSession::Packet> later = fixture.session->next(5s);
	ASSERT_TRUE(later.has_value());
	EXPECT_EQ(later->data, PacketWriter().D(4).data) << "the next burst is left for the next read";
}

TEST(GameSessionTalkTest, TalkStopsASteadyStreamAtItsLimit) {
	// a stream with no gap of `quiet` - one packet every 100 ms, `quiet` 800 ms - ends only at `limit` (2 s): not at `quiet`, and not when the
	// stream ends (after 5 s at the latest; the server stops early once talk has returned)
	TalkFixture fixture;
	std::atomic<bool> stop = false;
	std::thread server([&] {
		if (!fixture.server.receive(5s))
			return;
		for (int32_t i = 0; i < 50 && !stop; i++) {
			fixture.server.sendServerPacket(SM_NEARBY_QUESTS, PacketWriter().D(i).data);
			std::this_thread::sleep_for(100ms);
		}
	});
	const auto start = std::chrono::steady_clock::now();
	const GameSession::TalkOutcome outcome = fixture.session->talk(0x400B0010, QUEST_ACCEPT_1, 1101, 800ms, 2s);
	const auto elapsed = std::chrono::steady_clock::now() - start;
	stop = true;
	server.join();

	EXPECT_FALSE(outcome.closed);
	EXPECT_GE(elapsed, 2s - 50ms) << "the call ran to `limit`, not to the first `quiet`";
	EXPECT_LT(elapsed, 3500ms) << "the call stopped at `limit`, not at the end of the stream";
	ASSERT_FALSE(outcome.packets.empty());
	EXPECT_LT(outcome.packets.size(), 50u);
	for (size_t i = 0; i < outcome.packets.size(); i++)
		EXPECT_EQ(outcome.packets[i].data, PacketWriter().D(static_cast<int32_t>(i)).data) << "packet " << i << " in arrival order";
}

TEST(GameSessionTalkTest, TheQuestJournalTalksWithTargetZero) {
	// C10b: the journal's `CM_DIALOG_SELECT(target 0, 108 = SELECTED_QUEST_AUTO_REWARD, 1102)` (CM_DIALOG_SELECT.java:75-100)
	TalkFixture fixture;
	const GameSession::TalkOutcome outcome = fixture.session->talk(0, SELECTED_QUEST_AUTO_REWARD, 1102, 200ms);
	const std::optional<ClientPacket> received = fixture.server.receive(5s);
	ASSERT_TRUE(received.has_value());
	EXPECT_EQ(received->opcode, GameSession::CM_DIALOG_SELECT);
	EXPECT_EQ(received->data, GameSession::buildCM_DIALOG_SELECT(0, SELECTED_QUEST_AUTO_REWARD, 0, 0, 1102));
	EXPECT_TRUE(outcome.packets.empty()) << "a silent server answers nothing";
	EXPECT_EQ(outcome.firstPacket, 0u);
	EXPECT_FALSE(outcome.closed);
}

TEST(GameSessionTalkTest, TalkNoticesAClosedConnection) {
	TalkFixture fixture;
	fixture.server.close();
	const auto start = std::chrono::steady_clock::now();
	const GameSession::TalkOutcome outcome = fixture.session->talk(7, QUEST_ACCEPT_1, 1101, 10s, 20s);
	EXPECT_TRUE(outcome.closed);
	EXPECT_TRUE(outcome.packets.empty());
	EXPECT_LT(std::chrono::steady_clock::now() - start, 5s) << "a closed connection is not waited out";
}

} // namespace
} // namespace aion::gameserver::scenario
