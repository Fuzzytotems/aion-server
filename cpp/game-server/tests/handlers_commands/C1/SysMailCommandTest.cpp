// M5j stage 4 CP6 (m5j-plan.md §18.5, the orphaned commands of M5c): //sysmail (SysMail.java) on a real Player with a real AionConnection
// (CommandTestSupport.h). The texts are the Java literals. Sending a letter (SystemMailService.sendMail, MailFormatter.sendBlackCloudMail)
// and PlayerDAO.isNameUsed reach the database; the cases cover the parsing and checkExpress' refusals, which do not.

#include "CommandTestSupport.h"

#include <map>
#include <string>
#include <vector>

#include "aion/gameserver/configs/administration/CommandsConfig.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/ItemData.bind.h"
#include "aion/gameserver/dataholders/ItemData.h"
#include "aion/gameserver/handlers/admincommands/SysMail.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets::testing {
namespace {

class SysMailCommandTest : public CommandTest {
protected:
	void SetUp() override {
		CommandTest::SetUp();
		std::map<std::string, int8_t, std::less<>> levels(*configs::administration::CommandsConfig::ACCESS_LEVELS.get());
		levels["sysmail"] = 8; // commands.properties
		configs::administration::CommandsConfig::ACCESS_LEVELS.set(levels);
		if (!dataholders::DataManager::ITEM_DATA) { // checkExpress reads ITEM_DATA.getItemTemplate(item) before it looks at the item
			dataholders::DataManager::ITEM_DATA.publish(xml::bindString<dataholders::ItemData>(context, "<item_templates/>"));
			publishedItems = true;
		}
	}

	void TearDown() override {
		CommandTest::TearDown();
		if (publishedItems)
			dataholders::DataManager::ITEM_DATA.resetForTests();
	}

	Player& gm(int32_t objectId) {
		Player& player = connected(objectId, "Warden", 9);
		spawnInPoeta(player);
		return player;
	}

	xml::LoadContext context;
	bool publishedItems = false;
};

constexpr std::string_view SYNTAX =
	"No parameters detected.\n"
	"Please use //sysmail [%|$$<Sender>] <Recipient> <Regular|Blackcloud|Express> <Item> <Count> <Kinah> [|Title|] [|Message|]\n"
	"Sender name must start with % or $$. Can be ommitted.\n"
	"Regular mail type is 0, Express mail type is 1, Blackcloud type is 2.\n"
	"If parameters (Item, Count) = 0 than the item will not be send\n"
	"If parameters (Kinah) = 0 not send Kinah\n"
	"Recipient = Player name, @all, @elyos or @asmodians\n"
	"Optional Title and Message must be enclosed within pipe chars";

/** SysMail.java:49-100, :192-255: the syntax, the recipient, the numbers and checkExpress' refusals */
TEST_F(SysMailCommandTest, ParsingAndRefusals) {
	Player& admin = gm(730901);
	handlers::admincommands::SysMail sysMail;
	const std::vector<std::pair<std::vector<std::string>, std::string>> cases = {
		{args({"@all", "0", "0", "0"}), std::string(SYNTAX)},
		{args({"%Sender", "@all", "0", "0", "0"}), std::string(SYNTAX)},
		{args({"@orcs", "0", "0", "0", "5"}), "Recipient must be Player name, @all, @elyos or @asmodians."},
		{args({"@a", "0", "x", "0", "5"}), "<Regular|Blackcloud|Express> <Item|Count|Kinah> value must be an integer."},
		{args({"@all", "0", "0", "0", "0"}), "Parameters <Item> <Count> <Kinah> are icorrect."},
		{args({"$$Shop", "@e", "0", "0", "5", "0"}), "Please insert Item Id.."},
		{args({"@asmo", "0", "182400001", "0", "0"}), "Please insert Item Count."},
		{args({"@all", "0", "182400001", "1", "0"}), "Item id is incorrect: 182400001"},
		{args({"@all", "1", "0", "0", "-5"}), "Parameters <Item> <Count> <Kinah> are icorrect."}, // checked before the kinah sign
		{args({"@all", "2", "0", "0", "5"}), "Kinah attachment are not for black cloud letters!"},
		{args({"Nobody", "1", "0", "0", "5"}), "This Recipient is offline."},
	};
	for (const auto& [params, answer] : cases) {
		client()->clearSent();
		EXPECT_TRUE(sysMail.process(admin, params));
		EXPECT_EQ(client()->sentBytes(), exactly({message(answer)})) << params[0] << " " << params[1];
	}
}

} // namespace
} // namespace aion::gameserver::network::aion::clientpackets::testing
