// ChatLogDAO against the chat server test schema (AION_TEST_CS_DATABASE_URL, see support/ChatServerTestDatabase.h: these tests FAIL without it
// unless AION_CS_ALLOW_DATABASE_SKIP=1).

#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "aion/chatserver/dao/ChatLogDAO.h"
#include "aion/commons/database/DatabaseFactory.h"
#include "support/ChatServerTestDatabase.h"
#include "support/TestUtils.h"

namespace aion::chatserver::test {
namespace {

class ChatLogDAOTest : public ::testing::Test {
protected:
	void SetUp() override {
		AION_CS_REQUIRE_DATABASE();
		ASSERT_NO_THROW(database::recreateSchema());
		commons::database::DatabaseFactory::init(database::url(), database::user(), database::password(), 2, 5000);
	}

	void TearDown() override { commons::database::DatabaseFactory::shutdown(); }
};

TEST_F(ChatLogDAOTest, SaveInsertsSenderMessageAndType) {
	dao::ChatLogDAO::save("Alice", "hello world", "REGION (E)");
	dao::ChatLogDAO::save("Bob", "gr\xC3\xBC\xC3\x9F dich \xE4\xBD\xA0\xE5\xA5\xBD", "LANG: Deutsch (A)"); // "grüß dich 你好"
	std::vector<std::string> rows = database::queryRows("SELECT id, sender, message, type FROM chatlog ORDER BY id");
	EXPECT_EQ(rows, (std::vector<std::string>{"1|Alice|hello world|REGION (E)", "2|Bob|gr\xC3\xBC\xC3\x9F dich \xE4\xBD\xA0\xE5\xA5\xBD|LANG: Deutsch (A)"}));
}

TEST_F(ChatLogDAOTest, ErrorsAreLoggedNotThrown) {
	LogCapture log({"com.aionemu.chatserver.dao.ChatLogDAO"});
	database::openConnection()->executeSimple("DROP TABLE chatlog");
	EXPECT_NO_THROW(dao::ChatLogDAO::save("Alice", "lost", "REGION (E)"));
	EXPECT_TRUE(log.contains("Cannot insert chat message")) << log.dump();
	EXPECT_TRUE(log.contains("chatlog")) << log.dump(); // the cause names the missing table
}

} // namespace
} // namespace aion::chatserver::test
