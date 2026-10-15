// D12-CP2 (owner, 2026-10-13: D12 = port): //cinstance (CustomInstance.java), .buy (Buy.java) and CustomInstanceRankEnum's companion, on a real
// Player with a real AionConnection (CommandTestSupport.h). No database here: CustomInstanceDAO logs and answers "no rank" / false, so
// //cinstance's arms run on the created rank (the DAO's own cases are ServerDataDaoTest's). The texts are the Java literals.

#include "CommandTestSupport.h"

#include <map>
#include <string>
#include <vector>

#include "aion/gameserver/configs/administration/CommandsConfig.h"
#include "aion/gameserver/custom/instance/CustomInstanceRankEnumInfo.h"
#include "aion/gameserver/custom/instance/CustomInstanceService.h"
#include "aion/gameserver/dataholders/ItemData.h"
#include "aion/gameserver/dataholders/ItemRestrictionCleanupData.h"
#include "aion/gameserver/dataholders/ItemSetData.h"
#include "aion/gameserver/handlers/admincommands/CustomInstance.h"
#include "aion/gameserver/handlers/playercommands/Buy.h"
#include "aion/gameserver/model/gameobjects/Item.h"
#include "aion/gameserver/model/gameobjects/player/ResponseRequester.h"
#include "aion/gameserver/model/items/storage/Storage.h"
#include "aion/gameserver/network/aion/serverpackets/SM_QUESTION_WINDOW.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/utils/ChatUtil.h"
#include "aion/gameserver/utils/JavaColor.h"
#include "aion/gameserver/world/World.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets::testing {
namespace {

using custom::instance::CustomInstanceRankEnum;
using serverpackets::SM_QUESTION_WINDOW;
using serverpackets::SM_SYSTEM_MESSAGE;
using namespace items; // the item rows and helpers of ItemPacketTestSupport.h

/** item_templates.xml:897166-897168 (the reward coin), :837987-837991 (the Delta Enchantment Stone) */
constexpr std::string_view CUSTOM_ITEM_TEMPLATES_XML = R"xml(
	<item_template id="186000409" name="Daeva's Respite Coin" level="1" cName="world_coin_fellow_01_a" mask="12352" max_stack_count="10000" quality="COMMON" price="5" desc="847614">
		<inventory id="1"/>
	</item_template>
	<item_template id="166000194" name="Delta Enchantment Stone" level="1" cName="matter_enchant_g4" mask="12414" max_stack_count="100" item_group="ENCHANTMENT" quality="COMMON" price="950" desc="842374" activate_count="1">
		<actions>
			<enchant count="100"/>
		</actions>
	</item_template>
</item_templates>)xml";

constexpr int32_t COIN = custom::instance::CustomInstanceService::REWARD_COIN_ID;
constexpr int32_t DELTA = 166000194;

class CustomContentCommandsTest : public CommandTest {
protected:
	void SetUp() override {
		CommandTest::SetUp();
		std::map<std::string, int8_t, std::less<>> levels(*configs::administration::CommandsConfig::ACCESS_LEVELS.get());
		levels["cinstance"] = 9; // commands.properties
		levels["buy"] = 0;
		configs::administration::CommandsConfig::ACCESS_LEVELS.set(levels);
		std::string itemsXml(ITEM_TEMPLATES_XML);
		itemsXml.replace(itemsXml.rfind("</item_templates>"), std::string_view("</item_templates>").size(), CUSTOM_ITEM_TEMPLATES_XML);
		dataholders::DataManager::ITEM_DATA.publish(xml::bindString<dataholders::ItemData>(context, itemsXml));
		dataholders::DataManager::ITEM_CLEAN_UP.publish(std::make_unique<dataholders::ItemRestrictionCleanupData>());
		dataholders::DataManager::ITEM_SET_DATA.publish(std::make_unique<dataholders::ItemSetData>());
	}

	void TearDown() override {
		for (Player* player : stored) {
			player->setTarget(nullptr);
			world::World::getInstance().removeObject(*player);
		}
		stored.clear();
		loaded.clear();
		CommandTest::TearDown();
		dataholders::DataManager::ITEM_SET_DATA.resetForTests();
		dataholders::DataManager::ITEM_CLEAN_UP.resetForTests();
		dataholders::DataManager::ITEM_DATA.resetForTests();
	}

	Player& online(int32_t objectId, std::string_view name, int8_t accessLevel) {
		Player& player = connected(objectId, name, accessLevel);
		spawnInPoeta(player);
		world::World::getInstance().storeObject(player);
		stored.push_back(&player);
		return player;
	}

	void coins(Player& player, int32_t objId, int64_t count) {
		runtime::Ref<Item> item = loadedItem(objId, COIN, count, StorageType::CUBE);
		player.getInventory().onLoadHandler(*item);
		loaded.push_back(item);
	}

	static std::string item(int32_t itemId) { return utils::ChatUtil::item(itemId); }

	xml::LoadContext context;
	std::vector<Player*> stored;
	std::vector<runtime::Ref<Item>> loaded;
};

/** CustomInstanceRankEnum.java:10-53: the ranks' bounds, rewards, icons and descriptions */
TEST(CustomInstanceRankEnumTest, TheCompanion) {
	using namespace custom::instance;
	EXPECT_EQ(getByRank(0), CustomInstanceRankEnum::IRON);
	EXPECT_EQ(getByRank(2), CustomInstanceRankEnum::BRONZE);
	EXPECT_EQ(getByRank(3), CustomInstanceRankEnum::SILVER);
	EXPECT_EQ(getByRank(23), CustomInstanceRankEnum::ANCIENT);
	EXPECT_EQ(getByRank(24), CustomInstanceRankEnum::ANCIENT_PLUS);
	EXPECT_EQ(getByRank(1000), CustomInstanceRankEnum::ANCIENT_PLUS);
	EXPECT_THROW(getByRank(-1), runtime::IllegalArgumentException);
	EXPECT_EQ(getMinRank(CustomInstanceRankEnum::PLATINUM), 8);
	EXPECT_EQ(getMinReward(CustomInstanceRankEnum::PLATINUM), 28);
	EXPECT_EQ(getMinReward(CustomInstanceRankEnum::ANCIENT_PLUS), 50);
	EXPECT_EQ(getRankIcon(CustomInstanceRankEnum::IRON), u'');
	EXPECT_EQ(getRankIcon(CustomInstanceRankEnum::CERANIUM), u'');
	EXPECT_EQ(getRankDescription(0), "\xEE\x80\xAB IRON");
	EXPECT_EQ(getRankDescription(13), "\xEE\x80\xB0 MITHRIL");
	EXPECT_EQ(getRankDescription(24), "\xEE\x82\xAA ANCIENT +1");
	EXPECT_EQ(getRankDescription(30), "\xEE\x82\xAA ANCIENT +7");
}

/** CustomInstance.java:24-48: the syntax, no player target, and the three arms on a player without a rank row */
TEST_F(CustomContentCommandsTest, CInstance) {
	Player& admin = online(731601, "Warden", 9);
	Player& other = online(731602, "Rankee", 0);
	handlers::admincommands::CustomInstance command;
	EXPECT_TRUE(command.process(admin, args({})));
	EXPECT_EQ(client()->sentBytes(), info(command.getSyntaxInfo()));
	client()->clearSent();
	EXPECT_TRUE(command.process(admin, args({"getrank"})));
	EXPECT_EQ(client()->sentBytes(), std::vector<std::vector<uint8_t>>{serialized(SM_SYSTEM_MESSAGE::STR_INVALID_TARGET(), client().con())});

	admin.setTarget(runtime::Ptr<model::gameobjects::VisibleObject>(other));
	client()->clearSent();
	EXPECT_TRUE(command.process(admin, args({"GETRANK"})));
	EXPECT_EQ(client()->sentBytes(), info(utils::ChatUtil::charName(other) + "'s current rank is \xEE\x80\xAB IRON (0)."));
	client()->clearSent();
	EXPECT_TRUE(command.process(admin, args({"removecd"})));
	EXPECT_EQ(client()->sentBytes(), info(utils::ChatUtil::charName(other) + " does not need a reset.")) << "no rank row: resetEntryCooldown is false";
	client()->clearSent();
	EXPECT_TRUE(command.process(admin, args({"setrank", "24"})));
	EXPECT_EQ(client()->sentBytes(), info("Changed " + utils::ChatUtil::charName(other) + " to 24 which is equivalent to \xEE\x82\xAA ANCIENT +1"));
	client()->clearSent();
	EXPECT_TRUE(command.process(admin, args({"setrank"})));
	EXPECT_EQ(client()->sentBytes(), info(command.getSyntaxInfo())) << "setrank needs the rank";
	client()->clearSent();
	EXPECT_TRUE(command.process(admin, args({"setrank", "x"})));
	EXPECT_EQ(client()->sentBytes(), info("Invalid number: \"x\"")) << "Java: Integer.parseInt's NumberFormatException";
}

/** Buy.java:64-96: the reward list with the padded costs and the coin icon, the steps */
TEST_F(CustomContentCommandsTest, BuyShowsTheRewards) {
	Player& player = online(731603, "Buyer", 0);
	handlers::playercommands::Buy buy;
	EXPECT_TRUE(buy.process(player, args({})));
	std::vector<std::vector<uint8_t>> expected = info("Cost:");
	auto add = [&](const std::string& text) {
		for (std::vector<uint8_t>& packet : info(text))
			expected.push_back(std::move(packet));
	};
	add("    40\xEE\x81\xAC   5x " + item(166000194)); // "40" padded with 2 * 2 spaces
	add("    50\xEE\x81\xAC   5x " + item(166000195));
	add("  100\xEE\x81\xAC   " + item(188051516)); // an amount of 1 is not written
	ASSERT_GT(client()->sentBytes().size(), 4u);
	std::vector<std::vector<uint8_t>> sent = client()->sentBytes();
	for (size_t i = 0; i < expected.size(); i++)
		EXPECT_EQ(sent[i], expected[i]) << i;
	EXPECT_EQ(sent.size(), 1u + 22u + 4u) << "Cost:, 22 rewards, To buy: and three steps";
	EXPECT_EQ(sent[22], info("2500\xEE\x81\xAC   " + item(188053109))[0]) << "the last reward: four digits, no padding";
	EXPECT_EQ(sent.back(), info("3. (Ctrl + Right-click) the item from your Memo Pad.")[0]);
	EXPECT_EQ(sent[25], info("2. Type " + utils::ChatUtil::color(".buy", utils::JavaColor::WHITE) + " in your chat field (with space).")[0]);
	client()->clearSent();
	EXPECT_TRUE(buy.process(player, args({"a", "b"})));
	EXPECT_EQ(client()->sentBytes(), info(buy.getSyntaxInfo()));
}

/** Buy.java:98-134: an unknown item, an item not in the rewards, too few coins, then the question, its acceptance, the coins and the item */
TEST_F(CustomContentCommandsTest, BuyAnItem) {
	Player& player = online(731604, "Buyer", 0);
	handlers::playercommands::Buy buy;
	EXPECT_TRUE(buy.process(player, args({"999999999"})));
	EXPECT_EQ(client()->sentBytes(), info("\"999999999\" is not a valid item (must be an item link or ID)."));
	client()->clearSent();
	EXPECT_TRUE(buy.process(player, args({std::to_string(COIN)})));
	EXPECT_EQ(client()->sentBytes(), info(item(COIN) + " is not contained in the rewards."));
	coins(player, 731690, 30);
	coins(player, 731691, 5);
	client()->clearSent();
	EXPECT_TRUE(buy.process(player, args({std::to_string(DELTA)})));
	EXPECT_EQ(client()->sentBytes(), info("You need 40\xEE\x81\xAC to buy " + item(DELTA) + ".")) << "35 coins in two stacks";
	coins(player, 731692, 10);
	client()->clearSent();
	EXPECT_TRUE(buy.process(player, args({item(DELTA)})));
	std::vector<std::vector<uint8_t>> question = client()->sentBytes();
	ASSERT_EQ(question.size(), 1u);
	EXPECT_EQ(question[0], serialized(SM_QUESTION_WINDOW(SM_QUESTION_WINDOW::STR_AIONJEWEL_SHOP_BUY_CONFIRM, 0, 0, 40,
		dataholders::DataManager::ITEM_DATA->getItemTemplate(COIN)->getL10n()), client().con()));
	EXPECT_EQ(player.getInventory().getItemCountByItemId(COIN), 45) << "nothing is taken before the answer";
	client()->clearSent();
	ASSERT_TRUE(player.getResponseRequester().respond(SM_QUESTION_WINDOW::STR_AIONJEWEL_SHOP_BUY_CONFIRM, 1));
	EXPECT_EQ(player.getInventory().getItemCountByItemId(COIN), 5);
	EXPECT_EQ(player.getInventory().getItemCountByItemId(DELTA), 5) << "the reward's amount";
	bool spent = false;
	for (const std::vector<uint8_t>& packet : client()->sentBytes())
		spent = spent || packet == info("You have spent 40\xEE\x81\xAC.")[0];
	EXPECT_TRUE(spent);
}

} // namespace
} // namespace aion::gameserver::network::aion::clientpackets::testing
