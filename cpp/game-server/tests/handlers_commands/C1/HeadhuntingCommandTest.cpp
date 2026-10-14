// D12-CP5b (owner, 2026-10-13: D12 = port): //headhunting (Headhunting.java) on a real Player with a real AionConnection
// (CommandTestSupport.h) and PvpService's headhunters seeded through getHeadhunterById, without a database (the DAO and the mail fail and
// log). One hunter per race and class: the sort of analyze then compares nothing (Headhunter.compareTo is lane B's, M5i I-4). The texts are
// the Java literals.

#include "CommandTestSupport.h"

#include <map>
#include <string>

#include "aion/gameserver/configs/administration/CommandsConfig.h"
#include "aion/gameserver/configs/main/EventsConfig.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/ItemData.bind.h"
#include "aion/gameserver/dataholders/ItemData.h"
#include "aion/gameserver/dataholders/KillBountyData.bind.h"
#include "aion/gameserver/dataholders/KillBountyData.h"
#include "aion/gameserver/handlers/admincommands/Headhunting.h"
#include "aion/gameserver/model/event/Headhunter.h"
#include "aion/gameserver/model/gameobjects/Persistable.h"
#include "aion/gameserver/services/PvpService.h"
#include "aion/gameserver/world/World.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets::testing {
namespace {

using configs::main::EventsConfig;

/** item_templates.xml:836077-836082, :930737, :896317-896319: the reward items (SystemMailService reads their templates) */
constexpr std::string_view REWARD_ITEMS_XML = R"x(<item_templates>
	<item_template id="164002276" name="Eternal War Battle Scroll" level="65" cName="world_event_scroll_leader_pvp_65a" mask="12360" max_stack_count="1000" quality="COMMON" price="5" restrict="65 65 65 65 65 65 65 65 65 65 65 65 65 65 65 65 65" desc="838506" activate_target="STANDALONE" activate_count="1">
		<actions>
			<skilluse level="3" skillid="10946"/>
		</actions>
		<uselimits usedelay="1000" usedelayid="33"/>
	</item_template>
	<item_template id="188950017" name="Special Courier Pass (Abyss Eternal/Lv. 61-65)" level="1" cName="World_cash_coin_post_abyss_E_65" mask="12410" max_stack_count="10000" quality="EPIC" price="5" desc="826589"/>
	<item_template id="186000051" name="Major Ancient Crown" level="50" cName="treasure_01a" mask="12360" max_stack_count="100" quality="RARE" price="9600000" desc="746100">
		<acquisition type="AP" ap="6400"/>
	</item_template>
</item_templates>)x";

class HeadhuntingCommandTest : public CommandTest {
protected:
	void SetUp() override {
		CommandTest::SetUp();
		std::map<std::string, int8_t, std::less<>> levels(*configs::administration::CommandsConfig::ACCESS_LEVELS.get());
		levels["headhunting"] = 9;
		configs::administration::CommandsConfig::ACCESS_LEVELS.set(levels);
		savedEnabled = EventsConfig::ENABLE_HEADHUNTING.exchange(false);
		if (!dataholders::DataManager::KILL_BOUNTY_DATA) { // PvpService's constructor reads the kill bounties (and, without a database, no hunters)
			xml::LoadContext context;
			dataholders::DataManager::KILL_BOUNTY_DATA.publish(xml::bindString<dataholders::KillBountyData>(context, "<kill_bounties/>"));
			bountiesPublished = true;
		}
		services::PvpService::getInstance().finalizeHeadhuntingSeason(); // no hunters of another case
		dataholders::DataManager::ITEM_DATA.publish(xml::bindString<dataholders::ItemData>(itemContext, REWARD_ITEMS_XML));
	}

	void TearDown() override {
		services::PvpService::getInstance().finalizeHeadhuntingSeason();
		EventsConfig::ENABLE_HEADHUNTING.store(savedEnabled);
		dataholders::DataManager::ITEM_DATA.resetForTests();
		if (bountiesPublished)
			dataholders::DataManager::KILL_BOUNTY_DATA.resetForTests();
		for (Player* player : stored)
			world::World::getInstance().removeObject(*player);
		stored.clear();
		CommandTest::TearDown();
	}

	Player& gm(int32_t objectId) {
		Player& player = connected(objectId, "Warden", 9);
		spawnInPoeta(player);
		world::World::getInstance().storeObject(player);
		stored.push_back(&player);
		return player;
	}

	bool savedEnabled = false;
	bool bountiesPublished = false;
	xml::LoadContext itemContext;
	std::vector<Player*> stored;
};

/** Headhunting.java:73-115: the syntax, an unknown arm, show without its argument, addKills and finalize without theirs */
TEST_F(HeadhuntingCommandTest, Syntax) {
	Player& admin = gm(731701);
	handlers::admincommands::Headhunting command;
	for (std::vector<std::string> params : {args({}), args({"unknown"}), args({"show"}), args({"addKills", "1"}), args({"finalize"})}) {
		client()->clearSent();
		EXPECT_TRUE(command.process(admin, params));
		EXPECT_EQ(client()->sentBytes(), info(command.getSyntaxInfo())) << params.size();
	}
	client()->clearSent();
	EXPECT_TRUE(command.process(admin, args({"Analyze"})));
	EXPECT_EQ(client()->sentBytes(), info(command.getSyntaxInfo())) << "the switch is case-sensitive";
}

/** Headhunting.java:117-134: addKills while the season is off, then on: the hunter is created, the kills added, never below 0 */
TEST_F(HeadhuntingCommandTest, AddKills) {
	Player& admin = gm(731702);
	handlers::admincommands::Headhunting command;
	EXPECT_TRUE(command.process(admin, args({"addKills", "731702", "5"})));
	EXPECT_EQ(client()->sentBytes(), info("Season is not active."));
	EventsConfig::ENABLE_HEADHUNTING.store(true);
	client()->clearSent();
	EXPECT_TRUE(command.process(admin, args({"addKills", "731702", "5"})));
	EXPECT_EQ(client()->sentBytes(), info("Successfully added 5 kills for player [ID=731702, name=Warden, currentKills=5]."));
	runtime::Ptr<model::event::Headhunter> hunter = services::PvpService::getInstance().getHeadhunter(731702);
	ASSERT_NE(hunter, nullptr);
	EXPECT_EQ(hunter->getKills(), 5);
	EXPECT_EQ(hunter->getPersistentState(), model::gameobjects::Persistable::PersistentState::UPDATE_REQUIRED);
	client()->clearSent();
	EXPECT_TRUE(command.process(admin, args({"addKills", "731702", "-9"})));
	EXPECT_EQ(client()->sentBytes(), info("Successfully added -9 kills for player [ID=731702, name=Warden, currentKills=0]."));
	EXPECT_EQ(hunter->getKills(), 0);
}

/** Headhunting.java:136-160, 253-276 and 211-251: analyze once, clear, finalize's refusals, and the rewarding of the one hunter (the mail fails
 * without a database: no mail sent), which clears the season */
TEST_F(HeadhuntingCommandTest, AnalyzeClearAndFinalize) {
	Player& admin = gm(731703);
	handlers::admincommands::Headhunting command;
	EventsConfig::ENABLE_HEADHUNTING.store(true);
	EXPECT_TRUE(command.process(admin, args({"addKills", "731703", "7"})));
	client()->clearSent();
	EXPECT_TRUE(command.process(admin, args({"analyze"})));
	EXPECT_EQ(client()->sentBytes(), info("Season successfully analyzed."));
	client()->clearSent();
	EXPECT_TRUE(command.process(admin, args({"analyze"})));
	EXPECT_EQ(client()->sentBytes(), info("Season is still ascertained. Use <show> paramater to get a survey with the final information "
										  "or use <reward> to execute the reward progress."));
	client()->clearSent();
	EXPECT_TRUE(command.process(admin, args({"finalize", "true"})));
	EXPECT_EQ(client()->sentBytes(), info("Season is still active! You have to disable headhunting to reward participants."));
	EventsConfig::ENABLE_HEADHUNTING.store(false);
	client()->clearSent();
	EXPECT_TRUE(command.process(admin, args({"clear"})));
	EXPECT_EQ(client()->sentBytes(), info("Results successfully cleared."));
	client()->clearSent();
	EXPECT_TRUE(command.process(admin, args({"clear"})));
	EXPECT_TRUE(client()->sentBytes().empty()) << "nothing to clear";
	client()->clearSent();
	EXPECT_TRUE(command.process(admin, args({"finalize", "TRUE"})));
	EXPECT_EQ(client()->sentBytes(), info("No results available! The headhunting season needs to be analyzed before rewarding."));

	client()->clearSent();
	EXPECT_TRUE(command.process(admin, args({"analyze"})));
	client()->clearSent();
	EXPECT_TRUE(command.process(admin, args({"finalize", "true"})));
	std::vector<std::vector<uint8_t>> sent = client()->sentBytes();
	std::vector<std::vector<uint8_t>> rewarded = info("Successfully rewarded 1 Players and sent 0 Mails.");
	std::vector<std::vector<uint8_t>> cleared = info("Successfully cleared all references for this season and finished archiving.");
	ASSERT_EQ(sent.size(), 2u);
	EXPECT_EQ(sent[0], rewarded[0]) << "the first rank's three items, whose mails fail without a database";
	EXPECT_EQ(sent[1], cleared[0]);
	EXPECT_EQ(services::PvpService::getInstance().getHeadhunter(731703), nullptr) << "PvpService.finalizeHeadhuntingSeason";
	client()->clearSent();
	EXPECT_TRUE(command.process(admin, args({"finalize", "true"})));
	EXPECT_EQ(client()->sentBytes(), info("No results available! The headhunting season needs to be analyzed before rewarding.")) << "results = null";
}

} // namespace
} // namespace aion::gameserver::network::aion::clientpackets::testing
