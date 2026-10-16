// CM_EQUIP_ITEM and CM_DELETE_ITEM (P5-15, m5b3-plan.md P-05/P-06): C_USE_EQUIPMENT_ITEM, what the client sends to equip, unequip or switch the
// weapon sets, and C_DESTROY_ITEM, what it sends when the player destroys an item of the cube.
//
// Java: game-server/src/com/aionemu/gameserver/network/aion/clientpackets/CM_EQUIP_ITEM.java:28-63 and CM_DELETE_ITEM.java:25-44.
//
// The byte vectors are laid out field by field from the Java readImpl; the run tests drive runImpl against the fixture of ItemPacketTestSupport.h
// (a spawned warrior in Poeta whose packets a real AionConnection keeps). What the services behind the packets do on their own is their chunks'
// tests' business (Equipment: tests/player and tests/stats; Storage.delete and ItemPacketService: tests/itemsvc); these cases pin what the packet
// adds: its reads, its order of calls (cancelUseItem before canChangeEquip, the restriction before any equipment change), its own answers
// (STR_UI_INVENTORY_FULL on an unequip the cube refuses, STR_UNBREAKABLE_ITEM) and the appearance broadcast; and the client's weapon swap of
// the 2026-09-29 trace, whose stale unequip the C++ refuses silently (a deviation, docs/deviations/P5-15.md).

#include "ItemPacketTestSupport.h"

#include <cstdint>
#include <functional>
#include <initializer_list>
#include <memory>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include "aion/commons/configuration/ConfigValue.h"
#include "aion/commons/utils/ByteBuffer.h"
#include "aion/gameserver/configs/network/NetworkConfig.h"
#include "aion/gameserver/controllers/ObserveController.h"
#include "aion/gameserver/controllers/PlayerController.h"
#include "aion/gameserver/controllers/observer/ItemUseObserver.h"
#include "aion/gameserver/model/TaskId.h"
#include "aion/gameserver/model/stats/container/PlayerGameStats.h"
#include "aion/gameserver/model/templates/item/ItemTemplate.h"
#include "aion/gameserver/network/aion/clientpackets/CM_DELETE_ITEM.h"
#include "aion/gameserver/network/aion/clientpackets/CM_EQUIP_ITEM.h"
#include "aion/gameserver/network/aion/serverpackets/SM_INVENTORY_UPDATE_ITEM.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/network/aion/serverpackets/SM_UPDATE_PLAYER_APPEARANCE.h"
#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/services/item/ItemPacketService_ItemUpdateType.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets {

std::unique_ptr<AionClientPacket> CM_EQUIP_ITEM_clientPacketFactory(int32_t opcode, const StateSet& validStates);
std::unique_ptr<AionClientPacket> CM_DELETE_ITEM_clientPacketFactory(int32_t opcode, const StateSet& validStates);

/** The friend CM_EQUIP_ITEM.h declares: the fields readImpl decoded, which Java keeps private */
struct CM_EQUIP_ITEMTestAccess {
	static int32_t action(const CM_EQUIP_ITEM& p) { return p.action; }
	static int64_t slotRead(const CM_EQUIP_ITEM& p) { return p.slotRead; }
	static int32_t itemObjId(const CM_EQUIP_ITEM& p) { return p.itemObjId; }
};

namespace testing::items {
namespace {

using network::test::LogCapture;
using serverpackets::SM_SYSTEM_MESSAGE;
using serverpackets::SM_UPDATE_PLAYER_APPEARANCE;
using EquipAccess = CM_EQUIP_ITEMTestAccess;

const char* BASE_CLIENT_PACKET_LOGGER = "com.aionemu.commons.network.packet.BaseClientPacket";

/** the decoded opcodes of ClientPacketInfo.gen.inc:49 and :107 (Java AionClientPacketFactory.java:66 and :144, State.IN_GAME) */
constexpr int32_t CM_EQUIP_ITEM_OPCODE = 38;
constexpr int32_t CM_DELETE_ITEM_OPCODE = 116;

constexpr int64_t MAIN_HAND = 1; // ItemSlot.MAIN_HAND.getSlotIdMask()

/** Java CM_EQUIP_ITEM.readImpl: C action, Q slot, D item object id */
std::vector<uint8_t> equipBody(int32_t action, int64_t slot, int32_t itemObjId) {
	return PacketWriter().C(action).Q(slot).D(itemObjId).data;
}

/** A fresh packet of type P that has read `data`; nullptr if read() failed. `unread` is the byte count left */
template <class P>
std::unique_ptr<P> readPacket(const std::vector<uint8_t>& data, int32_t opcode, int32_t& unread) {
	std::vector<uint8_t> copy = data;
	auto packet = std::make_unique<P>(opcode, StateSet{AionConnection_State::IN_GAME});
	packet->setBuffer(commons::utils::ByteBuffer::wrap(copy));
	if (!packet->read())
		return nullptr;
	unread = packet->getRemainingBytes();
	return packet;
}

TEST(EquipDeleteReadTest, EquipReadsAByteALongAndAnInt) {
	LogCapture capture({BASE_CLIENT_PACKET_LOGGER});
	int32_t unread = -1;
	// a slot mask above 32 bits (Java long): readQ must keep the high half
	std::unique_ptr<CM_EQUIP_ITEM> p = readPacket<CM_EQUIP_ITEM>(equipBody(2, 0x0000000180000001LL, 0x7F010203), CM_EQUIP_ITEM_OPCODE, unread);
	ASSERT_NE(p, nullptr);
	EXPECT_EQ(EquipAccess::action(*p), 2);
	EXPECT_EQ(EquipAccess::slotRead(*p), 0x0000000180000001LL);
	EXPECT_EQ(EquipAccess::itemObjId(*p), 0x7F010203);
	EXPECT_EQ(unread, 0) << "1 + 8 + 4 bytes";
	EXPECT_FALSE(capture.contains("Missing")) << capture.dump();

	// readC is signed (Java byte): an action byte 0xFF is -1, which no switch arm takes
	std::unique_ptr<CM_EQUIP_ITEM> negative = readPacket<CM_EQUIP_ITEM>(equipBody(0xFF, 1, 7), CM_EQUIP_ITEM_OPCODE, unread);
	ASSERT_NE(negative, nullptr);
	EXPECT_EQ(EquipAccess::action(*negative), -1);
}

TEST(EquipDeleteReadTest, AShortEquipBodyLogsTheMissingField) {
	LogCapture capture({BASE_CLIENT_PACKET_LOGGER});
	int32_t unread = -1;
	ASSERT_NE(readPacket<CM_EQUIP_ITEM>(PacketWriter().C(0).Q(1).H(5).data, CM_EQUIP_ITEM_OPCODE, unread), nullptr);
	EXPECT_TRUE(capture.contains("Missing D")) << capture.dump();
}

TEST(EquipDeleteReadTest, DeleteReadsTheObjectId) {
	LogCapture capture({BASE_CLIENT_PACKET_LOGGER});
	int32_t unread = -1;
	std::unique_ptr<CM_DELETE_ITEM> p = readPacket<CM_DELETE_ITEM>(PacketWriter().D(-2).C(9).data, CM_DELETE_ITEM_OPCODE, unread);
	ASSERT_NE(p, nullptr);
	EXPECT_EQ(p->itemObjectId, -2) << "a public field in Java too";
	EXPECT_EQ(unread, 1) << "readImpl reads one int only";
	EXPECT_FALSE(capture.contains("Missing")) << capture.dump();
}

TEST(EquipDeleteReadTest, TheMarkersRegisterBothClassesUnderTheirJavaOpcodes) {
	// AION_CLIENT_PACKET(CM_X) defines the factory the generated registry table names; the opcode table is the Java factory's
	std::unique_ptr<AionClientPacket> equip = CM_EQUIP_ITEM_clientPacketFactory(CM_EQUIP_ITEM_OPCODE, StateSet{AionConnection_State::IN_GAME});
	EXPECT_NE(dynamic_cast<CM_EQUIP_ITEM*>(equip.get()), nullptr);
	std::unique_ptr<AionClientPacket> del = CM_DELETE_ITEM_clientPacketFactory(CM_DELETE_ITEM_OPCODE, StateSet{AionConnection_State::IN_GAME});
	EXPECT_NE(dynamic_cast<CM_DELETE_ITEM*>(del.get()), nullptr);
	int32_t found = 0;
#define AION_CLIENT_PACKET_INFO(opcode, wireOpcode, Class, clientName, ...)                                                                             \
	if (std::string_view(#Class) == "CM_EQUIP_ITEM") {                                                                                                  \
		EXPECT_EQ(opcode, CM_EQUIP_ITEM_OPCODE);                                                                                                        \
		++found;                                                                                                                                        \
	} else if (std::string_view(#Class) == "CM_DELETE_ITEM") {                                                                                          \
		EXPECT_EQ(opcode, CM_DELETE_ITEM_OPCODE);                                                                                                       \
		++found;                                                                                                                                        \
	}
#include "aion/gameserver/network/aion/ClientPacketInfo.gen.inc"
#undef AION_CLIENT_PACKET_INFO
	EXPECT_EQ(found, 2);
}

/** An item-use observer that records its abort and, like every Java one, cancels the ITEM_USE task (Equipment.java:737) */
struct RecordingItemUseObserver final : controllers::observer::ItemUseObserver {
	AION_MAKE_REF_FRIEND

	const runtime::Ref<model::gameobjects::player::Player> player;
	int32_t aborts = 0;

	static runtime::Ref<RecordingItemUseObserver> create(model::gameobjects::player::Player& playerValue) {
		return runtime::makeRef<RecordingItemUseObserver>(playerValue);
	}

	void abort() override {
		++aborts;
		player->getController().cancelTask(model::TaskId::ITEM_USE);
	}

protected:
	explicit RecordingItemUseObserver(model::gameobjects::player::Player& playerValue) : player(playerValue) {}
	~RecordingItemUseObserver() override = default;
};

class EquipDeleteRunTest : public ItemPacketTest {
protected:
	void SetUp() override {
		ItemPacketTest::SetUp();
		runtime::resetUnportedHitsForTests();
		runtime::resetPartialHitsForTests();
	}

	void TearDown() override {
		if (f.player)
			f.player->getController().cancelTask(model::TaskId::ITEM_USE);
		ItemPacketTest::TearDown();
	}

	void equip(int32_t action, int64_t slot, int32_t itemObjId) {
		Driver<CM_EQUIP_ITEM> packet(CM_EQUIP_ITEM_OPCODE);
		packet.readAndRun(equipBody(action, slot, itemObjId), client->get());
	}

	void destroy(int32_t itemObjId) {
		Driver<CM_DELETE_ITEM> packet(CM_DELETE_ITEM_OPCODE);
		packet.readAndRun(PacketWriter().D(itemObjId).data, client->get());
	}

	/** SM_UPDATE_PLAYER_APPEARANCE of the player's current equipment, as the packet builds it (CM_EQUIP_ITEM.java:61-62) */
	std::vector<uint8_t> appearance() {
		return serializedFor(SM_UPDATE_PLAYER_APPEARANCE(player().getObjectId(), player().getEquipment().getEquippedForAppearance()));
	}

	/** A long ITEM_USE task, as an item use in progress holds it (nothing runs it: the manual clock does not move) */
	void startItemUseTask() {
		player().getController().addTask(model::TaskId::ITEM_USE, utils::ThreadPoolManager::getInstance().schedule([] {}, 60000));
		ASSERT_TRUE(player().getController().hasScheduledTask(model::TaskId::ITEM_USE));
	}
};

TEST_F(EquipDeleteRunTest, AnEquipEndsWithTheNewAppearanceBroadcastToThePlayer) {
	Item& sword = stored(720001, TRAINING_SWORD, 1);

	equip(0, MAIN_HAND, 720001);

	EXPECT_TRUE(sword.isEquipped()) << "action 0 -> Equipment.equipItem";
	EXPECT_EQ(player().getEquipment().getEquippedItemByObjId(720001).get(), &sword);
	std::vector<std::vector<uint8_t>> packets = sent();
	ASSERT_FALSE(packets.empty());
	// CM_EQUIP_ITEM.java:60-62: the result is not null, so the appearance goes to the player (broadcastPacket(..., true)) after everything
	// equipItem sent; it names the sword
	EXPECT_EQ(packets.back(), appearance());
	EXPECT_EQ(packetsOf(packets, SM_UPDATE_PLAYER_APPEARANCE_OPCODE).size(), 1u);
	EXPECT_EQ(player().getEquipment().getEquippedForAppearance().size(), 1u);
	EXPECT_EQ(runtime::unportedHitCount(), 0u) << "canChangeEquip, notifyEquipAction, updateItemAfterEquip, onItemEquipment are ported";
}

TEST_F(EquipDeleteRunTest, AnEquipThatFailsSendsNoAppearance) {
	// no such object in the cube: equipItem answers null (Equipment.java:60-62) and the packet broadcasts nothing
	equip(0, MAIN_HAND, 720099);

	EXPECT_TRUE(sent().empty());
}

TEST_F(EquipDeleteRunTest, AnUnequipBroadcastsTheAppearanceWithoutTheItem) {
	Item& sword = equipped(720002, TRAINING_SWORD, MAIN_HAND);
	ASSERT_TRUE(sword.isEquipped());

	equip(1, 0, 720002);

	EXPECT_FALSE(sword.isEquipped()) << "action 1 -> Equipment.unEquipItem";
	EXPECT_EQ(storage(StorageType::CUBE).getItemByObjId(720002).get(), &sword);
	std::vector<std::vector<uint8_t>> packets = sent();
	ASSERT_FALSE(packets.empty());
	EXPECT_EQ(packets.back(), appearance());
	EXPECT_TRUE(player().getEquipment().getEquippedForAppearance().empty());
	EXPECT_EQ(packetsOf(packets, SM_SYSTEM_MESSAGE_OPCODE).size(), 0u) << "no STR_UI_INVENTORY_FULL for an unequip that worked";
	EXPECT_EQ(runtime::unportedHitCount(), 0u) << "onItemUnequipment is ported";
}

TEST_F(EquipDeleteRunTest, AnUnequipOfAnItemThatIsNotEquippedFailsSilently) {
	// Deviation (report 3 of the 2026-09-28 play session, docs/deviations/P5-15.md): CM_EQUIP_ITEM.java:51-53 answers every null result of
	// unEquipItem with the full inventory message, whatever the cause. The C++ answers only when the item is still equipped, i.e. when the cube was
	// the reason; an object id that names no equipped item (Equipment.java:235-237) is refused silently - here one that is in the cube, as the
	// second unequip of the client's weapon swap finds it, and one nobody has
	stored(720007, TRAINING_SWORD, 1);

	equip(1, 0, 720007);
	equip(1, 0, 720099);

	EXPECT_TRUE(sent().empty()) << "no STR_UI_INVENTORY_FULL, no appearance";
}

TEST_F(EquipDeleteRunTest, AnUnequipIntoAFullCubeIsRefusedWithInventoryFull) {
	// CM_EQUIP_ITEM.java:51 calls the one-argument unEquipItem, i.e. checkFullInventory = true (Equipment.java:264-266), and with the cube
	// full Equipment.java:230-232 answers null before anything changes: the message the packet is named after, no appearance
	Item& sword = equipped(720006, TRAINING_SWORD, MAIN_HAND);
	model::items::storage::Storage& cube = storage(StorageType::CUBE);
	for (int32_t objId = 720100; !cube.isFull() && objId < 721000; ++objId)
		stored(objId, SPARKIE_CARAPACE_FRAGMENT, 1);
	ASSERT_TRUE(cube.isFull()) << cube.size() << " items, limit " << cube.getLimit();

	equip(1, 0, 720006);

	EXPECT_EQ(sent(), exactly({serializedFor(SM_SYSTEM_MESSAGE::STR_UI_INVENTORY_FULL())}));
	EXPECT_TRUE(sword.isEquipped());
	EXPECT_EQ(player().getEquipment().getEquippedItemByObjId(720006).get(), &sword);
	EXPECT_FALSE(cube.getItemByObjId(720006));
}

TEST_F(EquipDeleteRunTest, ASwitchOfHandsAlwaysBroadcastsTheAppearance) {
	// CM_EQUIP_ITEM.java:55-57, :60: `resultItem != null || action == 2` - with nothing equipped the appearance still goes out, after the
	// stats switchHands itself sends (Equipment.java switchHands: updateStatsAndSpeedVisually)
	equip(2, 0, 0);

	std::vector<std::vector<uint8_t>> packets = sent();
	ASSERT_FALSE(packets.empty());
	EXPECT_EQ(packets.back(), appearance());
	EXPECT_EQ(packetsOf(packets, SM_UPDATE_PLAYER_APPEARANCE_OPCODE).size(), 1u);
}

TEST_F(EquipDeleteRunTest, AnUnknownActionChangesNothingAndSendsNothing) {
	stored(720003, TRAINING_SWORD, 1);

	equip(3, MAIN_HAND, 720003);

	EXPECT_TRUE(sent().empty()) << "no switch arm: resultItem stays null and the action is not 2";
	EXPECT_FALSE(player().getEquipment().getEquippedItemByObjId(720003));
}

TEST_F(EquipDeleteRunTest, AnItemUseInProgressRefusesTheEquipBeforeAnythingChanges) {
	Item& sword = stored(720004, TRAINING_SWORD, 1);
	startItemUseTask(); // a task without an item-use observer, which cancelUseItem cannot abort

	equip(0, MAIN_HAND, 720004);

	// CM_EQUIP_ITEM.java:41-42 -> PlayerRestrictions.canChangeEquip (PlayerRestrictions.java:384-387)
	EXPECT_EQ(sent(), exactly({serializedFor(SM_SYSTEM_MESSAGE::STR_CANT_EQUIP_ITEM_IN_ACTION())}));
	EXPECT_FALSE(sword.isEquipped());
}

TEST_F(EquipDeleteRunTest, TheItemUseInProgressIsCancelledFirst) {
	// CM_EQUIP_ITEM.java:39: cancelUseItem aborts the item-use observers (PlayerController.java:548-550), whose abort cancels the ITEM_USE task
	// - so canChangeEquip, asked next, finds no task and the equip goes through. Without the cancel the case above would answer
	Item& sword = stored(720005, TRAINING_SWORD, 1);
	startItemUseTask();
	runtime::Ref<RecordingItemUseObserver> observer = RecordingItemUseObserver::create(player());
	player().getObserveController()->attach(*observer);

	equip(0, MAIN_HAND, 720005);

	EXPECT_EQ(observer->aborts, 1);
	EXPECT_TRUE(sword.isEquipped());
	EXPECT_TRUE(packetsOf(sent(), SM_SYSTEM_MESSAGE_OPCODE).empty()) << "no STR_CANT_EQUIP_ITEM_IN_ACTION";
}

TEST_F(EquipDeleteRunTest, ABreakableItemIsDestroyedWithTheDiscardDeleteType) {
	Item& junk = stored(720011, SPARKIE_CARAPACE_FRAGMENT, 3);
	stored(720012, MINOR_LIFE_POTION, 100);
	ASSERT_TRUE(junk.getItemTemplate()->isBreakable()) << "mask 12414 has BREAKABLE (1 << 6)";

	destroy(720011);

	// CM_DELETE_ITEM.java:41 -> Storage.delete(item, DISCARD) -> ItemPacketService.sendItemDeletePacket (ItemPacketService.java:178-185):
	// SM_DELETE_ITEM with DISCARD's mask 0x15 (ItemPacketService.java:118), then the cube size of the one item left
	EXPECT_EQ(sent(), exactly({deleteItem(720011, 0x15), cubeSize(StorageType::CUBE, 1)}));
	EXPECT_FALSE(storage(StorageType::CUBE).getItemByObjId(720011));
	EXPECT_EQ(junk.getPersistentState(), model::gameobjects::Persistable_PersistentState::DELETED);
}

TEST_F(EquipDeleteRunTest, AnUnbreakableItemStaysWithItsMessage) {
	Item& ancientKinah = stored(720013, LESSER_ANCIENT_KINAH, 5);
	ASSERT_FALSE(ancientKinah.getItemTemplate()->isBreakable()) << "mask 28684 has no BREAKABLE";

	destroy(720013);

	EXPECT_EQ(sent(), exactly({serializedFor(SM_SYSTEM_MESSAGE::STR_UNBREAKABLE_ITEM(ancientKinah.getL10n()))}));
	EXPECT_EQ(storage(StorageType::CUBE).getItemByObjId(720013).get(), &ancientKinah);
}

TEST_F(EquipDeleteRunTest, OnlyTheCubeIsSearched) {
	// CM_DELETE_ITEM.java:34-35: player.getInventory().getItemByObjId - an equipped item or one of the warehouse is not found, nothing happens
	equipped(720014, TRAINING_SWORD, MAIN_HAND);
	stored(720015, SPARKIE_CARAPACE_FRAGMENT, 1, StorageType::REGULAR_WAREHOUSE);

	destroy(720014);
	destroy(720015);
	destroy(720099);

	EXPECT_TRUE(sent().empty());
	EXPECT_TRUE(player().getEquipment().getEquippedItemByObjId(720014));
	EXPECT_TRUE(storage(StorageType::REGULAR_WAREHOUSE).getItemByObjId(720015));
}

// ------------------------------------------------------------------------------------------ the weapon swap of a dual-wielding player
//
// Deviation (report 3 of the 2026-09-28 play session; docs/deviations/P5-15.md, docs/design/m5d0-client-session.md R-3). The owner's client
// packet trace of 2026-09-29 (gameserver.network.trace.client_packets = CM_EQUIP_ITEM; character Rectangle, a one-handed weapon in each hand,
// object ids 104391 and 104781) shows each swap as four CM_EQUIP_ITEMs sent within a few milliseconds: two unequips (slot 0), then an equip into
// the main hand (slot mask 1) and one into the off hand (2). The trace holds 12 such sequences in two forms:
// - unequip X, unequip Y, equip Y into the main hand, equip X into the off hand (10, e.g. 22:26:43): as a swap, the main-hand weapon goes first.
//   Java's "retail like" rule sends both weapons to the cube with it (Equipment.java:239-247), so the second unequip names an item that is no
//   longer equipped, and Java answers that null with STR_UI_INVENTORY_FULL (CM_EQUIP_ITEM.java:50-53): the spurious message of the report.
//   The swap itself completes.
// - unequip X, unequip Y, equip X into the main hand, equip Y into the off hand (2: 22:27:50 and 22:28:02): the off-hand weapon goes first,
//   each unequip moves one weapon, and there never was a message.

class WeaponSwapTest : public EquipDeleteRunTest {
protected:
	/** the object ids of the trace's two weapons */
	static constexpr int32_t WEAPON_1 = 104391;
	static constexpr int32_t WEAPON_2 = 104781;

	static constexpr int64_t SUB_HAND = 2; // ItemSlot.SUB_HAND.getSlotIdMask()

	void SetUp() override {
		EquipDeleteRunTest::SetUp();
		// WeaponDualEffect.hasDualWieldEffect asks a spawned player's skill efficiency, which the dual-wield passive of a Scout sets; without it
		// Equipment.onLoadHandler puts the off-hand weapon back into the cube and equipItem turns slot 2 into the main hand (Equipment.java:67-68)
		player().getGameStats()->setSkillEfficiency(1.0f);
		mainHand = &equipped(WEAPON_1, TRAINING_SWORD, MAIN_HAND);
		offHand = &equipped(WEAPON_2, FABLED_TEST_SWORD, SUB_HAND);
		ASSERT_EQ(player().getEquipment().getMainHandWeapon().get(), mainHand);
		ASSERT_EQ(player().getEquipment().getOffHandWeapon().get(), offHand) << "the off-hand weapon was put back into the cube";
		clearSent();
	}

	/** What one CM_EQUIP_ITEM sent, and the appearance of the equipment it left (SM_UPDATE_PLAYER_APPEARANCE, CM_EQUIP_ITEM.java:60-62) */
	struct Step {
		std::vector<std::vector<uint8_t>> packets;
		std::vector<uint8_t> appearanceAfter;
	};

	Step step(int32_t action, int64_t slot, int32_t itemObjId) {
		clearSent();
		equip(action, slot, itemObjId);
		return {sent(), appearance()};
	}

	/** The packets of the steps, in order */
	static std::vector<std::vector<uint8_t>> packetsOfSteps(std::initializer_list<const Step*> steps) {
		std::vector<std::vector<uint8_t>> packets;
		for (const Step* s : steps)
			packets.insert(packets.end(), s->packets.begin(), s->packets.end());
		return packets;
	}

	/**
	 * SM_INVENTORY_UPDATE_ITEM(EQUIP_UNEQUIP) of `item` equipped in `slot`, or in the cube for 0: what Storage.put sends for an unequipped item
	 * (Storage.java:213) and ItemPacketService.updateItemAfterEquip for an equipped one (ItemPacketService.java:163-164); its blob is the
	 * EQUIPPED_SLOT entry alone, the slot or 0 (EquippedSlotBlobEntry.java:20). Serialized from a look-alike item: the real one has moved on
	 */
	std::vector<uint8_t> equipUpdate(Item& item, int64_t slot) {
		runtime::Ref<Item> lookAlike = loadedItem(item.getObjectId(), item.getItemId(), 1, StorageType::CUBE, slot, slot != 0);
		return serializedFor(
			serverpackets::SM_INVENTORY_UPDATE_ITEM(player(), *lookAlike, services::item::ItemPacketService_ItemUpdateType::EQUIP_UNEQUIP));
	}

	/** The swap's end: the weapons crossed, both equipped, neither in the cube */
	void expectSwapped() {
		model::gameobjects::player::Equipment& equipment = player().getEquipment();
		EXPECT_EQ(equipment.getMainHandWeapon().get(), offHand);
		EXPECT_EQ(equipment.getOffHandWeapon().get(), mainHand);
		EXPECT_TRUE(offHand->isEquipped());
		EXPECT_EQ(offHand->getEquipmentSlot(), MAIN_HAND);
		EXPECT_TRUE(mainHand->isEquipped());
		EXPECT_EQ(mainHand->getEquipmentSlot(), SUB_HAND);
		EXPECT_FALSE(storage(StorageType::CUBE).getItemByObjId(WEAPON_1));
		EXPECT_FALSE(storage(StorageType::CUBE).getItemByObjId(WEAPON_2));
	}

	Item* mainHand = nullptr; // the weapon of the main hand before the swap
	Item* offHand = nullptr;  // the weapon of the off hand before the swap
};

TEST_F(WeaponSwapTest, TheMainHandFirstSwapCompletesWithoutTheInventoryFullMessage) {
	// the first form, as at 22:26:43: unequip 104391 (the main hand), unequip 104781, equip 104781 into slot 1, equip 104391 into slot 2
	const Step first = step(1, 0, WEAPON_1);
	EXPECT_FALSE(offHand->isEquipped()) << "Equipment.java:239-247: the off-hand weapon went to the cube with the main-hand one";
	const Step second = step(1, 0, WEAPON_2);
	const Step third = step(0, MAIN_HAND, WEAPON_2);
	const Step fourth = step(0, SUB_HAND, WEAPON_1);

	expectSwapped();
	EXPECT_TRUE(second.packets.empty()) << "the stale unequip fails silently, where Java answers STR_UI_INVENTORY_FULL (CM_EQUIP_ITEM.java:52-53)";
	const std::vector<std::vector<uint8_t>> packets = packetsOfSteps({&first, &second, &third, &fourth});
	EXPECT_TRUE(packetsOf(packets, SM_SYSTEM_MESSAGE_OPCODE).empty());
	// the first unequip puts the off-hand weapon into the cube before the main-hand one (unEquip(SUB_HAND), then its own slot), then the equips
	EXPECT_EQ(packetsOf(packets, SM_INVENTORY_UPDATE_ITEM_OPCODE), exactly({equipUpdate(*offHand, 0), equipUpdate(*mainHand, 0),
																	   equipUpdate(*offHand, MAIN_HAND), equipUpdate(*mainHand, SUB_HAND)}));
	// every step that changed the equipment ends with its appearance: both hands empty, the new main hand, both hands
	EXPECT_EQ(packetsOf(packets, SM_UPDATE_PLAYER_APPEARANCE_OPCODE).size(), 3u);
	for (const Step* changed : {&first, &third, &fourth}) {
		ASSERT_FALSE(changed->packets.empty());
		EXPECT_EQ(changed->packets.back(), changed->appearanceAfter);
	}
}

TEST_F(WeaponSwapTest, TheOffHandFirstSwapCompletesWithoutAMessage) {
	// the second form, as at 22:27:50: unequip 104781 (the off hand), unequip 104391, equip 104781 into slot 1, equip 104391 into slot 2
	const Step first = step(1, 0, WEAPON_2);
	EXPECT_TRUE(mainHand->isEquipped()) << "an off-hand unequip moves the off-hand weapon alone";
	const Step second = step(1, 0, WEAPON_1);
	const Step third = step(0, MAIN_HAND, WEAPON_2);
	const Step fourth = step(0, SUB_HAND, WEAPON_1);

	expectSwapped();
	const std::vector<std::vector<uint8_t>> packets = packetsOfSteps({&first, &second, &third, &fourth});
	EXPECT_TRUE(packetsOf(packets, SM_SYSTEM_MESSAGE_OPCODE).empty());
	EXPECT_EQ(packetsOf(packets, SM_INVENTORY_UPDATE_ITEM_OPCODE), exactly({equipUpdate(*offHand, 0), equipUpdate(*mainHand, 0),
																	   equipUpdate(*offHand, MAIN_HAND), equipUpdate(*mainHand, SUB_HAND)}));
	EXPECT_EQ(packetsOf(packets, SM_UPDATE_PLAYER_APPEARANCE_OPCODE).size(), 4u);
	for (const Step* changed : {&first, &second, &third, &fourth}) {
		ASSERT_FALSE(changed->packets.empty());
		EXPECT_EQ(changed->packets.back(), changed->appearanceAfter);
	}
}

TEST_F(WeaponSwapTest, AMainHandUnequipWithOneFreeCubeSlotIsStillAnsweredWithInventoryFull) {
	// Equipment.java:240-245: unequipping the main-hand weapon with a weapon in the off hand moves both, so it needs two free cube slots. With
	// one, unEquipItem answers null while both are still equipped: the cube is the reason, and the message stays (CM_EQUIP_ITEM.java:52-53)
	model::items::storage::Storage& cube = storage(StorageType::CUBE);
	for (int32_t objId = 720100; cube.getFreeSlots() > 1 && objId < 721000; ++objId)
		stored(objId, SPARKIE_CARAPACE_FRAGMENT, 1);
	ASSERT_EQ(cube.getFreeSlots(), 1) << cube.size() << " items, limit " << cube.getLimit();
	ASSERT_FALSE(cube.isFull()) << "so the full-cube check (Equipment.java:231-232) lets the unequip through to the two-weapon rule";

	equip(1, 0, WEAPON_1);

	EXPECT_EQ(sent(), exactly({serializedFor(SM_SYSTEM_MESSAGE::STR_UI_INVENTORY_FULL())}));
	EXPECT_EQ(player().getEquipment().getMainHandWeapon().get(), mainHand);
	EXPECT_EQ(player().getEquipment().getOffHandWeapon().get(), offHand);
	EXPECT_FALSE(cube.getItemByObjId(WEAPON_1));
	EXPECT_FALSE(cube.getItemByObjId(WEAPON_2));
}

// ---------------------------------------------------------------------------------------------------------- the client packet trace
//
// C++ only (play-session fixes 2026-09-28, docs/deviations/P4-15.md, P4-01.md and P5-15.md): gameserver.network.trace.client_packets names the
// client packets AionClientPacket::run logs at INFO with the player's name before runImpl, so the next play session can capture the sequence of
// CM_EQUIP_ITEMs behind the "inventory full" report; CM_EQUIP_ITEM's C++-only toString shows the decoded fields.

const char* AION_CLIENT_PACKET_LOGGER = "com.aionemu.gameserver.network.aion.AionClientPacket";

using TraceKey = commons::configuration::ConfigValue<std::set<std::string, std::less<>>>;

/** Sets gameserver.network.trace.client_packets (or `key`, e.g. the server packet trace) for one case and puts the previous value back */
class TraceScope {
public:
	explicit TraceScope(std::set<std::string, std::less<>> names, TraceKey& keyValue = configs::network::NetworkConfig::TRACE_CLIENT_PACKETS)
		: key(keyValue), previous(*keyValue.get()) {
		key.set(std::move(names));
	}
	~TraceScope() { key.set(previous); }
	TraceScope(const TraceScope&) = delete;
	TraceScope& operator=(const TraceScope&) = delete;

private:
	TraceKey& key;
	const std::set<std::string, std::less<>> previous;
};

class ClientPacketTraceTest : public EquipDeleteRunTest {
protected:
	/**
	 * Creates the packet with its registered factory (so its dynamic type, which getPacketName reads, is the packet class itself, not a test
	 * Driver), reads the body and runs it through AionClientPacket::run, the entry the PacketProcessor calls (Driver::readAndRun calls runImpl)
	 */
	void dispatch(std::unique_ptr<AionClientPacket> packet, const std::vector<uint8_t>& body) {
		std::vector<uint8_t> copy = body;
		packet->setBuffer(commons::utils::ByteBuffer::wrap(copy));
		packet->setConnection(client->get());
		ASSERT_TRUE(packet->read());
		packet->run();
	}

	void equipPacket(int32_t action, int64_t slot, int32_t itemObjId) {
		dispatch(CM_EQUIP_ITEM_clientPacketFactory(CM_EQUIP_ITEM_OPCODE, StateSet{AionConnection_State::IN_GAME}), equipBody(action, slot, itemObjId));
	}

	void deletePacket(int32_t itemObjId) {
		dispatch(CM_DELETE_ITEM_clientPacketFactory(CM_DELETE_ITEM_OPCODE, StateSet{AionConnection_State::IN_GAME}), PacketWriter().D(itemObjId).data);
	}

	std::string traceLine(std::string_view packet) {
		return "info|" + std::string(AION_CLIENT_PACKET_LOGGER) + "|Client packet trace: " + player().getName() + " sent " + std::string(packet);
	}
};

TEST_F(ClientPacketTraceTest, ANamedPacketIsLoggedWithThePlayerAndItsFieldsAndStillRuns) {
	TraceScope trace({"CM_EQUIP_ITEM"});
	stored(720101, TRAINING_SWORD, 1);
	stored(720102, SPARKIE_CARAPACE_FRAGMENT, 1);
	LogCapture log({AION_CLIENT_PACKET_LOGGER});

	equipPacket(0, MAIN_HAND, 720101);
	deletePacket(720102);

	EXPECT_EQ(log.count("Client packet trace: "), 1) << "CM_DELETE_ITEM is not named: " << log.dump();
	EXPECT_TRUE(log.contains(traceLine("CM_EQUIP_ITEM [action=0, slot=1, itemObjId=720101]"))) << log.dump();
	EXPECT_TRUE(player().getEquipment().getEquippedItemByObjId(720101)) << "the traced packet ran";
	EXPECT_FALSE(storage(StorageType::CUBE).getItemByObjId(720102)) << "the packet that is not traced ran too";
}

TEST_F(ClientPacketTraceTest, EveryNamedPacketIsLoggedInTheOrderItRan) {
	TraceScope trace({"CM_DELETE_ITEM", "CM_EQUIP_ITEM", "CM_TUNE"});
	equipped(720103, TRAINING_SWORD, MAIN_HAND);
	LogCapture log({AION_CLIENT_PACKET_LOGGER});

	equipPacket(1, MAIN_HAND, 720103);
	deletePacket(720099);
	equipPacket(1, MAIN_HAND, 720103);

	// the second unequip is the report's case: the sword is in the cube already; Java answers STR_UI_INVENTORY_FULL, the C++ nothing (P5-15.md)
	EXPECT_EQ(log.dump(), traceLine("CM_EQUIP_ITEM [action=1, slot=1, itemObjId=720103]") + "\n" + traceLine("[116] CM_DELETE_ITEM") + "\n" +
							  traceLine("CM_EQUIP_ITEM [action=1, slot=1, itemObjId=720103]") + "\n")
		<< "a packet without a toString of its own prints Java's [opcode] name";
}

TEST_F(ClientPacketTraceTest, AnEmptyListLogsNothing) {
	TraceScope trace({});
	stored(720104, TRAINING_SWORD, 1);
	LogCapture log({AION_CLIENT_PACKET_LOGGER});

	equipPacket(0, MAIN_HAND, 720104);
	deletePacket(720099);

	EXPECT_EQ(log.dump(), "") << "the default: no trace";
	EXPECT_TRUE(player().getEquipment().getEquippedItemByObjId(720104));
}

// The line is written before runImpl, so a traced packet whose runImpl throws is traced before run() logs the failure (the line one needs next to
// "Error handling client packet"). A CM_EQUIP_ITEM on a connection without an active player throws Java's NullPointerException at its first
// statement (CM_EQUIP_ITEM.java:37-39); before a player entered the world the connection stands in for the player's name.
TEST_F(ClientPacketTraceTest, APacketIsTracedBeforeItRunsSoAFailingOneIsTracedBeforeItsError) {
	TraceScope trace({"CM_EQUIP_ITEM"});
	TestClient connected; // no player: the state a fresh connection has
	std::unique_ptr<AionClientPacket> packet =
		CM_EQUIP_ITEM_clientPacketFactory(CM_EQUIP_ITEM_OPCODE, StateSet{connected->getState()}); // valid in that state
	std::vector<uint8_t> body = equipBody(1, MAIN_HAND, 720105);
	packet->setBuffer(commons::utils::ByteBuffer::wrap(body));
	packet->setConnection(connected.get());
	ASSERT_TRUE(packet->read());
	LogCapture log({AION_CLIENT_PACKET_LOGGER});

	packet->run();

	const std::string lines = log.dump();
	const size_t traced = lines.find("info|" + std::string(AION_CLIENT_PACKET_LOGGER) + "|Client packet trace: " + connected->toString() +
									 " sent CM_EQUIP_ITEM [action=1, slot=1, itemObjId=720105]\n");
	const size_t failed = lines.find("|Error handling client packet from ");
	ASSERT_NE(traced, std::string::npos) << lines;
	ASSERT_NE(failed, std::string::npos) << "runImpl threw: " << lines;
	EXPECT_LT(traced, failed) << lines;
}

// Only a packet run() lets through is traced: one whose connection state is not among its valid states (AionClientPacket.java:36-43, here an
// AUTHED-only packet on the IN_GAME connection) is neither run nor traced
TEST_F(ClientPacketTraceTest, APacketRefusedByTheConnectionStateIsNotTraced) {
	TraceScope trace({"CM_EQUIP_ITEM"});
	stored(720106, TRAINING_SWORD, 1);
	LogCapture log({AION_CLIENT_PACKET_LOGGER});

	dispatch(CM_EQUIP_ITEM_clientPacketFactory(CM_EQUIP_ITEM_OPCODE, StateSet{AionConnection_State::AUTHED}), equipBody(0, MAIN_HAND, 720106));

	EXPECT_EQ(log.dump(), "") << "not traced";
	EXPECT_FALSE(player().getEquipment().getEquippedItemByObjId(720106)) << "not run";
	EXPECT_TRUE(storage(StorageType::CUBE).getItemByObjId(720106));
}

// ---------------------------------------------------------------------------------------------------------- the server packet trace
//
// C++ only (play-session diagnostics 2026-09-29, docs/deviations/P4-15.md and P4-01.md): gameserver.network.trace.server_packets names the server
// packets AionConnection::sendPacket logs at INFO, after queuing them, with the receiving player's name - so the next play session can time the
// SM_GATHERABLE_INFO of the Sanctum crafting benches against the player's arrival (the CM_MOVEs of the client packet trace).

const char* AION_CONNECTION_LOGGER = "com.aionemu.gameserver.network.aion.AionConnection";

class ServerPacketTraceTest : public EquipDeleteRunTest {
protected:
	static std::string traceLine(std::string_view packet, std::string_view receiver) {
		return "info|" + std::string(AION_CONNECTION_LOGGER) + "|Server packet trace: sent " + std::string(packet) + " to " + std::string(receiver) +
			"\n";
	}
};

TEST_F(ServerPacketTraceTest, ANamedPacketIsLoggedWithTheReceivingPlayerAndStillSent) {
	TraceScope trace({"SM_UPDATE_PLAYER_APPEARANCE"}, configs::network::NetworkConfig::TRACE_SERVER_PACKETS);
	equipped(720201, TRAINING_SWORD, MAIN_HAND);
	LogCapture log({AION_CONNECTION_LOGGER});

	equip(1, 0, 720201); // the unequip sends the sword's SM_INVENTORY_UPDATE_ITEM, the stats and the appearance (broadcast to the player too)

	EXPECT_EQ(log.dump(), traceLine("[036] SM_UPDATE_PLAYER_APPEARANCE", "Holder")) << "the named packet alone";
	const std::vector<std::vector<uint8_t>> packets = sent();
	EXPECT_EQ(packetsOf(packets, SM_UPDATE_PLAYER_APPEARANCE_OPCODE).size(), 1u) << "the traced packet is still sent";
	EXPECT_EQ(packetsOf(packets, SM_INVENTORY_UPDATE_ITEM_OPCODE).size(), 1u) << "the packets not traced are sent too";
}

TEST_F(ServerPacketTraceTest, EveryNamedPacketIsLoggedInTheOrderItWasSent) {
	TraceScope trace({"SM_GATHERABLE_INFO", "SM_INVENTORY_UPDATE_ITEM", "SM_UPDATE_PLAYER_APPEARANCE"}, configs::network::NetworkConfig::TRACE_SERVER_PACKETS);
	equipped(720202, TRAINING_SWORD, MAIN_HAND);
	LogCapture log({AION_CONNECTION_LOGGER});

	equip(1, 0, 720202);
	equip(1, 0, 720202); // the stale unequip: nothing sent, nothing traced

	EXPECT_EQ(log.dump(), traceLine("[029] SM_INVENTORY_UPDATE_ITEM", "Holder") + traceLine("[036] SM_UPDATE_PLAYER_APPEARANCE", "Holder"));
}

TEST_F(ServerPacketTraceTest, AnEmptyListLogsNothing) {
	TraceScope trace({}, configs::network::NetworkConfig::TRACE_SERVER_PACKETS);
	equipped(720203, TRAINING_SWORD, MAIN_HAND);
	LogCapture log({AION_CONNECTION_LOGGER});

	equip(1, 0, 720203);

	EXPECT_EQ(log.dump(), "") << "the default: no trace";
	EXPECT_FALSE(sent().empty());
}

// Before a player entered the world (the login screens send server packets too) the connection stands in for the player's name, as in the client
// packet trace
TEST_F(ServerPacketTraceTest, BeforeTheWorldTheConnectionStandsInForThePlayer) {
	TraceScope trace({"SM_SYSTEM_MESSAGE"}, configs::network::NetworkConfig::TRACE_SERVER_PACKETS);
	TestClient connected; // no player: the state a fresh connection has
	LogCapture log({AION_CONNECTION_LOGGER});

	connected->sendPacket(SM_SYSTEM_MESSAGE::STR_UI_INVENTORY_FULL());

	EXPECT_EQ(log.dump(), traceLine("[025] SM_SYSTEM_MESSAGE", connected->toString()));
	EXPECT_EQ(connected->sentBytes().size(), 1u);
}

} // namespace
} // namespace testing::items
} // namespace aion::gameserver::network::aion::clientpackets
