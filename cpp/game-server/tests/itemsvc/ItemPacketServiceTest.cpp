// P5-07 ItemPacketService (m5b3-plan.md T-02, which closes E-13 and E-14): the nine bodies of ItemPacketService.java:163-234, driven against
// a real Player whose packets a real AionConnection captures (tests/cm_ak/InWorldPacketRunSupport.h, included by relative path the way
// tests/cm_lz and tests/skills include it: the chunks share no test support directory).
//
// Every expected packet is spelled out as the bytes Java writes: the header of AionServerPacket.writeOP with the opcode of
// ServerPacketsOpcodes.java (SM_INVENTORY_ADD_ITEM 27, SM_DELETE_ITEM 28, SM_INVENTORY_UPDATE_ITEM 29, SM_CUBE_UPDATE 130,
// SM_WAREHOUSE_ADD_ITEM 169, SM_DELETE_WAREHOUSE_ITEM 170, SM_WAREHOUSE_UPDATE_ITEM 171) and the writeImpl fields the service chooses. The
// packets whose bodies carry an ItemInfoBlob are compared whole against their own serialization (their bytes are pinned by tests/sm_ak) after
// the fields the service chooses were read from the bytes by hand.
//
// The last case is E-13 end to end: an item whose expire time has passed, ExpireTimerTask firing on the DeterministicExecutor's ManualClock,
// the item gone from the storage and Java's SM_DELETE_ITEM, SM_CUBE_UPDATE and STR_MSG_DELETE_CASH_ITEM_BY_TIMEOUT (Item.java:onExpire,
// Storage.java:235-249).

#include "../cm_ak/InWorldPacketRunSupport.h"

#include <chrono>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "aion/commons/utils/TimeUtils.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/ItemData.bind.h"
#include "aion/gameserver/dataholders/ItemData.h"
#include "aion/gameserver/dataholders/ItemRestrictionCleanupData.h"
#include "aion/gameserver/model/Expirable.h"
#include "aion/gameserver/model/gameobjects/Item.h"
#include "aion/gameserver/model/gameobjects/Persistable.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/items/storage/Storage.h"
#include "aion/gameserver/model/items/storage/StorageType.h"
#include "aion/gameserver/model/items/storage/StorageTypeInfo.h"
#include "aion/gameserver/model/templates/item/ItemTemplate.h"
#include "aion/gameserver/network/Crypt.h"
#include "aion/gameserver/network/aion/serverpackets/SM_INVENTORY_ADD_ITEM.h"
#include "aion/gameserver/network/aion/serverpackets/SM_INVENTORY_UPDATE_ITEM.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/network/aion/serverpackets/SM_WAREHOUSE_ADD_ITEM.h"
#include "aion/gameserver/network/aion/serverpackets/SM_WAREHOUSE_UPDATE_ITEM.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/services/item/ItemPacketService.h"
#include "aion/gameserver/taskmanager/tasks/ExpireTimerTask.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::services::item::test {
namespace {

namespace cp = network::aion::clientpackets::testing;
using namespace std::chrono_literals;
using model::gameobjects::Item;
using model::items::storage::StorageType;
using network::aion::serverpackets::SM_INVENTORY_ADD_ITEM;
using network::aion::serverpackets::SM_INVENTORY_UPDATE_ITEM;
using network::aion::serverpackets::SM_SYSTEM_MESSAGE;
using network::aion::serverpackets::SM_WAREHOUSE_ADD_ITEM;
using network::aion::serverpackets::SM_WAREHOUSE_UPDATE_ITEM;
using network::test::PacketReader;
using network::test::PacketWriter;
using runtime::Ptr;
using runtime::Ref;
using ItemAddType = ItemPacketService::ItemAddType;
using ItemDeleteType = ItemPacketService::ItemDeleteType;
using ItemUpdateType = ItemPacketService::ItemUpdateType;
using PersistentState = model::gameobjects::Persistable::PersistentState;

// ServerPacketsOpcodes.java
constexpr int32_t SM_INVENTORY_ADD_ITEM_OPCODE = 27;
constexpr int32_t SM_DELETE_ITEM_OPCODE = 28;
constexpr int32_t SM_INVENTORY_UPDATE_ITEM_OPCODE = 29;
constexpr int32_t SM_CUBE_UPDATE_OPCODE = 130;
constexpr int32_t SM_WAREHOUSE_ADD_ITEM_OPCODE = 169;
constexpr int32_t SM_DELETE_WAREHOUSE_ITEM_OPCODE = 170;
constexpr int32_t SM_WAREHOUSE_UPDATE_ITEM_OPCODE = 171;

/** item_templates.xml:850013 */
constexpr int32_t BANDAGE = 169300002;
/** item_templates.xml:834664, "Administrator's Boon - 3-Day Pass" of every new character (expire_time 4321 minutes) */
constexpr int32_t BOON_3_DAY = 164002039;
/** item_templates.xml:834670, the 7-day pass (expire_time 10081) */
constexpr int32_t BOON_7_DAY = 164002040;
constexpr int32_t KINAH = 182400001;

/**
 * The four templates as item_templates.xml has them, without the Boons' <actions> and <uselimits> (the item action hooks belong to later
 * P5-07 work) and with an ASCII hyphen in their names (a name is never sent: the packets carry the L10n id of `desc`).
 */
constexpr const char* ITEM_TEMPLATES_XML =
	R"(<item_templates>)"
	R"(<item_template id="169300002" name="Bandage" level="1" cName="bandage_01" mask="12414" max_stack_count="10000" quality="COMMON" price="5")"
	R"( desc="701824"/>)"
	R"(<item_template id="164002039" name="Administrator's Boon - 3-Day Pass" level="1" cName="cash_scroll_start_kit_01_3day" mask="4168")"
	R"( quality="LEGEND" price="5" desc="778666" activate_target="STANDALONE" activate_count="1000" expire_time="4321"/>)"
	R"(<item_template id="164002040" name="Administrator's Boon - 7-Day Pass" level="1" cName="cash_scroll_start_kit_01_7day" mask="4168")"
	R"( quality="LEGEND" price="5" desc="778667" activate_target="STANDALONE" activate_count="1000" expire_time="10081"/>)"
	R"(<item_template id="182400001" name="Kinah" level="1" cName="gold" mask="12350" quality="COMMON" price="0" desc="701677"/>)"
	R"(</item_templates>)";

/** One server packet as Java writes it: AionServerPacket.writeOP ([H op][C 0x44][H ~op], op = Crypt.encodeServerPacketOpcode) and the body */
std::vector<uint8_t> javaPacket(int32_t opcode, const PacketWriter& body) {
	int32_t op = (opcode + 207) ^ 0xDF; // Crypt.encodeServerPacketOpcode: (opcode + SM_VERSION_CHECK.INTERNAL_VERSION) ^ 0xDF
	PacketWriter packet;
	packet.H(op).C(0x44).H(~op);
	packet.B(body.data);
	return packet.data;
}

/** The opcode Java wrote into the header of a captured packet */
int32_t javaOpcodeOf(const std::vector<uint8_t>& bytes) {
	PacketReader reader(bytes);
	return ((static_cast<uint16_t>(reader.H()) ^ 0xDF) - 207) & 0xFFFF;
}

/** The last two body bytes of a captured packet as Java's final writeH (the update type mask of a sendable type), -1 for a shorter body */
int32_t trailingMask(const std::vector<uint8_t>& packet) {
	std::vector<uint8_t> body = cp::bodyOf(packet);
	return body.size() < 2 ? -1 : body[body.size() - 2] | body[body.size() - 1] << 8;
}

/** SM_CUBE_UPDATE.cubeSize(type, player) of a character without expansions: action 0, type.ordinal(), the item count and three zero bytes */
std::vector<uint8_t> cubeSize(StorageType type, int32_t itemsCount) {
	return javaPacket(SM_CUBE_UPDATE_OPCODE, PacketWriter().C(0).C(static_cast<int32_t>(type)).D(itemsCount).C(0).C(0).C(0));
}

/** An item row as the inventory DAO loads it: count, expire time and location, everything else 0 */
Ref<Item> loadedItem(int32_t objId, int32_t itemId, int64_t count, int32_t expireTime, StorageType location) {
	return Item::create(objId, itemId, count, std::nullopt, 0, "", expireTime, 0, false, false, 0, model::items::storage::getId(location), 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, false, 0, 0);
}

/** A fresh ExpireTimerTask: its constructor schedules run() on the installed DeterministicExecutor (500-550 ms, then every 1,000 ms) */
class TestExpireTimerTask final : public taskmanager::tasks::ExpireTimerTask {
	AION_MAKE_REF_FRIEND
public:
	TestExpireTimerTask() = default;

protected:
	~TestExpireTimerTask() override = default;
};

class ItemPacketServiceTest : public cp::InWorldPacketTest {
protected:
	void SetUp() override {
		InWorldPacketTest::SetUp();
		// the base fixture's DeterministicExecutor again, with a handle the expiry case advances (nothing is scheduled yet)
		utils::ThreadPoolManager::installBackend(nullptr);
		auto backend = std::make_unique<runtime::DeterministicExecutor>(clock, 17);
		executor = backend.get(); // owned by ThreadPoolManager until the base TearDown installs no backend
		utils::ThreadPoolManager::installBackend(std::move(backend));
		xml::LoadContext context;
		dataholders::DataManager::ITEM_DATA.publish(xml::bindString<dataholders::ItemData>(context, ITEM_TEMPLATES_XML));
		// the item info blob's GENERAL_INFO entry asks it (GeneralInfoBlobEntry: hasAccountOrLegionWhStorabilityDisabled); no cleanup entries
		dataholders::DataManager::ITEM_CLEAN_UP.publish(std::make_unique<dataholders::ItemRestrictionCleanupData>());
		f = cp::makePlayer(700001, 9701, "Carrier");
		client = std::make_unique<cp::TestClient>();
		client->enterWorld(f);
		(*client)->clearSent();
	}

	void TearDown() override {
		if (f.player)
			f.player->setClientConnection(nullptr);
		client.reset();
		items.clear();
		f = {};
		InWorldPacketTest::TearDown();
		dataholders::DataManager::ITEM_CLEAN_UP.resetForTests();
		dataholders::DataManager::ITEM_DATA.resetForTests();
	}

	model::gameobjects::player::Player& player() { return *f.player; }

	/** An item of the template, loaded into the storage of `location` the way the DAO does (onLoadHandler: no packet) */
	Item& stored(int32_t objId, int32_t itemId, int64_t count, StorageType location = StorageType::CUBE, int32_t expireTime = 0) {
		Ref<Item> item = loadedItem(objId, itemId, count, expireTime, location);
		player().getStorage(model::items::storage::getId(location))->onLoadHandler(*item);
		items.push_back(item);
		return *item;
	}

	/** An item that is in no storage (the service does not look it up) */
	Item& loose(int32_t objId, int32_t itemId, int64_t count, StorageType location = StorageType::CUBE) {
		Ref<Item> item = loadedItem(objId, itemId, count, 0, location);
		items.push_back(item);
		return *item;
	}

	std::vector<std::vector<uint8_t>> sent() { return (*client)->sentBytes(); }

	std::vector<uint8_t> serialized(network::aion::AionServerPacket&& packet) { return cp::serialized(std::move(packet), client->con()); }

	runtime::DeterministicExecutor* executor = nullptr;
	cp::PlayerFixture f;
	std::unique_ptr<cp::TestClient> client;
	std::vector<Ref<Item>> items;
};

// ------------------------------------------------------------------------------------------------------------------------- sendItemDeletePacket

TEST_F(ItemPacketServiceTest, ACubeItemIsDeletedWithSmDeleteItemThenTheCubeSize) {
	// ItemPacketService.java:177-185: CUBE -> SM_DELETE_ITEM(objectId, deleteType), then SM_CUBE_UPDATE.cubeSize(storageType, player)
	stored(810001, BANDAGE, 3);
	Item& bandage = stored(810002, BANDAGE, 5);
	ItemPacketService::sendItemDeletePacket(player(), StorageType::CUBE, bandage, ItemDeleteType::DISCARD);
	EXPECT_EQ(sent(), cp::exactly({javaPacket(SM_DELETE_ITEM_OPCODE, PacketWriter().D(810002).C(0x15)), cubeSize(StorageType::CUBE, 2)}))
		<< "SM_DELETE_ITEM: writeD(itemObjectId), writeC(DISCARD 0x15); the cube still holds both items (the service sends, Storage deletes)";
}

TEST_F(ItemPacketServiceTest, AnyOtherStorageDeletesWithSmDeleteWarehouseItemAndItsStorageId) {
	// ItemPacketService.java:180-184: SM_DELETE_WAREHOUSE_ITEM(storageType.getId(), objectId, deleteType) - the id, not the ordinal - then the
	// cube size of that storage type (SM_CUBE_UPDATE writes the ordinal; a pet bag has no size arm, so it writes zeros)
	Item& inWarehouse = stored(810003, BANDAGE, 2, StorageType::REGULAR_WAREHOUSE);
	ItemPacketService::sendItemDeletePacket(player(), StorageType::REGULAR_WAREHOUSE, inWarehouse, ItemDeleteType::SPLIT);
	EXPECT_EQ(sent(),
		cp::exactly({javaPacket(SM_DELETE_WAREHOUSE_ITEM_OPCODE, PacketWriter().C(1).D(810003).C(0x04)), cubeSize(StorageType::REGULAR_WAREHOUSE, 1)}));

	(*client)->clearSent();
	Item& inPetBag = loose(810004, BANDAGE, 1, StorageType::PET_BAG_6);
	ItemPacketService::sendItemDeletePacket(player(), StorageType::PET_BAG_6, inPetBag, ItemDeleteType::MOVE);
	EXPECT_EQ(sent(), cp::exactly({javaPacket(SM_DELETE_WAREHOUSE_ITEM_OPCODE, PacketWriter().C(32).D(810004).C(0x14)),
						  javaPacket(SM_CUBE_UPDATE_OPCODE, PacketWriter().C(0).C(4).D(0).C(0).C(0).C(0))}))
		<< "PET_BAG_6: id 32 in the delete, ordinal 4 in the cube update";
}

// ------------------------------------------------------------------------------------------------------------------------- sendItemPacket

TEST_F(ItemPacketServiceTest, AnEmptiedItemIsDeletedWithTheDeleteTypeOfItsUpdateType) {
	// ItemPacketService.java:167-173 and ItemDeleteType.fromUpdateType (:143-150): DEC_ITEM_SPLIT -> SPLIT, DEC_ITEM_USE -> USE,
	// DEC_ITEM_SPLIT_MOVE -> MOVE, anything else -> DEFAULT
	const std::vector<std::pair<ItemUpdateType, int32_t>> cases{{ItemUpdateType::DEC_ITEM_SPLIT, 0x04}, {ItemUpdateType::DEC_ITEM_USE, 0x17},
		{ItemUpdateType::DEC_ITEM_SPLIT_MOVE, 0x14}, {ItemUpdateType::INC_ITEM_MERGE, 0x00}, {ItemUpdateType::STATS_CHANGE, 0x00}};
	Item& empty = loose(810005, BANDAGE, 0);
	for (const auto& [updateType, deleteMask] : cases) {
		SCOPED_TRACE(std::string(xml::enumName(updateType)));
		(*client)->clearSent();
		ItemPacketService::sendItemPacket(player(), StorageType::CUBE, empty, updateType);
		EXPECT_EQ(sent(), cp::exactly({javaPacket(SM_DELETE_ITEM_OPCODE, PacketWriter().D(810005).C(deleteMask)), cubeSize(StorageType::CUBE, 0)}));
	}
}

TEST_F(ItemPacketServiceTest, AStackThatIsLeftAndKinahAtZeroAreUpdatedInstead) {
	// ItemPacketService.java:168: `item.getItemCount() <= 0 && !isKinah()` deletes; a stack with items left, and kinah even at 0, is updated
	// (sendItemUpdatePacket: CUBE -> SM_INVENTORY_UPDATE_ITEM(player, item, updateType) alone, no cube size)
	Item& bandage = stored(810006, BANDAGE, 4);
	ItemPacketService::sendItemPacket(player(), StorageType::CUBE, bandage, ItemUpdateType::DEC_ITEM_USE);
	std::vector<std::vector<uint8_t>> packets = sent();
	ASSERT_EQ(packets.size(), 1u);
	EXPECT_EQ(javaOpcodeOf(packets[0]), SM_INVENTORY_UPDATE_ITEM_OPCODE);
	EXPECT_EQ(PacketReader(cp::bodyOf(packets[0])).D(), 810006) << "writeD(item.getObjectId())";
	EXPECT_EQ(trailingMask(packets[0]), 0x16) << "sendable: writeH(DEC_ITEM_USE 0x16) last";
	EXPECT_EQ(packets[0], serialized(SM_INVENTORY_UPDATE_ITEM(player(), bandage, ItemUpdateType::DEC_ITEM_USE)));

	(*client)->clearSent();
	Item& kinah = loose(810007, KINAH, 0);
	ItemPacketService::sendItemPacket(player(), StorageType::CUBE, kinah, ItemUpdateType::DEC_KINAH_BUY);
	packets = sent();
	ASSERT_EQ(packets.size(), 1u) << "kinah at 0 is not deleted";
	EXPECT_EQ(javaOpcodeOf(packets[0]), SM_INVENTORY_UPDATE_ITEM_OPCODE);
	EXPECT_EQ(packets[0], serialized(SM_INVENTORY_UPDATE_ITEM(player(), kinah, ItemUpdateType::DEC_KINAH_BUY)));
}

TEST_F(ItemPacketServiceTest, AMergeIntoAStackOfTheInventorySendsTheUpdateAlone) {
	// Storage.increaseItemCount -> ItemPacketService.sendItemPacket (Storage.java; m5b3-plan.md Y3: a merge sends SM_INVENTORY_UPDATE_ITEM alone)
	Item& bandage = stored(810008, BANDAGE, 4);
	player().getInventory().increaseItemCount(bandage, 3, ItemUpdateType::INC_ITEM_MERGE);
	EXPECT_EQ(bandage.getItemCount(), 7);
	std::vector<std::vector<uint8_t>> packets = sent();
	ASSERT_EQ(packets.size(), 1u);
	EXPECT_EQ(trailingMask(packets[0]), 0x01) << "INC_ITEM_MERGE";
	EXPECT_EQ(packets[0], serialized(SM_INVENTORY_UPDATE_ITEM(player(), bandage, ItemUpdateType::INC_ITEM_MERGE)));
}

TEST_F(ItemPacketServiceTest, AnItemPacketOutsideTheCubeIsSentWithThePacketsOfItsStorage) {
	// ItemPacketService.java:167-173 passes its storageType on to both arms: the counts of a warehouse (Storage.increaseItemCount /
	// decreaseItemCount of a warehouse) go out as warehouse packets
	Item& emptied = loose(810027, BANDAGE, 0, StorageType::REGULAR_WAREHOUSE);
	Item& left = stored(810028, BANDAGE, 3, StorageType::REGULAR_WAREHOUSE);
	ItemPacketService::sendItemPacket(player(), StorageType::REGULAR_WAREHOUSE, emptied, ItemUpdateType::DEC_ITEM_SPLIT);
	EXPECT_EQ(sent(), cp::exactly({javaPacket(SM_DELETE_WAREHOUSE_ITEM_OPCODE, PacketWriter().C(1).D(810027).C(0x04)),
						  cubeSize(StorageType::REGULAR_WAREHOUSE, 1)}))
		<< "an emptied warehouse item: SM_DELETE_WAREHOUSE_ITEM(1, objectId, SPLIT) and the warehouse's size";

	(*client)->clearSent();
	ItemPacketService::sendItemPacket(player(), StorageType::REGULAR_WAREHOUSE, left, ItemUpdateType::DEC_ITEM_SPLIT);
	EXPECT_EQ(sent(), cp::exactly({serialized(SM_WAREHOUSE_UPDATE_ITEM(player(), left, 1, ItemUpdateType::DEC_ITEM_SPLIT))}))
		<< "a warehouse stack with items left: SM_WAREHOUSE_UPDATE_ITEM alone";
}

// ------------------------------------------------------------------------------------------------------------------------- sendItemUpdatePacket

TEST_F(ItemPacketServiceTest, AnUpdateOutsideTheCubeIsSmWarehouseUpdateItemWithTheStorageId) {
	// ItemPacketService.java:190-204: default -> SM_WAREHOUSE_UPDATE_ITEM(player, item, storageType.getId(), updateType); LEGION_WAREHOUSE falls
	// through to it for anything but kinah
	Item& inWarehouse = stored(810009, BANDAGE, 2, StorageType::ACCOUNT_WAREHOUSE);
	ItemPacketService::sendItemUpdatePacket(player(), StorageType::ACCOUNT_WAREHOUSE, inWarehouse, ItemUpdateType::DEC_ITEM_SPLIT);
	std::vector<std::vector<uint8_t>> packets = sent();
	ASSERT_EQ(packets.size(), 1u) << "no cube size after an update";
	EXPECT_EQ(javaOpcodeOf(packets[0]), SM_WAREHOUSE_UPDATE_ITEM_OPCODE);
	PacketReader reader(cp::bodyOf(packets[0]));
	EXPECT_EQ(reader.D(), 810009);
	EXPECT_EQ(reader.C(), 2) << "writeC(warehouseType): ACCOUNT_WAREHOUSE's id";
	EXPECT_EQ(packets[0], serialized(SM_WAREHOUSE_UPDATE_ITEM(player(), inWarehouse, 2, ItemUpdateType::DEC_ITEM_SPLIT)));

	(*client)->clearSent();
	Item& inLegionWarehouse = loose(810010, BANDAGE, 2, StorageType::LEGION_WAREHOUSE);
	ItemPacketService::sendItemUpdatePacket(player(), StorageType::LEGION_WAREHOUSE, inLegionWarehouse, ItemUpdateType::PUT);
	packets = sent();
	ASSERT_EQ(packets.size(), 1u);
	EXPECT_EQ(javaOpcodeOf(packets[0]), SM_WAREHOUSE_UPDATE_ITEM_OPCODE) << "the legion warehouse falls through for an item";
	EXPECT_EQ(PacketReader(cp::bodyOf(packets[0])).B(5)[4], 3) << "LEGION_WAREHOUSE's id";
	EXPECT_EQ(packets[0], serialized(SM_WAREHOUSE_UPDATE_ITEM(player(), inLegionWarehouse, 3, ItemUpdateType::PUT)));

	// the storages above have id == ordinal; a pet bag tells them apart (id 32, ordinal 4)
	(*client)->clearSent();
	Item& inPetBag = loose(810024, BANDAGE, 2, StorageType::PET_BAG_6);
	ItemPacketService::sendItemUpdatePacket(player(), StorageType::PET_BAG_6, inPetBag, ItemUpdateType::DEC_ITEM_SPLIT);
	packets = sent();
	ASSERT_EQ(packets.size(), 1u) << "no cube size after an update";
	EXPECT_EQ(PacketReader(cp::bodyOf(packets[0])).B(5)[4], 32) << "writeC(warehouseType): PET_BAG_6's id, not its ordinal 4";
	EXPECT_EQ(packets[0], serialized(SM_WAREHOUSE_UPDATE_ITEM(player(), inPetBag, 32, ItemUpdateType::DEC_ITEM_SPLIT)));
}

TEST_F(ItemPacketServiceTest, LegionWarehouseKinahOfAPlayerWithoutALegionReachesNoClient) {
	// ItemPacketService.java:196-199: SM_LEGION_EDIT(0x04, player.getLegion()); with a null legion Java's caller goes on and the packet's
	// writeImpl throws on the write thread after its header went into the write buffer, which corrupts the client's stream
	// (docs/deviations/P5-07.md: C++ does not build the packet, so the client gets nothing). The fixture's player has no legion.
	Item& kinah = loose(810011, KINAH, 500, StorageType::LEGION_WAREHOUSE);
	EXPECT_NO_THROW(ItemPacketService::sendItemUpdatePacket(player(), StorageType::LEGION_WAREHOUSE, kinah, ItemUpdateType::INC_KINAH_COLLECT));
	EXPECT_TRUE(sent().empty());
}

// ------------------------------------------------------------------------------------------------------------------------- sendStorageUpdatePacket

TEST_F(ItemPacketServiceTest, ANewCubeItemIsAddedWithSmInventoryAddItemThenTheCubeSize) {
	// ItemPacketService.java:214-228: CUBE -> SM_INVENTORY_ADD_ITEM(singletonList(item), player, addType), then the cube size
	Item& bandage = stored(810012, BANDAGE, 10);
	ItemPacketService::sendStorageUpdatePacket(player(), StorageType::CUBE, bandage, ItemAddType::QUEST_REWARD_ITEM);
	std::vector<std::vector<uint8_t>> packets = sent();
	ASSERT_EQ(packets.size(), 2u);
	EXPECT_EQ(javaOpcodeOf(packets[0]), SM_INVENTORY_ADD_ITEM_OPCODE);
	PacketReader reader(cp::bodyOf(packets[0]));
	EXPECT_EQ(reader.H(), 0x30) << "writeH(QUEST_REWARD_ITEM 0x30)";
	EXPECT_EQ(reader.H(), 1) << "one item";
	EXPECT_EQ(reader.D(), 810012);
	EXPECT_EQ(reader.D(), BANDAGE);
	EXPECT_EQ(packets[0], serialized(SM_INVENTORY_ADD_ITEM({Ptr<Item>(bandage)}, player(), ItemAddType::QUEST_REWARD_ITEM)));
	EXPECT_EQ(packets[1], cubeSize(StorageType::CUBE, 1));

	// the three-argument overload adds with ITEM_COLLECT (ItemPacketService.java:206-208)
	(*client)->clearSent();
	ItemPacketService::sendStorageUpdatePacket(player(), StorageType::CUBE, bandage);
	EXPECT_EQ(sent(), cp::exactly({serialized(SM_INVENTORY_ADD_ITEM({Ptr<Item>(bandage)}, player(), ItemAddType::ITEM_COLLECT)),
						  cubeSize(StorageType::CUBE, 1)}));
}

TEST_F(ItemPacketServiceTest, ANewWarehouseItemIsAddedWithSmWarehouseAddItemThenThatStoragesSize) {
	// ItemPacketService.java:224-227: default -> SM_WAREHOUSE_ADD_ITEM(item, storageType.getId(), player, addType), then the cube size
	stored(810013, BANDAGE, 1, StorageType::REGULAR_WAREHOUSE);
	Item& added = stored(810014, BANDAGE, 6, StorageType::REGULAR_WAREHOUSE);
	ItemPacketService::sendStorageUpdatePacket(player(), StorageType::REGULAR_WAREHOUSE, added, ItemAddType::BUY);
	std::vector<std::vector<uint8_t>> packets = sent();
	ASSERT_EQ(packets.size(), 2u);
	EXPECT_EQ(javaOpcodeOf(packets[0]), SM_WAREHOUSE_ADD_ITEM_OPCODE);
	PacketReader reader(cp::bodyOf(packets[0]));
	EXPECT_EQ(reader.C(), 1) << "writeC(warehouseType)";
	EXPECT_EQ(reader.H(), 0x1C) << "writeH(BUY 0x1C)";
	EXPECT_EQ(reader.H(), 1);
	EXPECT_EQ(reader.D(), 810014);
	EXPECT_EQ(packets[0], serialized(SM_WAREHOUSE_ADD_ITEM(added, 1, player(), ItemAddType::BUY)));
	EXPECT_EQ(packets[1], cubeSize(StorageType::REGULAR_WAREHOUSE, 2));

	// a pet bag: its id 32 in the add, its ordinal 4 in the cube size (a pet bag has no size arm, so it writes zeros)
	(*client)->clearSent();
	Item& inPetBag = loose(810025, BANDAGE, 6, StorageType::PET_BAG_6);
	ItemPacketService::sendStorageUpdatePacket(player(), StorageType::PET_BAG_6, inPetBag, ItemAddType::BUY);
	packets = sent();
	ASSERT_EQ(packets.size(), 2u);
	EXPECT_EQ(PacketReader(cp::bodyOf(packets[0])).C(), 32) << "writeC(warehouseType): PET_BAG_6's id, not its ordinal 4";
	EXPECT_EQ(packets[0], serialized(SM_WAREHOUSE_ADD_ITEM(inPetBag, 32, player(), ItemAddType::BUY)));
	EXPECT_EQ(packets[1], javaPacket(SM_CUBE_UPDATE_OPCODE, PacketWriter().C(0).C(4).D(0).C(0).C(0).C(0)));
}

TEST_F(ItemPacketServiceTest, ALegionWarehouseItemOfAPlayerWithoutALegionThrowsAtTheCubeSizeAsInJava) {
	// ItemPacketService.java:219-227: an item falls through to SM_WAREHOUSE_ADD_ITEM(item, 3, ...); then SM_CUBE_UPDATE.cubeSize(LEGION_WAREHOUSE)
	// dereferences player.getLegion() in the caller - a NullPointerException in Java as in C++
	Item& item = loose(810015, BANDAGE, 1, StorageType::LEGION_WAREHOUSE);
	EXPECT_THROW(ItemPacketService::sendStorageUpdatePacket(player(), StorageType::LEGION_WAREHOUSE, item, ItemAddType::ALL_SLOT),
		runtime::NullPointerException);
	EXPECT_EQ(sent(), cp::exactly({serialized(SM_WAREHOUSE_ADD_ITEM(item, 3, player(), ItemAddType::ALL_SLOT))}));
}

TEST_F(ItemPacketServiceTest, LegionWarehouseKinahIsNeverAddedWithSmWarehouseAddItem) {
	// ItemPacketService.java:219-223: LEGION_WAREHOUSE kinah -> SM_LEGION_EDIT(0x04, player.getLegion()) and break, no SM_WAREHOUSE_ADD_ITEM; then
	// SM_CUBE_UPDATE.cubeSize(LEGION_WAREHOUSE) throws in the caller. The fixture's player has no legion, so C++ builds no SM_LEGION_EDIT
	// (docs/deviations/P5-07.md) and nothing is sent before the throw.
	Item& kinah = loose(810026, KINAH, 500, StorageType::LEGION_WAREHOUSE);
	EXPECT_THROW(ItemPacketService::sendStorageUpdatePacket(player(), StorageType::LEGION_WAREHOUSE, kinah, ItemAddType::ALL_SLOT),
		runtime::NullPointerException);
	EXPECT_TRUE(sent().empty()) << "kinah does not fall through to SM_WAREHOUSE_ADD_ITEM";
}

// ------------------------------------------------------------------------------------------------------------------------- sendItemUnlockPacket

TEST_F(ItemPacketServiceTest, AnUnlockedItemIsSentAgainToTheStorageOfItsLocationWithAllSlot) {
	// ItemPacketService.java:230-234: StorageType.getStorageTypeById(item.getItemLocation()) -> sendStorageUpdatePacket(..., ALL_SLOT); an
	// unknown location sends nothing
	Item& inWarehouse = stored(810016, BANDAGE, 3, StorageType::REGULAR_WAREHOUSE);
	ItemPacketService::sendItemUnlockPacket(player(), inWarehouse);
	std::vector<std::vector<uint8_t>> packets = sent();
	ASSERT_EQ(packets.size(), 2u);
	PacketReader reader(cp::bodyOf(packets[0]));
	EXPECT_EQ(reader.C(), 1);
	EXPECT_EQ(reader.H(), 0x13) << "ALL_SLOT";
	EXPECT_EQ(packets[0], serialized(SM_WAREHOUSE_ADD_ITEM(inWarehouse, 1, player(), ItemAddType::ALL_SLOT)));
	EXPECT_EQ(packets[1], cubeSize(StorageType::REGULAR_WAREHOUSE, 1));

	(*client)->clearSent();
	Item& inCube = stored(810017, BANDAGE, 3);
	ItemPacketService::sendItemUnlockPacket(player(), inCube);
	EXPECT_EQ(sent(), cp::exactly({serialized(SM_INVENTORY_ADD_ITEM({Ptr<Item>(inCube)}, player(), ItemAddType::ALL_SLOT)),
						  cubeSize(StorageType::CUBE, 1)}));

	(*client)->clearSent();
	Item& nowhere = loose(810018, BANDAGE, 3);
	nowhere.setItemLocation(99);
	ItemPacketService::sendItemUnlockPacket(player(), nowhere);
	EXPECT_TRUE(sent().empty()) << "no storage type has id 99";
}

// ------------------------------------------------------------------------------------------------------------------------- updateItemAfter*

TEST_F(ItemPacketServiceTest, InfoChangesAndEquipSendSmInventoryUpdateItemWithTheirUpdateType) {
	// ItemPacketService.java:152-162: updateItemAfterInfoChange(player, item) is SM_INVENTORY_UPDATE_ITEM(player, item), whose update type is
	// DEC_ITEM_USE (SM_INVENTORY_UPDATE_ITEM.java); the three-argument one passes its type; updateItemAfterEquip passes EQUIP_UNEQUIP, which is
	// not sendable (no trailing mask)
	Item& bandage = stored(810019, BANDAGE, 2);
	ItemPacketService::updateItemAfterInfoChange(player(), bandage);
	ItemPacketService::updateItemAfterInfoChange(player(), bandage, ItemUpdateType::STATS_CHANGE);
	ItemPacketService::updateItemAfterEquip(player(), bandage);
	std::vector<std::vector<uint8_t>> packets = sent();
	ASSERT_EQ(packets.size(), 3u);
	for (const std::vector<uint8_t>& packet : packets)
		EXPECT_EQ(javaOpcodeOf(packet), SM_INVENTORY_UPDATE_ITEM_OPCODE);
	EXPECT_EQ(trailingMask(packets[0]), 0x16) << "the two-argument constructor's DEC_ITEM_USE";
	EXPECT_EQ(trailingMask(packets[1]), 0x00) << "STATS_CHANGE";
	EXPECT_EQ(packets[0], serialized(SM_INVENTORY_UPDATE_ITEM(player(), bandage, ItemUpdateType::DEC_ITEM_USE)));
	EXPECT_EQ(packets[1], serialized(SM_INVENTORY_UPDATE_ITEM(player(), bandage, ItemUpdateType::STATS_CHANGE)));
	EXPECT_EQ(packets[2], serialized(SM_INVENTORY_UPDATE_ITEM(player(), bandage, ItemUpdateType::EQUIP_UNEQUIP)));
	EXPECT_NE(packets[2], serialized(SM_INVENTORY_UPDATE_ITEM(player(), bandage, ItemUpdateType::STATS_CHANGE)))
		<< "EQUIP_UNEQUIP writes the equipped-slot blob, not the full blob";
}

// ------------------------------------------------------------------------------------------------------------------------- E-13 end to end

TEST_F(ItemPacketServiceTest, AnExpiredItemIsDeletedByTheExpireTimerTaskWithJavasPackets) {
	// A new character's Boon expires 4321 minutes after it was made: Item's constructor sets `now + expire_time * 60 - 1` (Item.java)
	const int32_t now = static_cast<int32_t>(commons::utils::currentTimeMillis() / 1000);
	Ref<Item> fresh = Item::create(810020, dataholders::DataManager::ITEM_DATA->getItemTemplate(BOON_3_DAY));
	EXPECT_NEAR(fresh->getExpireTime() - now, 4321 * 60 - 1, 1);
	player().getInventory().onLoadHandler(*fresh);
	items.push_back(fresh);

	// the login after the expiry: the DAO loads the Boon with its stored expire time, in the past, and one expired pass in the warehouse
	Item& boon = stored(810021, BOON_3_DAY, 1, StorageType::CUBE, now - 10);
	Item& warehouseBoon = stored(810022, BOON_7_DAY, 1, StorageType::REGULAR_WAREHOUSE, now - 10);

	network::test::LogCapture taskLog({"com.aionemu.commons.utils.concurrent.ExecuteWrapper"});
	Ref<TestExpireTimerTask> task = runtime::makeRef<TestExpireTimerTask>();
	// PlayerEnterWorldService registers the storages' items (ExpireTimerTask.registerExpirables)
	task->registerExpirables(player().getInventory().getItems(), player());
	task->registerExpirables(player().getWarehouse().getItems(), player());
	(*client)->clearSent();

	executor->advance(499ms);
	ASSERT_EQ(player().getInventory().size(), 2) << "the first run comes 500-550 ms after the task was made";

	executor->advance(51ms);
	// ExpireTimerTask.run -> Item.onExpire -> storage.delete(this) -> Storage.delete(item, DEFAULT, actor) -> sendItemDeletePacket, then the
	// timeout message of that storage (Item.java onExpire)
	EXPECT_FALSE(player().getInventory().getItemByObjId(810021)) << "the expired Boon left the cube";
	EXPECT_FALSE(player().getWarehouse().getItemByObjId(810022)) << "and the expired pass the warehouse";
	EXPECT_TRUE(player().getInventory().getItemByObjId(810020)) << "the fresh Boon stays";
	EXPECT_EQ(boon.getPersistentState(), PersistentState::DELETED);
	EXPECT_EQ(warehouseBoon.getPersistentState(), PersistentState::DELETED);

	const std::vector<std::vector<uint8_t>> cubeExpiry{javaPacket(SM_DELETE_ITEM_OPCODE, PacketWriter().D(810021).C(0x00)),
		cubeSize(StorageType::CUBE, 1), serialized(SM_SYSTEM_MESSAGE::STR_MSG_DELETE_CASH_ITEM_BY_TIMEOUT(boon.getL10n()))};
	const std::vector<std::vector<uint8_t>> warehouseExpiry{javaPacket(SM_DELETE_WAREHOUSE_ITEM_OPCODE, PacketWriter().C(1).D(810022).C(0x00)),
		cubeSize(StorageType::REGULAR_WAREHOUSE, 0),
		serialized(SM_SYSTEM_MESSAGE::STR_MSG_DELETE_CASH_ITEM_BY_TIMEOUT_IN_WAREHOUSE(warehouseBoon.getL10n()))};
	std::vector<std::vector<uint8_t>> packets = sent();
	ASSERT_EQ(packets.size(), 6u) << "both expiries in the same run (the map's order decides which comes first)";
	std::vector<std::vector<uint8_t>> first(packets.begin(), packets.begin() + 3);
	std::vector<std::vector<uint8_t>> second(packets.begin() + 3, packets.end());
	EXPECT_TRUE((first == cubeExpiry && second == warehouseExpiry) || (first == warehouseExpiry && second == cubeExpiry))
		<< "each expiry: SM_DELETE_ITEM / SM_DELETE_WAREHOUSE_ITEM (DEFAULT), SM_CUBE_UPDATE, the timeout message";
	EXPECT_EQ(taskLog.count("error|"), 0) << taskLog.dump();

	// the expired entries left the map (Java i.remove()): the expired Boon, put back into the cube as a probe, is not expired a second time
	player().getInventory().onLoadHandler(boon);

	// the task keeps its fixed rate: an item registered later expires on a later run, and that run sends its packets alone (the fresh Boon is
	// warned only 30 minutes before it expires)
	Item& later = stored(810023, BOON_3_DAY, 1, StorageType::CUBE, now - 1);
	task->registerExpirable(later, player());
	(*client)->clearSent();
	executor->advance(2000ms);
	EXPECT_FALSE(player().getInventory().getItemByObjId(810023));
	EXPECT_TRUE(player().getInventory().getItemByObjId(810021)) << "the probe stays: no entry of the map holds it any more";
	EXPECT_EQ(sent(), cp::exactly({javaPacket(SM_DELETE_ITEM_OPCODE, PacketWriter().D(810023).C(0x00)), cubeSize(StorageType::CUBE, 2),
						  serialized(SM_SYSTEM_MESSAGE::STR_MSG_DELETE_CASH_ITEM_BY_TIMEOUT(later.getL10n()))}))
		<< "the fresh Boon and the probe are left in the cube";
	EXPECT_EQ(taskLog.count("error|"), 0) << taskLog.dump();
	task->unregisterExpirables(player());
}

} // namespace
} // namespace aion::gameserver::services::item::test
