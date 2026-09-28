// M5c K-02 (m5c-plan.md §5, P5-16; W-23): CM_SELECT_DECOMPOSABLE (C_SELECT_DISASSEMBLY_ITEM), the player's pick of a selectable
// decomposable's reward - the answer to SM_FIRST_SHOW_DECOMPOSABLE, which DecomposeAction sends for a selectable bundle (stage 0, E-04).
//
// Java: CM_SELECT_DECOMPOSABLE.java:38-68. The read case lays the body out from the Java readImpl (D object id, D unknown, the index as an
// unsigned byte); the run cases drive runImpl on the economy packet fixture (EconomyPacketTestSupport.h) with two selectable rows of
// decomposable_items.xml, verbatim: the [Event] Dandy Form Candy Box (:26565, six candies, three per race) and the [Event] Cold Box (:7309, five
// hats and three Level 60 Composite Manastone Bundles). The index counts the rewards the player can obtain (ResultedItem.isObtainableFor: the
// holder is an ELYOS warrior), one past them is refused; the box is used up by one, and the reward added with the row's count
// (Rnd.get(min, max): no selectable row of the shipped data has a max_count, so the count is min_count).

#include "../cm_ak/EconomyPacketTestSupport.h"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "aion/gameserver/network/aion/clientpackets/CM_SELECT_DECOMPOSABLE.h"
#include "aion/gameserver/network/aion/serverpackets/SM_INVENTORY_ADD_ITEM.h"
#include "aion/gameserver/network/aion/serverpackets/SM_INVENTORY_UPDATE_ITEM.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/services/item/ItemPacketService.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets {

std::unique_ptr<AionClientPacket> CM_SELECT_DECOMPOSABLE_clientPacketFactory(int32_t opcode, const StateSet& validStates);

/** The friend CM_SELECT_DECOMPOSABLE.h declares: the fields readImpl decoded, which Java keeps private */
struct CM_SELECT_DECOMPOSABLETestAccess {
	static int32_t objectId(const CM_SELECT_DECOMPOSABLE& p) { return p.objectId; }
	static int32_t unk(const CM_SELECT_DECOMPOSABLE& p) { return p.unk; }
	static int32_t index(const CM_SELECT_DECOMPOSABLE& p) { return p.index; }
};

namespace testing::items {
namespace {

using Access = CM_SELECT_DECOMPOSABLETestAccess;
using serverpackets::SM_INVENTORY_ADD_ITEM;
using serverpackets::SM_INVENTORY_UPDATE_ITEM;
using serverpackets::SM_SYSTEM_MESSAGE;
using services::item::ItemPacketService;

/** the decoded opcode of ClientPacketInfo.gen.inc (Java AionClientPacketFactory.java:264, State.IN_GAME) */
constexpr int32_t CM_SELECT_DECOMPOSABLE_OPCODE = 236;

constexpr int32_t CANDY_BOX = 900001;
constexpr int32_t COLD_BOX_ITEM = 900002;
constexpr int32_t POTIONS = 900003;
constexpr int32_t CANDIES = 900004;
constexpr int32_t WATCHER = 710404;

TEST(SelectDecomposableReadTest, TheBodyIsTheItemAnUnknownIntAndAnUnsignedByteIndex) {
	int32_t unread = -1;
	auto p = readAlone<CM_SELECT_DECOMPOSABLE>(CM_SELECT_DECOMPOSABLE_OPCODE, PacketWriter().D(0x0A0B0C0D).D(-7).C(0xFE).data, unread);
	ASSERT_NE(p, nullptr);
	EXPECT_EQ(Access::objectId(*p), 0x0A0B0C0D);
	EXPECT_EQ(Access::unk(*p), -7);
	EXPECT_EQ(Access::index(*p), 254) << "readUC";
	EXPECT_EQ(unread, 0);
}

TEST(SelectDecomposableReadTest, TheMarkerRegistersTheClassUnderItsJavaOpcode) {
	EXPECT_NE(dynamic_cast<CM_SELECT_DECOMPOSABLE*>(
				  CM_SELECT_DECOMPOSABLE_clientPacketFactory(CM_SELECT_DECOMPOSABLE_OPCODE, StateSet{AionConnection_State::IN_GAME}).get()),
		nullptr);
	EXPECT_EQ(economyTableEntries("CM_SELECT_DECOMPOSABLE", CM_SELECT_DECOMPOSABLE_OPCODE), 1);
}

class SelectDecomposableTest : public EconomyPacketTest {
protected:
	void select(int32_t objectId, int32_t index) {
		readAndRun<CM_SELECT_DECOMPOSABLE>(CM_SELECT_DECOMPOSABLE_OPCODE, PacketWriter().D(objectId).D(0).C(index).data);
	}

	/** SM_SECONDARY_SHOW_DECOMPOSABLE(objectId, emptyList) (SM_SECONDARY_SHOW_DECOMPOSABLE.java writeImpl: D object, D 0, C size) */
	static std::vector<uint8_t> emptySecondaryShow(int32_t objectId) {
		return javaPacket(SM_SECONDARY_SHOW_DECOMPOSABLE_OPCODE, PacketWriter().D(objectId).D(0).C(0));
	}
};

// :55-65: the ELYOS rewards of the candy box are, in the row's order, Clever (index 0), Swift (1) and Stalwart (2). A player who sees the
// holder gets the usage animation too (:59 broadcastPacketAndReceive), and nothing else
TEST_F(SelectDecomposableTest, TheIndexCountsOnlyTheRewardsOfThePlayersRace) {
	Item& box = stored(CANDY_BOX, DANDY_FORM_CANDY_BOX, 2);
	std::string boxName = box.getL10n();
	OtherPlayer& watcher = otherPlayer(WATCHER, "Watcher", model::Race::ELYOS, 102.0f, true);
	watcher.clearSent();

	select(CANDY_BOX, 2);

	EXPECT_EQ(countOf(player(), STALWART_DANDI_CANDY_ELYOS), 1);
	EXPECT_EQ(countOf(player(), SWIFT_DANDI_CANDY_ASMODIANS), 0) << "the unfiltered list's index 2";
	EXPECT_EQ(countOf(player(), DANDY_FORM_CANDY_BOX), 1) << "one box used up";
	std::vector<std::vector<uint8_t>> packets = sent();
	// the usage animation to the player and those around him, the message, the box's new count (decreaseByObjectId(objectId, 1)), the empty
	// secondary list, then ItemService.addItem's new stack and cube size
	ASSERT_EQ(opcodesOf(packets), (std::vector<int32_t>{SM_ITEM_USAGE_ANIMATION_OPCODE, SM_SYSTEM_MESSAGE_OPCODE, SM_INVENTORY_UPDATE_ITEM_OPCODE,
									  SM_SECONDARY_SHOW_DECOMPOSABLE_OPCODE, SM_INVENTORY_ADD_ITEM_OPCODE, SM_CUBE_UPDATE_OPCODE}));
	// SM_ITEM_USAGE_ANIMATION(player, object, item): time 0, end 1, unk 1 (SM_ITEM_USAGE_ANIMATION.java:22-30)
	EXPECT_EQ(packets[0], usageAnimation(player().getObjectId(), CANDY_BOX, DANDY_FORM_CANDY_BOX, 0, 1, 1));
	EXPECT_EQ(packets[1], serializedFor(SM_SYSTEM_MESSAGE::STR_UNCOMPRESS_COMPRESSED_ITEM_SUCCEEDED(boxName)));
	EXPECT_EQ(packets[3], emptySecondaryShow(CANDY_BOX));
	// :66-67: the new stack goes out with the predicate's add type, ItemAddType.DECOMPOSABLE (mask 0x50)
	std::vector<runtime::Ptr<Item>> candies = player().getInventory().getItemsByItemId(STALWART_DANDI_CANDY_ELYOS);
	ASSERT_EQ(candies.size(), 1u);
	EXPECT_EQ(packets[4], serializedFor(SM_INVENTORY_ADD_ITEM({candies[0]}, player(), ItemPacketService::ItemAddType::DECOMPOSABLE)));
	EXPECT_EQ(watcher.sent(), exactly({usageAnimation(player().getObjectId(), CANDY_BOX, DANDY_FORM_CANDY_BOX, 0, 1, 1)}));
}

// :66-67: a reward the player has a stack of grows that stack (ItemService.addStackableItem: Storage.increaseItemCount with the predicate's
// update type), sent as SM_INVENTORY_UPDATE_ITEM with ItemUpdateType.INC_ITEM_COLLECT (0x19) - no new stack, no cube size
TEST_F(SelectDecomposableTest, ARewardThePlayerHasAStackOfGrowsItAsACollect) {
	stored(CANDY_BOX, DANDY_FORM_CANDY_BOX, 2);
	Item& candies = stored(CANDIES, CLEVER_DANDI_CANDY_ELYOS, 5);

	select(CANDY_BOX, 0);

	EXPECT_EQ(candies.getItemCount(), 6);
	EXPECT_EQ(countOf(player(), CLEVER_DANDI_CANDY_ELYOS), 6) << "the one stack";
	std::vector<std::vector<uint8_t>> packets = sent();
	ASSERT_EQ(opcodesOf(packets), (std::vector<int32_t>{SM_ITEM_USAGE_ANIMATION_OPCODE, SM_SYSTEM_MESSAGE_OPCODE, SM_INVENTORY_UPDATE_ITEM_OPCODE,
									  SM_SECONDARY_SHOW_DECOMPOSABLE_OPCODE, SM_INVENTORY_UPDATE_ITEM_OPCODE}));
	EXPECT_EQ(packets[4], serializedFor(SM_INVENTORY_UPDATE_ITEM(player(), candies, ItemPacketService::ItemUpdateType::INC_ITEM_COLLECT)));
}

// :67: addItem's allowInventoryOverflow is true - a full cube still takes the reward as a new stack, past the cube's limit, without
// STR_MSG_DICE_INVEN_ERROR (ItemService.addStackableItem's loop and addItem's message ask it)
TEST_F(SelectDecomposableTest, AFullCubeStillTakesTheReward) {
	stored(CANDY_BOX, DANDY_FORM_CANDY_BOX, 2);
	model::items::storage::Storage& cube = storage(StorageType::CUBE);
	for (int32_t objId = 900100; !cube.isFull() && objId < 901000; ++objId)
		stored(objId, SPARKIE_CARAPACE_FRAGMENT, 1);
	ASSERT_TRUE(cube.isFull()) << cube.size() << " items, limit " << cube.getLimit();
	const int32_t limit = cube.getLimit();

	select(CANDY_BOX, 0);

	EXPECT_EQ(countOf(player(), CLEVER_DANDI_CANDY_ELYOS), 1);
	EXPECT_EQ(static_cast<int32_t>(cube.size()), limit + 1);
	EXPECT_EQ(opcodesOf(sent()), (std::vector<int32_t>{SM_ITEM_USAGE_ANIMATION_OPCODE, SM_SYSTEM_MESSAGE_OPCODE, SM_INVENTORY_UPDATE_ITEM_OPCODE,
									 SM_SECONDARY_SHOW_DECOMPOSABLE_OPCODE, SM_INVENTORY_ADD_ITEM_OPCODE, SM_CUBE_UPDATE_OPCODE}))
		<< "one message only: the success";
}

// :56-58: one past the player's rewards is refused before anything happens (index + 1 > size)
TEST_F(SelectDecomposableTest, AnIndexPastThePlayersRewardsIsRefused) {
	stored(CANDY_BOX, DANDY_FORM_CANDY_BOX, 2);

	select(CANDY_BOX, 3);
	select(CANDY_BOX, 255);

	EXPECT_EQ(countOf(player(), DANDY_FORM_CANDY_BOX), 2);
	EXPECT_EQ(countOf(player(), SWIFT_DANDI_CANDY_ASMODIANS), 0) << "the unfiltered list's index 3";
	EXPECT_TRUE(sent().empty());
}

// :63-65: the first index, and the reward's count is the row's min_count; the last box goes and the reward still comes
TEST_F(SelectDecomposableTest, TheRewardCountIsTheRowsCountAndTheLastBoxIsUsedUp) {
	stored(COLD_BOX_ITEM, COLD_BOX, 1);

	select(COLD_BOX_ITEM, 5);

	EXPECT_EQ(countOf(player(), COMPOSITE_MANASTONE_BUNDLE), 3) << "decomposable_items.xml:7315 min_count 3";
	EXPECT_FALSE(player().getInventory().getItemByObjId(COLD_BOX_ITEM));
	EXPECT_EQ(packetsOf(sent(), SM_DELETE_ITEM_OPCODE).size(), 1u);

	clearSent();
	stored(CANDY_BOX, DANDY_FORM_CANDY_BOX, 1);
	select(CANDY_BOX, 0);
	EXPECT_EQ(countOf(player(), CLEVER_DANDI_CANDY_ELYOS), 1);
}

// :49-54: an item not in the cube, or one without selectable rewards, is not decomposed
TEST_F(SelectDecomposableTest, AnItemNotInTheCubeOrWithoutSelectableRewardsIsIgnored) {
	stored(POTIONS, MINOR_LIFE_POTION, 10);

	select(CANDY_BOX, 0);
	select(POTIONS, 0);

	EXPECT_EQ(countOf(player(), MINOR_LIFE_POTION), 10);
	EXPECT_TRUE(sent().empty());
}

// :47-48
TEST_F(SelectDecomposableTest, WithoutAPlayerNothingIsDecomposed) {
	stored(CANDY_BOX, DANDY_FORM_CANDY_BOX, 2);
	TestClient loggedOut;
	EconomyDriver<CM_SELECT_DECOMPOSABLE> packet(CM_SELECT_DECOMPOSABLE_OPCODE);
	ASSERT_TRUE(packet.readOn(PacketWriter().D(CANDY_BOX).D(0).C(0).data, loggedOut.get()));

	EXPECT_NO_THROW(packet.runNow());
	EXPECT_EQ(countOf(player(), DANDY_FORM_CANDY_BOX), 2);
}

} // namespace
} // namespace testing::items
} // namespace aion::gameserver::network::aion::clientpackets
