// The soul-bind accept of Equipment (P4-12, m5b3-plan.md P-04, §2.8 E-12): equipping a soul-bound item asks the player first; accepting starts
// a 5 s item use (Equipment$1.acceptRequest) that an item-use abort cancels (Equipment$2) and whose end binds and equips the item
// (Equipment$3).
//
// Java: Equipment.java:698-780 (soulBindItem and its three anonymous classes). The item is the Manastone Slot Test Superior Sword
// (item_templates.xml:2062, mask 138494 with SOUL_BOUND = 1 << 7, ItemMask.java), the player the item packet fixture's
// (tests/cm_ak/ItemPacketTestSupport.h: a spawned warrior in Poeta who knows the sword skill), and the 5 s run on the fixture's manual clock.

#include "../cm_ak/ItemPacketTestSupport.h"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <vector>

#include "aion/gameserver/controllers/ObserveController.h"
#include "aion/gameserver/controllers/PlayerController.h"
#include "aion/gameserver/controllers/observer/ItemUseObserver.h"
#include "aion/gameserver/model/TaskId.h"
#include "aion/gameserver/model/gameobjects/player/ResponseRequester.h"
#include "aion/gameserver/model/templates/item/ItemTemplate.h"
#include "aion/gameserver/network/aion/serverpackets/SM_INVENTORY_UPDATE_ITEM.h"
#include "aion/gameserver/network/aion/serverpackets/SM_QUESTION_WINDOW.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/network/aion/serverpackets/SM_UPDATE_PLAYER_APPEARANCE.h"
#include "aion/gameserver/runtime/base/Unported.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets::testing::items {
namespace {

using serverpackets::SM_INVENTORY_UPDATE_ITEM;
using serverpackets::SM_QUESTION_WINDOW;
using serverpackets::SM_SYSTEM_MESSAGE;
using serverpackets::SM_UPDATE_PLAYER_APPEARANCE;
using namespace std::chrono_literals;

constexpr int64_t MAIN_HAND = 1; // ItemSlot.MAIN_HAND.getSlotIdMask()
constexpr int32_t SWORD = 791001;

/**
 * SM_ITEM_USAGE_ANIMATION(playerObjId, itemObjId, itemId, time, end) as Java writes it (SM_ITEM_USAGE_ANIMATION.java, the five-argument
 * constructor and writeImpl): D player, D target = player, D item object, D item id, D time, C end, C unk 0, C unk1 0, C unk2 1, D unk3 0
 */
std::vector<uint8_t> usageAnimation(int32_t playerObjId, int32_t itemObjId, int32_t itemId, int32_t time, int32_t end) {
	return javaPacket(SM_ITEM_USAGE_ANIMATION_OPCODE,
		PacketWriter().D(playerObjId).D(playerObjId).D(itemObjId).D(itemId).D(time).C(end).C(0).C(0).C(1).D(0));
}

class SoulBindTest : public ItemPacketTest {
protected:
	void SetUp() override {
		ItemPacketTest::SetUp();
		runtime::resetUnportedHitsForTests();
		sword = &stored(SWORD, SOUL_BOUND_TEST_SWORD, 1);
		ASSERT_TRUE(sword->getItemTemplate()->isSoulBound());
		ASSERT_FALSE(sword->isSoulBound());
		// Equipment.equipItem of an unbound soul-bound item asks first and equips nothing (Equipment.java:138-141, :772-779)
		EXPECT_FALSE(player().getEquipment().equipItem(SWORD, MAIN_HAND));
		EXPECT_EQ(opcodesOf(sent()), (std::vector<int32_t>{SM_QUESTION_WINDOW_OPCODE}));
		clearSent();
	}

	void TearDown() override {
		if (f.player)
			f.player->getController().cancelTask(model::TaskId::ITEM_USE);
		sword = nullptr;
		ItemPacketTest::TearDown();
	}

	/** The player's answer to the question window, as CM_QUESTION_RESPONSE hands it to the ResponseRequester */
	void answer(int32_t responseCode) {
		ASSERT_TRUE(player().getResponseRequester().respond(SM_QUESTION_WINDOW::STR_SOUL_BOUND_ITEM_DO_YOU_WANT_SOUL_BOUND, responseCode));
	}

	int32_t playerId() { return player().getObjectId(); }

	Item* sword = nullptr;
};

TEST_F(SoulBindTest, AcceptingStartsAFiveSecondItemUse) {
	answer(1);

	// Equipment.java:728-747: cancelUseItem, the 5 s animation (end 4) to the player, the observer, the ITEM_USE task - and nothing bound yet
	EXPECT_EQ(sent(), exactly({usageAnimation(playerId(), SWORD, SOUL_BOUND_TEST_SWORD, 5000, 4)}));
	EXPECT_TRUE(player().getController().hasScheduledTask(model::TaskId::ITEM_USE));
	EXPECT_FALSE(sword->isSoulBound());
	EXPECT_FALSE(sword->isEquipped());
	EXPECT_EQ(runtime::unportedHitCount(), 0u) << "the accept is no AION_UNPORTED any more";
}

/** An item-use observer that counts its aborts (ItemUseObserver.java) */
struct CountingItemUseObserver final : controllers::observer::ItemUseObserver {
	AION_MAKE_REF_FRIEND

	int32_t aborts = 0;

	static runtime::Ref<CountingItemUseObserver> create() { return runtime::makeRef<CountingItemUseObserver>(); }

	void abort() override { ++aborts; }

protected:
	CountingItemUseObserver() = default;
	~CountingItemUseObserver() override = default;
};

TEST_F(SoulBindTest, AcceptingCancelsAnItemUseInProgressFirst) {
	// Equipment.java:728: responder.getController().cancelUseItem() - another item use is aborted before the binding's own observer is attached
	runtime::Ref<CountingItemUseObserver> other = CountingItemUseObserver::create();
	player().getObserveController()->attach(*other);

	answer(1);

	EXPECT_EQ(other->aborts, 1);
	EXPECT_TRUE(player().getController().hasScheduledTask(model::TaskId::ITEM_USE)) << "the binding's own item use is not aborted";
}

TEST_F(SoulBindTest, AfterFiveSecondsTheItemIsBoundAndEquipped) {
	answer(1);
	clearSent();
	executor->advance(4999ms);
	EXPECT_TRUE(sent().empty()) << "the task runs 5,000 ms later";
	EXPECT_FALSE(sword->isSoulBound());

	// The info packet the task sends (Equipment.java:756-757) goes out after the bind and before the equip: the sword bound, still in the
	// cube. Its item info blob carries the soul-bound byte (EnchantInfoBlobEntry.java:39 `writeC(item.isSoulBound() ? 1 : 0)`), so the
	// packet of the bound sword differs from the unbound one's - a task that sent it before setSoulBound(true) would tell the client the
	// sword is still unbound.
	const std::vector<uint8_t> unboundInfo = serializedFor(SM_INVENTORY_UPDATE_ITEM(player(), *sword));
	sword->setSoulBound(true);
	const std::vector<uint8_t> boundInfo = serializedFor(SM_INVENTORY_UPDATE_ITEM(player(), *sword));
	sword->setSoulBound(false);
	ASSERT_NE(boundInfo, unboundInfo) << "the blob writes the soul-bound byte";

	executor->advance(1ms);

	// Equipment.java:751-761: the observer removed, the closing animation (end 6), STR_SOUL_BOUND_ITEM_SUCCEED, the item bound and its info
	// resent, equip(slot, item), and the new appearance last
	EXPECT_TRUE(sword->isSoulBound());
	EXPECT_TRUE(sword->isEquipped());
	EXPECT_EQ(player().getEquipment().getEquippedItemByObjId(SWORD).get(), sword);
	std::vector<std::vector<uint8_t>> packets = sent();
	ASSERT_GE(packets.size(), 4u);
	EXPECT_EQ(packets[0], usageAnimation(playerId(), SWORD, SOUL_BOUND_TEST_SWORD, 0, 6));
	EXPECT_EQ(packets[1], serializedFor(SM_SYSTEM_MESSAGE::STR_SOUL_BOUND_ITEM_SUCCEED(sword->getL10n())));
	EXPECT_EQ(javaOpcodeOf(packets[2]), SM_INVENTORY_UPDATE_ITEM_OPCODE) << "ItemPacketService.updateItemAfterInfoChange";
	EXPECT_EQ(PacketReader(bodyOf(packets[2])).D(), SWORD);
	EXPECT_EQ(packets[2], boundInfo) << "item.setSoulBound(true) before updateItemAfterInfoChange (Equipment.java:756-757)";
	EXPECT_EQ(packets.back(), serializedFor(SM_UPDATE_PLAYER_APPEARANCE(playerId(), player().getEquipment().getEquippedForAppearance())));
	// the observer went first (:751), so the equip's own observer notification (Equipment.notifyItemEquipped -> ItemUseObserver.equip)
	// aborts nothing: one animation (the closing one) and no cancel message
	EXPECT_EQ(packetsOf(packets, SM_ITEM_USAGE_ANIMATION_OPCODE).size(), 1u);
	EXPECT_EQ(std::ranges::count(packets, serializedFor(SM_SYSTEM_MESSAGE::STR_SOUL_BOUND_ITEM_CANCELED(sword->getL10n()))), 0);
	EXPECT_FALSE(player().getController().hasScheduledTask(model::TaskId::ITEM_USE));

	// the task removed its observer: a later move aborts nothing
	clearSent();
	player().getObserveController()->notifyMoveObservers();
	EXPECT_TRUE(sent().empty());
	EXPECT_EQ(runtime::unportedHitCount(), 0u);
}

TEST_F(SoulBindTest, AMoveDuringTheFiveSecondsCancelsTheBinding) {
	answer(1);
	clearSent();

	player().getObserveController()->notifyMoveObservers(); // ItemUseObserver.moved -> abort (Equipment.java:735-741)

	EXPECT_EQ(sent(), exactly({serializedFor(SM_SYSTEM_MESSAGE::STR_SOUL_BOUND_ITEM_CANCELED(sword->getL10n())),
						  usageAnimation(playerId(), SWORD, SOUL_BOUND_TEST_SWORD, 0, 8)}));
	EXPECT_FALSE(player().getController().hasScheduledTask(model::TaskId::ITEM_USE)) << "abort cancels the task";
	clearSent();
	executor->advance(5000ms);
	EXPECT_TRUE(sent().empty());
	EXPECT_FALSE(sword->isSoulBound());
	EXPECT_FALSE(sword->isEquipped());
}

TEST_F(SoulBindTest, DecliningOnlySaysSo) {
	answer(0);

	EXPECT_EQ(sent(), exactly({serializedFor(SM_SYSTEM_MESSAGE::STR_SOUL_BOUND_ITEM_CANCELED(sword->getL10n()))})) << "Equipment.java:766-769";
	EXPECT_FALSE(player().getController().hasScheduledTask(model::TaskId::ITEM_USE));
	EXPECT_FALSE(sword->isSoulBound());
}

} // namespace
} // namespace aion::gameserver::network::aion::clientpackets::testing::items
