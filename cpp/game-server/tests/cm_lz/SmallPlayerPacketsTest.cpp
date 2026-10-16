// Eight small client packets of P5-16 (play-session wishes of 2026-10-02) whose paths were ported already, each on the item packet fixture of
// tests/cm_ak (a player in a Verteron map instance with the real GeneralInstanceHandler, on a recording connection) and, where a second player
// matters, a buddy on its own recording connection, in the World and/or in the player's known list:
// - CM_UNWRAP_ITEM (C_UNPACK_ITEM, CM_UNWRAP_ITEM.java:26-45) and CM_SET_NOTE (C_TODAY_WORDS, CM_SET_NOTE.java:28-44);
// - CM_CLIENT_COMMAND_ROLL (/roll, CM_CLIENT_COMMAND_ROLL.java:26-38), CM_VIEW_PLAYER_DETAILS (inspect, CM_VIEW_PLAYER_DETAILS.java:25-40),
//   CM_TITLE_SET (CM_TITLE_SET.java:21-33), CM_CHAT_PLAYER_INFO (CM_CHAT_PLAYER_INFO.java:25-38);
// - CM_INSTANCE_LEAVE (CM_INSTANCE_LEAVE.java:24-29) and CM_STOP_TRAINING (CM_STOP_TRAINING.java:24-27), which only call the map instance's
//   handler: on an open-world map the player is in no instance and the GeneralInstanceHandler's onStopTraining is empty, so they send nothing.

#include "../cm_ak/ItemPacketTestSupport.h"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "aion/commons/utils/Rnd.h"
#include "aion/gameserver/configs/administration/AdminConfig.h"
#include "aion/gameserver/dataholders/TitleData.bind.h"
#include "aion/gameserver/dataholders/TitleData.h"
#include "aion/gameserver/model/gameobjects/player/DeniedStatus.h"
#include "aion/gameserver/model/gameobjects/player/DeniedStatusInfo.h"
#include "aion/gameserver/model/gameobjects/player/Friend.h"
#include "aion/gameserver/model/gameobjects/player/FriendList.h"
#include "aion/gameserver/model/gameobjects/player/PlayerSettings.h"
#include "aion/gameserver/model/gameobjects/player/title/TitleList.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"
#include "aion/gameserver/network/aion/clientpackets/CM_CHAT_PLAYER_INFO.h"
#include "aion/gameserver/network/aion/clientpackets/CM_CLIENT_COMMAND_ROLL.h"
#include "aion/gameserver/network/aion/clientpackets/CM_INSTANCE_LEAVE.h"
#include "aion/gameserver/network/aion/clientpackets/CM_SET_NOTE.h"
#include "aion/gameserver/network/aion/clientpackets/CM_STOP_TRAINING.h"
#include "aion/gameserver/network/aion/clientpackets/CM_TITLE_SET.h"
#include "aion/gameserver/network/aion/clientpackets/CM_UNWRAP_ITEM.h"
#include "aion/gameserver/network/aion/clientpackets/CM_VIEW_PLAYER_DETAILS.h"
#include "aion/gameserver/network/aion/serverpackets/SM_CHAT_WINDOW.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/network/aion/serverpackets/SM_UNWRAP_ITEM.h"
#include "aion/gameserver/network/aion/serverpackets/SM_UPDATE_NOTE.h"
#include "aion/gameserver/runtime/base/Finally.h"
#include "aion/gameserver/world/World.h"
#include "aion/gameserver/world/knownlist/KnownList.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets::testing::items {
namespace {

using model::gameobjects::Item;
using model::gameobjects::player::DeniedStatus;
using network::test::PacketWriter;
using serverpackets::SM_SYSTEM_MESSAGE;

// the decoded opcodes of ClientPacketInfo.gen.inc (Java AionClientPacketFactory: packets[n], State.IN_GAME)
constexpr int32_t CM_UNWRAP_ITEM_OPCODE = 240;          // :200
constexpr int32_t CM_SET_NOTE_OPCODE = 58;              // :67
constexpr int32_t CM_CLIENT_COMMAND_ROLL_OPCODE = 107;  // :101
constexpr int32_t CM_VIEW_PLAYER_DETAILS_OPCODE = 100;  // :98
constexpr int32_t CM_TITLE_SET_OPCODE = 139;            // :126
constexpr int32_t CM_CHAT_PLAYER_INFO_OPCODE = 39;      // :50
constexpr int32_t CM_INSTANCE_LEAVE_OPCODE = 46;        // :57
constexpr int32_t CM_STOP_TRAINING_OPCODE = 84;         // :90

constexpr int32_t SM_FRIEND_LIST_OPCODE = opcodeOf<serverpackets::SM_FRIEND_LIST>;
constexpr int32_t SM_VIEW_PLAYER_DETAILS_OPCODE = opcodeOf<serverpackets::SM_VIEW_PLAYER_DETAILS>;
constexpr int32_t SM_TITLE_INFO_OPCODE = opcodeOf<serverpackets::SM_TITLE_INFO>;

/** one Asmodian-or-Elyos title row (the player is ELYOS, makePlayer's default) */
const char* const TITLES_XML = R"(<player_titles><title id="4" nameId="1100903" desc="Tree Hugger" race="PC_ALL"/></player_titles>)";

/** KnownList::addPair / delPair are protected (the visibility update calls them); the test pairs two players directly */
struct KnownListAccess : world::knownlist::KnownList {
	using KnownList::addPair;
	using KnownList::delPair;
};

class SmallPlayerPacketsTest : public ItemPacketTest {
protected:
	void TearDown() override {
		if (buddy.player) {
			if (buddyNear)
				KnownListAccess::delPair(player(), *buddy.player);
			world::World::getInstance().removeObject(*buddy.player);
			buddy.player->setClientConnection(nullptr);
		}
		buddyClient.reset();
		buddy = {};
		dataholders::DataManager::TITLE_DATA.resetForTests();
		ItemPacketTest::TearDown();
	}

	template <class P>
	void run(int32_t opcode, const std::vector<uint8_t>& body = {}) {
		Driver<P> packet(opcode);
		packet.readAndRun(body, client->get());
	}

	/** A second player on its own recording connection, online in the World; `near`: in the player's known list (both ways) */
	void addBuddy(bool near) {
		buddy = makePlayer(710102, 9902, "Buddy");
		buddyClient = std::make_unique<TestClient>();
		buddyClient->enterWorld(buddy);
		world::World::getInstance().storeObject(*buddy.player);
		if (near) {
			buddy.player->setPosition(world::WorldPosition::create(210010000, 101.0f, 100.0f, 50.0f, int8_t{0}, mapInstance->getRegion(101.0f, 100.0f, 50.0f)));
			buddy.player->getPosition()->setIsSpawned(true); // addPair pairs spawned objects only (KnownList.cpp:116)
			buddyNear = KnownListAccess::addPair(player(), *buddy.player);
			ASSERT_TRUE(buddyNear);
		}
		clearSent();
		(*buddyClient)->clearSent();
	}

	std::vector<std::vector<uint8_t>> buddySent() { return (*buddyClient)->sentBytes(); }

	PlayerFixture buddy;
	std::unique_ptr<TestClient> buddyClient;
	bool buddyNear = false;
	xml::LoadContext titleContext;
};

// ---------------------------------------------------------------------------------------------------------------------- CM_UNWRAP_ITEM

TEST_F(SmallPlayerPacketsTest, APackagedItemIsUnwrapped) {
	Item& juice = stored(750201, MERCENARYS_FRUIT_JUICE, 5);
	juice.setPackCount(3);
	juice.setPersistentState(Item::PersistentState::UPDATED);

	run<CM_UNWRAP_ITEM>(CM_UNWRAP_ITEM_OPCODE, PacketWriter().D(750201).data);

	// CM_UNWRAP_ITEM.java:37-42: SM_UNWRAP_ITEM(objectId, packCount), the count turns negative (unwrapped), the item is saved and updated
	const std::vector<std::vector<uint8_t>> packets = sent();
	ASSERT_EQ(packets.size(), 2u);
	EXPECT_EQ(packets[0], serializedFor(serverpackets::SM_UNWRAP_ITEM(750201, 3)));
	EXPECT_EQ(packetsOf({packets[1]}, SM_INVENTORY_UPDATE_ITEM_OPCODE).size(), 1u);
	EXPECT_EQ(juice.getPackCount(), -3);
	EXPECT_EQ(juice.getPersistentState(), Item::PersistentState::UPDATE_REQUIRED);
}

TEST_F(SmallPlayerPacketsTest, AnItemThatIsNotPackagedOrNotThereIsLeftAlone) {
	Item& juice = stored(750202, MERCENARYS_FRUIT_JUICE, 5);
	clearSent();
	for (const int32_t packCount : {0, -3}) { // never packaged, and already unwrapped
		SCOPED_TRACE("pack count " + std::to_string(packCount));
		juice.setPackCount(packCount);
		run<CM_UNWRAP_ITEM>(CM_UNWRAP_ITEM_OPCODE, PacketWriter().D(750202).data);
		EXPECT_TRUE(sent().empty());
		EXPECT_EQ(juice.getPackCount(), packCount);
	}
	run<CM_UNWRAP_ITEM>(CM_UNWRAP_ITEM_OPCODE, PacketWriter().D(750299).data); // :35-36: no such item in the inventory
	EXPECT_TRUE(sent().empty());
}

// ------------------------------------------------------------------------------------------------------------------------- CM_SET_NOTE

TEST_F(SmallPlayerPacketsTest, ANewNoteIsStoredAndShownAndOnlineFriendsAreRefreshed) {
	addBuddy(false);
	player().getFriendList().addFriend(*model::gameobjects::player::Friend::create(*buddy.commonData, ""));

	run<CM_SET_NOTE>(CM_SET_NOTE_OPCODE, PacketWriter().S("Back in five").data);

	// CM_SET_NOTE.java:36-43: the note is stored, every online friend gets a fresh SM_FRIEND_LIST, the player (and who sees him) SM_UPDATE_NOTE
	EXPECT_EQ(player().getCommonData()->getNote(), "Back in five");
	EXPECT_EQ(sent(), exactly({serializedFor(serverpackets::SM_UPDATE_NOTE(player()))}));
	EXPECT_EQ(packetsOf(buddySent(), SM_FRIEND_LIST_OPCODE).size(), 1u);
}

TEST_F(SmallPlayerPacketsTest, TheSameNoteAgainChangesNothing) {
	player().getCommonData()->setNote("Back in five");
	addBuddy(false);
	player().getFriendList().addFriend(*model::gameobjects::player::Friend::create(*buddy.commonData, ""));

	run<CM_SET_NOTE>(CM_SET_NOTE_OPCODE, PacketWriter().S("Back in five").data);

	EXPECT_TRUE(sent().empty()) << ":35-36: the note equals the stored one - return";
	EXPECT_TRUE(buddySent().empty());
}

// --------------------------------------------------------------------------------------------------------------- CM_CLIENT_COMMAND_ROLL

TEST_F(SmallPlayerPacketsTest, ARollGoesToThePlayerAndEveryoneAround) {
	addBuddy(true);
	for (const auto& [asked, max] : std::vector<std::pair<int32_t, int32_t>>{{50, 50}, {0, 100}, {-5, 100}}) {
		SCOPED_TRACE("/roll " + std::to_string(asked));
		run<CM_CLIENT_COMMAND_ROLL>(CM_CLIENT_COMMAND_ROLL_OPCODE, PacketWriter().D(asked).data);

		// CM_CLIENT_COMMAND_ROLL.java:33-37: a maximum of 0 or less means 100; the roll is in [1, max], told to the player and to the others
		const std::vector<std::vector<uint8_t>> mine = sent();
		const std::vector<std::vector<uint8_t>> theirs = buddySent();
		ASSERT_EQ(mine.size(), 1u);
		ASSERT_EQ(theirs.size(), 1u);
		int32_t rolled = 0;
		for (int32_t roll = 1; roll <= max && rolled == 0; roll++)
			if (mine[0] == serializedFor(SM_SYSTEM_MESSAGE::STR_MSG_DICE_CUSTOM_ME(roll, max)))
				rolled = roll;
		ASSERT_NE(rolled, 0) << "STR_MSG_DICE_CUSTOM_ME(roll, " << max << ") with a roll in [1, " << max << "]";
		EXPECT_EQ(theirs[0], serialized(SM_SYSTEM_MESSAGE::STR_MSG_DICE_CUSTOM_OTHER("Holder", rolled, max), buddyClient->con()));
		clearSent();
		(*buddyClient)->clearSent();
	}
}

// --------------------------------------------------------------------------------------------------------------- CM_VIEW_PLAYER_DETAILS

TEST_F(SmallPlayerPacketsTest, APlayerAroundCanBeInspectedUnlessHeDeniesIt) {
	// admin.properties' gameserver.administration.view_player_details = 5 (AdminConfig.cpp:17); the test process loads no properties, and with
	// the field's 0 every access level (the player's 0 too) could inspect a player who denies it
	const int8_t previousLevel = configs::administration::AdminConfig::VIEW_PLAYER_DETAILS.load();
	configs::administration::AdminConfig::VIEW_PLAYER_DETAILS.store(5);
	auto restore = runtime::finally([previousLevel]() noexcept { configs::administration::AdminConfig::VIEW_PLAYER_DETAILS.store(previousLevel); });
	addBuddy(true);

	run<CM_VIEW_PLAYER_DETAILS>(CM_VIEW_PLAYER_DETAILS_OPCODE, PacketWriter().D(710102).data);
	EXPECT_EQ(packetsOf(sent(), SM_VIEW_PLAYER_DETAILS_OPCODE).size(), 1u) << "CM_VIEW_PLAYER_DETAILS.java:35-36";
	clearSent();

	buddy.player->getPlayerSettings()->setDeny(model::gameobjects::player::getId(DeniedStatus::VIEW_DETAILS));
	run<CM_VIEW_PLAYER_DETAILS>(CM_VIEW_PLAYER_DETAILS_OPCODE, PacketWriter().D(710102).data);
	EXPECT_EQ(sent(), exactly({serializedFor(SM_SYSTEM_MESSAGE::STR_MSG_REJECTED_WATCH("Buddy"))})) << ":37-38";
}

TEST_F(SmallPlayerPacketsTest, APlayerOutOfSightCannotBeInspected) {
	addBuddy(false); // online, but not in the player's known list

	run<CM_VIEW_PLAYER_DETAILS>(CM_VIEW_PLAYER_DETAILS_OPCODE, PacketWriter().D(710102).data);

	EXPECT_TRUE(sent().empty()) << "CM_VIEW_PLAYER_DETAILS.java:32-34: getKnownList().getPlayer finds nobody - return";
}

// ------------------------------------------------------------------------------------------------------------------------ CM_TITLE_SET

TEST_F(SmallPlayerPacketsTest, OnlyAnOwnedTitleOrNoTitleCanBeDisplayed) {
	dataholders::DataManager::TITLE_DATA.publish(xml::bindString<dataholders::TitleData>(titleContext, TITLES_XML));
	player().getTitleList().setOwner(player()); // Java Player.setTitleList binds the list's owner at load (PlayerService); the fixture does not load

	run<CM_TITLE_SET>(CM_TITLE_SET_OPCODE, PacketWriter().H(4).data);
	EXPECT_TRUE(sent().empty()) << "CM_TITLE_SET.java:28-30: a title the player does not have - return";
	EXPECT_NE(player().getCommonData()->getTitleId(), 4);

	player().getTitleList().addEntry(4, 0);
	run<CM_TITLE_SET>(CM_TITLE_SET_OPCODE, PacketWriter().H(4).data);
	EXPECT_EQ(player().getCommonData()->getTitleId(), 4) << ":31: setDisplayTitle";
	EXPECT_FALSE(packetsOf(sent(), SM_TITLE_INFO_OPCODE).empty());
	clearSent();

	run<CM_TITLE_SET>(CM_TITLE_SET_OPCODE, PacketWriter().H(0xFFFF).data); // read unsigned: 65535, no title
	EXPECT_EQ(player().getCommonData()->getTitleId(), 0xFFFF) << ":28: 0xFFFF skips the ownership check";
}

// ----------------------------------------------------------------------------------------------------------------- CM_CHAT_PLAYER_INFO

TEST_F(SmallPlayerPacketsTest, AChatWindowOpensForAPlayerOutOfSightOnly) {
	run<CM_CHAT_PLAYER_INFO>(CM_CHAT_PLAYER_INFO_OPCODE, PacketWriter().S("Nobody").data);
	EXPECT_EQ(sent(), exactly({serializedFor(SM_SYSTEM_MESSAGE::STR_NO_SUCH_USER("Nobody"))})) << "CM_CHAT_PLAYER_INFO.java:31-34";
	clearSent();

	addBuddy(false);
	run<CM_CHAT_PLAYER_INFO>(CM_CHAT_PLAYER_INFO_OPCODE, PacketWriter().S("Buddy").data);
	EXPECT_EQ(sent(), exactly({serializedFor(serverpackets::SM_CHAT_WINDOW(*buddy.player, false))})) << ":36-37: online, not in sight";
}

TEST_F(SmallPlayerPacketsTest, NoChatWindowForAPlayerInSight) {
	addBuddy(true);

	run<CM_CHAT_PLAYER_INFO>(CM_CHAT_PLAYER_INFO_OPCODE, PacketWriter().S("Buddy").data);

	EXPECT_TRUE(sent().empty()) << "CM_CHAT_PLAYER_INFO.java:36: the player knows the target - nothing";
}

// ------------------------------------------------------------------------------------------------- CM_INSTANCE_LEAVE / CM_STOP_TRAINING

TEST_F(SmallPlayerPacketsTest, LeavingAndStopTrainingOutsideAnInstanceDoNothing) {
	ASSERT_FALSE(player().isInInstance());

	run<CM_INSTANCE_LEAVE>(CM_INSTANCE_LEAVE_OPCODE); // CM_INSTANCE_LEAVE.java:26: not in an instance
	run<CM_STOP_TRAINING>(CM_STOP_TRAINING_OPCODE);   // CM_STOP_TRAINING.java:26: GeneralInstanceHandler.onStopTraining is empty

	EXPECT_TRUE(sent().empty());
	EXPECT_TRUE(player().getPosition()->isSpawned());
}

} // namespace
} // namespace aion::gameserver::network::aion::clientpackets::testing::items
