// M5i I-8 (m5i-plan.md 15.3): //base (data/handlers/admincommands/BaseCommand.java) on a real Player with a real AionConnection
// (CommandTestSupport.h), over a two-row base table and no base spawns: the syntax, the list sorted by id, and the refusals of every arm.
// Capturing and assaulting real bases is the gate's (I-11, the bases).

#include "CommandTestSupport.h"

#include <map>
#include <string>
#include <vector>

#include "aion/gameserver/configs/administration/CommandsConfig.h"
#include "aion/gameserver/dataholders/BaseData.bind.h"
#include "aion/gameserver/dataholders/BaseData.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/handlers/admincommands/BaseCommand.h"
#include "aion/gameserver/services/BaseService.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets::testing {
namespace {

class BaseCommandTest : public CommandTest {
protected:
	void SetUp() override {
		CommandTest::SetUp();
		std::map<std::string, int8_t, std::less<>> levels(*configs::administration::CommandsConfig::ACCESS_LEVELS.get());
		levels["base"] = 9; // commands.properties: base = 9
		configs::administration::CommandsConfig::ACCESS_LEVELS.set(levels);
		// BaseService reads BASE_DATA once, in its constructor: published once for the whole executable (ids out of order on purpose)
		static const bool published = [] {
			static xml::LoadContext context;
			dataholders::DataManager::BASE_DATA.publish(xml::bindString<dataholders::BaseData>(context, R"(<base_locations>
	<base_location id="9202" type="SIEGE" world="400010000" default_occupier="ELYOS" />
	<base_location id="9201" type="CASUAL" world="210020000" />
</base_locations>)"));
			return true;
		}();
		static_cast<void>(published);
	}

	Player& gm() { return connected(730300, "Basewarden", 9); }
};

TEST_F(BaseCommandTest, WithoutAnArmItSendsTheSyntax) {
	Player& admin = gm();
	handlers::admincommands::BaseCommand base;
	EXPECT_TRUE(base.process(admin, args({})));
	EXPECT_EQ(client()->sentBytes(), info(base.getSyntaxInfo()));
	client()->clearSent();
	EXPECT_TRUE(base.process(admin, args({"defend"})));
	EXPECT_EQ(client()->sentBytes(), info(base.getSyntaxInfo())) << "an unknown arm";
}

TEST_F(BaseCommandTest, TheListIsSortedById) {
	Player& admin = gm();
	handlers::admincommands::BaseCommand base;
	EXPECT_TRUE(base.process(admin, args({"LIST"})));
	std::vector<std::vector<uint8_t>> expected = info("Base 9201 belongs to BALAUR");
	for (std::vector<uint8_t>& packet : info("Base 9202 belongs to ELYOS"))
		expected.push_back(std::move(packet));
	EXPECT_EQ(client()->sentBytes(), expected);
}

TEST_F(BaseCommandTest, EveryArmRefusesMissingArgumentsUnknownIdsAndInactiveBases) {
	Player& admin = gm();
	handlers::admincommands::BaseCommand base;
	const auto expectInfo = [&](std::initializer_list<std::string_view> arguments, std::string_view text) {
		client()->clearSent();
		EXPECT_TRUE(base.process(admin, args(arguments)));
		EXPECT_EQ(client()->sentBytes(), info(text)) << text;
	};
	expectInfo({"start"}, "Not enough parameters.");
	expectInfo({"capture", "9201"}, "Not enough parameters.");
	expectInfo({"start", "1"}, "Invalid base ID.");
	expectInfo({"stop", "9201"}, "Base is already inactive.");
	expectInfo({"capture", "9201", "elyos"}, "Inactive bases cannot be captured.");
	expectInfo({"assault", "9201", "elyos"}, "Inactive bases cannot be assaulted.");
	EXPECT_FALSE(services::BaseService::getInstance().isActive(9201));
}

} // namespace
} // namespace aion::gameserver::network::aion::clientpackets::testing
