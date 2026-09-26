#include <gtest/gtest.h>

#include "LoginServerDatabaseTest.h"
#include "aion/loginserver/dao/GameServersDAO.h"
#include "aion/loginserver/dao/PlayerTransferDAO.h"

using namespace aion::loginserver;
using namespace aion::loginserver::test;

namespace {

/** Stand-in for com.aionemu.loginserver.GameServerInfo (its constructor) */
struct TestGameServerInfo {
	TestGameServerInfo(int8_t serverId, std::string mask, std::string pass) : id(serverId), ipMask(std::move(mask)), password(std::move(pass)) {}
	TestGameServerInfo(const TestGameServerInfo&) = delete;
	int8_t id;
	std::string ipMask;
	std::string password;
};

/** Stand-in for com.aionemu.loginserver.service.ptransfer.PlayerTransferTask */
struct TestPlayerTransferTask {
	int32_t sourceAccountId = 0, targetAccountId = 0, playerId = 0;
	int8_t sourceServerId = 0, targetServerId = 0;
	int32_t id = 0;
	int8_t status = 0;
	std::optional<std::string> comment;

	static constexpr int8_t STATUS_WAIT = 0, STATUS_ACTIVE = 1, STATUS_DONE = 2, STATUS_ERROR = 3;
};

/** A task type with a non-nullable comment */
struct TaskWithStringComment : TestPlayerTransferTask {
	std::string comment;
};

class GameServersDAOTest : public LoginServerDatabaseTest {};
class PlayerTransferDAOTest : public LoginServerDatabaseTest {};

} // namespace

TEST_F(GameServersDAOTest, GetAllGameServers) {
	EXPECT_TRUE(dao::GameServersDAO::getAllGameServers<TestGameServerInfo>().empty());
	execute("INSERT INTO gameservers (id, mask, password) VALUES (1, '127.0.0.1', 'secret'), (2, '192.168.*.*', 'other')");
	auto servers = dao::GameServersDAO::getAllGameServers<TestGameServerInfo>();
	ASSERT_EQ(servers.size(), 2u);
	EXPECT_EQ(servers.at(1)->id, 1);
	EXPECT_EQ(servers.at(1)->ipMask, "127.0.0.1");
	EXPECT_EQ(servers.at(1)->password, "secret");
	EXPECT_EQ(servers.at(2)->ipMask, "192.168.*.*");
	EXPECT_EQ(servers.at(2)->password, "other");
}

TEST_F(GameServersDAOTest, ErrorIsLoggedAndServersReadBeforeAreKept) {
	execute("INSERT INTO gameservers (id, mask, password) VALUES (1, 'a', 'b'), (300, 'c', 'd')"); // 300 is out of the byte range
	LogCapture dbLog("com.aionemu.commons.database.DB");
	auto servers = dao::GameServersDAO::getAllGameServers<TestGameServerInfo>();
	EXPECT_EQ(servers.size(), 1u);
	EXPECT_TRUE(dbLog.contains("error|Error executing select query SELECT * FROM gameservers")) << dbLog.str();
}

TEST_F(PlayerTransferDAOTest, GetNewReturnsWaitingTasks) {
	execute("INSERT INTO player_transfers (id, source_server, target_server, source_account_id, target_account_id, player_id, status) VALUES "
					"(1, 1, 2, 100, 200, 5000, 0), (2, -3, 4, 101, 201, 5001, 1), (3, 5, 6, 102, 202, 5002, 0)");
	auto tasks = dao::PlayerTransferDAO::getNew<TestPlayerTransferTask>();
	ASSERT_EQ(tasks.size(), 2u);
	const auto& task = tasks[0].id == 1 ? tasks[0] : tasks[1];
	EXPECT_EQ(task.id, 1);
	EXPECT_EQ(task.sourceServerId, 1);
	EXPECT_EQ(task.targetServerId, 2);
	EXPECT_EQ(task.sourceAccountId, 100);
	EXPECT_EQ(task.targetAccountId, 200);
	EXPECT_EQ(task.playerId, 5000);
	EXPECT_EQ(task.status, 0);
	EXPECT_FALSE(task.comment);

	execute("UPDATE player_transfers SET status = 0 WHERE id = 2");
	tasks = dao::PlayerTransferDAO::getNew<TestPlayerTransferTask>();
	ASSERT_EQ(tasks.size(), 3u);
	for (const auto& t : tasks)
		if (t.id == 2)
			EXPECT_EQ(t.sourceServerId, -3);
}

TEST_F(PlayerTransferDAOTest, UpdateSetsStatusCommentAndTimes) {
	execute("INSERT INTO player_transfers (id, source_server, target_server, source_account_id, target_account_id, player_id) VALUES "
					"(1, 1, 2, 100, 200, 5000), (2, 1, 2, 100, 200, 5001), (3, 1, 2, 100, 200, 5002)");
	TestPlayerTransferTask task;
	task.id = 1;
	task.status = TestPlayerTransferTask::STATUS_ACTIVE;
	EXPECT_TRUE(dao::PlayerTransferDAO::update(task));
	EXPECT_EQ(queryLong("SELECT status FROM player_transfers WHERE id = 1"), 1);
	EXPECT_EQ(queryString("SELECT comment FROM player_transfers WHERE id = 1"), std::nullopt);
	EXPECT_NE(queryString("SELECT time_performed FROM player_transfers WHERE id = 1"), std::nullopt);
	EXPECT_EQ(queryString("SELECT time_done FROM player_transfers WHERE id = 1"), std::nullopt);

	task.status = TestPlayerTransferTask::STATUS_DONE;
	task.comment = "task done";
	EXPECT_TRUE(dao::PlayerTransferDAO::update(task));
	EXPECT_EQ(queryLong("SELECT status FROM player_transfers WHERE id = 1"), 2);
	EXPECT_EQ(queryString("SELECT comment FROM player_transfers WHERE id = 1"), "task done");
	EXPECT_NE(queryString("SELECT time_done FROM player_transfers WHERE id = 1"), std::nullopt);

	TaskWithStringComment error;
	error.id = 2;
	error.status = TestPlayerTransferTask::STATUS_ERROR;
	error.comment = "reason";
	EXPECT_TRUE(dao::PlayerTransferDAO::update(error));
	EXPECT_EQ(queryLong("SELECT status FROM player_transfers WHERE id = 2"), 3);
	EXPECT_EQ(queryString("SELECT comment FROM player_transfers WHERE id = 2"), "reason");
	EXPECT_NE(queryString("SELECT time_done FROM player_transfers WHERE id = 2"), std::nullopt);
	EXPECT_EQ(queryString("SELECT time_performed FROM player_transfers WHERE id = 2"), std::nullopt);

	// STATUS_WAIT sets no time
	EXPECT_TRUE(dao::PlayerTransferDAO::update(3, dao::PlayerTransferDAO::STATUS_WAIT, "waiting"));
	EXPECT_EQ(queryString("SELECT comment FROM player_transfers WHERE id = 3"), "waiting");
	EXPECT_EQ(queryString("SELECT time_performed FROM player_transfers WHERE id = 3"), std::nullopt);
	EXPECT_EQ(queryString("SELECT time_done FROM player_transfers WHERE id = 3"), std::nullopt);
}

TEST_F(PlayerTransferDAOTest, GetNewErrorIsLogged) {
	execute("DROP TABLE player_transfers");
	LogCapture log("com.aionemu.loginserver.dao.PlayerTransferDAO");
	EXPECT_TRUE(dao::PlayerTransferDAO::getNew<TestPlayerTransferTask>().empty());
	EXPECT_TRUE(log.contains("error|Can't select getNew: \n")) << log.str();
}
