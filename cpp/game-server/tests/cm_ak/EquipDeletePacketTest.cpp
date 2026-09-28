// CM_EQUIP_ITEM and CM_DELETE_ITEM (P5-15, m5b3-plan.md P-05/P-06): C_USE_EQUIPMENT_ITEM, what the client sends to equip, unequip or switch the
// weapon sets, and C_DESTROY_ITEM, what it sends when the player destroys an item of the cube.
//
// Java: game-server/src/com/aionemu/gameserver/network/aion/clientpackets/CM_EQUIP_ITEM.java:28-63 and CM_DELETE_ITEM.java:25-44.
//
// The byte vectors are laid out field by field from the Java readImpl; the run tests drive runImpl against the fixture of ItemPacketTestSupport.h
// (a spawned warrior in Poeta whose packets a real AionConnection keeps). What the services behind the packets do on their own is their chunks'
// tests' business (Equipment: tests/player and tests/stats; Storage.delete and ItemPacketService: tests/itemsvc); these cases pin what the packet
// adds: its reads, its order of calls (cancelUseItem before canChangeEquip, the restriction before any equipment change), its own answers
// (STR_UI_INVENTORY_FULL on a failed unequip, STR_UNBREAKABLE_ITEM) and the appearance broadcast.

#include "ItemPacketTestSupport.h"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "aion/commons/utils/ByteBuffer.h"
#include "aion/gameserver/controllers/ObserveController.h"
#include "aion/gameserver/controllers/PlayerController.h"
#include "aion/gameserver/controllers/observer/ItemUseObserver.h"
#include "aion/gameserver/model/TaskId.h"
#include "aion/gameserver/model/templates/item/ItemTemplate.h"
#include "aion/gameserver/network/aion/clientpackets/CM_DELETE_ITEM.h"
#include "aion/gameserver/network/aion/clientpackets/CM_EQUIP_ITEM.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/network/aion/serverpackets/SM_UPDATE_PLAYER_APPEARANCE.h"
#include "aion/gameserver/runtime/base/Unported.h"

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

TEST_F(EquipDeleteRunTest, AnUnequipThatFailsIsAnsweredWithInventoryFull) {
	// CM_EQUIP_ITEM.java:51-53: Java answers every null result of unEquipItem with the full inventory message, whatever the cause
	equip(1, 0, 720099);

	EXPECT_EQ(sent(), exactly({serializedFor(SM_SYSTEM_MESSAGE::STR_UI_INVENTORY_FULL())}));
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

} // namespace
} // namespace testing::items
} // namespace aion::gameserver::network::aion::clientpackets
