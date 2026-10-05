// ChatBanService (P5-08; m5g-plan.md W-03): the in-memory chat bans, their GAG task and their minutes. Java: ChatBanService.java:26-75.
//
// The fixture is TravelTestSupport.h (a connected Elyos in Poeta) on its DeterministicExecutor, so the GAG task runs when the cases advance
// the executor. The ban's expiry itself is wall-clock time (System.currentTimeMillis, getBanMinutes), so the minutes are asserted with a margin
// a case cannot cross. ChatServer.sendPlayerGagPacket is a no-op without a chat server link (ChatServer.cpp: upConnection()).
// The ban map is static and keyed by the player's object id; every case runs in a process of its own under ctest.

#include "TravelTestSupport.h"

#include <chrono>
#include <cstdint>
#include <thread>
#include <vector>

#include "aion/gameserver/controllers/PlayerController.h"
#include "aion/gameserver/model/TaskId.h"
#include "aion/gameserver/runtime/sched/DeterministicExecutor.h"
#include "aion/gameserver/services/ban/ChatBanService.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::services::teleport::test {
namespace {

using ban::ChatBanService;
using network::test::PacketWriter;

/** ServerPacketsOpcodes.java:43; SM_SYSTEM_MESSAGE.STR_CAN_CHAT_NOW (SM_SYSTEM_MESSAGE.java:12317-12318) */
constexpr int32_t SM_SYSTEM_MESSAGE_OPCODE = 25;
constexpr int32_t STR_CAN_CHAT_NOW = 1300644;

class ChatBanServiceTest : public TravelTest {
protected:
	static void advance(int64_t millis) {
		dynamic_cast<runtime::DeterministicExecutor&>(*utils::ThreadPoolManager::installedBackend()).advance(std::chrono::milliseconds(millis));
	}

	static std::vector<uint8_t> canChatNow() {
		return javaPacket(SM_SYSTEM_MESSAGE_OPCODE, PacketWriter().C(25).C(0).D(0).D(STR_CAN_CHAT_NOW).C(0).C(0));
	}
};

TEST_F(ChatBanServiceTest, NoBanIsZeroMinutes) {
	spawnActor(0);
	EXPECT_FALSE(ChatBanService::isBanned(player()));
	EXPECT_EQ(ChatBanService::getBanMinutes(player()), 0);
	EXPECT_FALSE(player().getController().hasTask(model::TaskId::GAG));
}

TEST_F(ChatBanServiceTest, ABanCountsItsMinutesRoundedUpAndRegistersTheGagTask) {
	spawnActor(0);
	ChatBanService::banPlayer(player(), 2 * 60 * 1000);
	EXPECT_TRUE(ChatBanService::isBanned(player()));
	EXPECT_EQ(ChatBanService::getBanMinutes(player()), 2) << "ceil(119.99.. s / 60 s)";
	EXPECT_TRUE(player().getController().hasTask(model::TaskId::GAG));

	ChatBanService::banPlayer(player(), 61 * 1000);
	EXPECT_EQ(ChatBanService::getBanMinutes(player()), 2) << "ceil(60.99.. s / 60 s): a started minute counts";
}

TEST_F(ChatBanServiceTest, TheGagTaskUnbansAndTellsThePlayer) {
	spawnActor(0);
	ChatBanService::banPlayer(player(), 60 * 1000);
	clearSent();
	advance(59 * 1000);
	EXPECT_TRUE(sent().empty());
	EXPECT_TRUE(player().getController().hasTask(model::TaskId::GAG));
	advance(1000);
	EXPECT_EQ(sent(), cptest::exactly({canChatNow()})) << "unbanPlayer: the ban was in the map and the player is online";
	EXPECT_FALSE(player().getController().hasTask(model::TaskId::GAG));
	clearSent();
	ChatBanService::unbanPlayer(player());
	EXPECT_TRUE(sent().empty()) << "no second STR_CAN_CHAT_NOW: the map holds no ban any more";
}

TEST_F(ChatBanServiceTest, AnExpiredBanIsLiftedByTheQuery) {
	spawnActor(0);
	ChatBanService::banPlayer(player(), 1);
	player().getController().cancelTask(model::TaskId::GAG); // as after a relog (the task died with the old controller tasks)
	std::this_thread::sleep_for(std::chrono::milliseconds(5));
	clearSent();
	EXPECT_EQ(ChatBanService::getBanMinutes(player()), 0);
	EXPECT_EQ(sent(), cptest::exactly({canChatNow()})) << "millisLeft <= 0: unbanPlayer";
	EXPECT_FALSE(ChatBanService::isBanned(player()));
}

TEST_F(ChatBanServiceTest, AQueryRegistersAMissingGagTask) {
	spawnActor(0);
	ChatBanService::banPlayer(player(), 60 * 1000);
	player().getController().cancelTask(model::TaskId::GAG);
	ASSERT_FALSE(player().getController().hasTask(model::TaskId::GAG));
	EXPECT_EQ(ChatBanService::getBanMinutes(player()), 1);
	EXPECT_TRUE(player().getController().hasTask(model::TaskId::GAG)) << "registerUnban(player, millisLeft) (ChatBanService.java:71-72)";
}

} // namespace
} // namespace aion::gameserver::services::teleport::test
