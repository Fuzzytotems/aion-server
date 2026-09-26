#pragma once

// InventoryModel: a character's items as its client knows them (m5b3-plan.md G-03, lifted out of M5b3ScenarioTest.cpp for M5c's X16 as
// m5c-plan.md G-02 asks). Every item packet the client received is applied in arrival order: SM_INVENTORY_INFO and SM_WAREHOUSE_INFO at enter
// world, then SM_INVENTORY_ADD_ITEM, SM_INVENTORY_UPDATE_ITEM, SM_DELETE_ITEM and their warehouse twins, and SM_CUBE_UPDATE's item count.
// The model answers "the count of the last packet the client got about it" - what a loot merges into, what a split or a sale starts from, what
// the database must hold after a quit - and, with two models, whether one object id is in two clients' inventories at once (m5c-plan.md X16).
//
// Nothing here reads a C++ server class: the packets are decoded with decoders/ItemDecoders.h and decoders/PacketDecoders.h, which are written
// from the Java writeImpl (m5a-plan.md D9). A body that does not decode is recorded in decodeFailures, not thrown, so a gate reports every
// failure of a burst at once.

#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include "GameSession.h"
#include "decoders/ItemDecoders.h" // STORAGE_CUBE, STORAGE_REGULAR_WAREHOUSE

namespace aion::gameserver::scenario {

/** one item of the model */
struct ModelItem {
	/** StorageType ids of the two storages the model keeps (StorageType.java:7-8), the `item_location` column */
	static constexpr int32_t CUBE = decoders::STORAGE_CUBE;
	static constexpr int32_t REGULAR_WAREHOUSE = decoders::STORAGE_REGULAR_WAREHOUSE;

	int32_t objectId = 0;
	int32_t itemId = 0;
	int64_t count = 0;
	int32_t location = CUBE;
	/** the EQUIPPED_SLOT blob entry / the equipped state of SM_INVENTORY_INFO: the slot mask, 0 when the item is not equipped */
	int64_t equippedSlot = 0;
	/** the low 16 bits of Item.getEquipmentSlot() the last ADD/INFO packet carried (0xFFFF for -1 and 65535) */
	uint16_t slot = 0xFFFF;
	int32_t godStoneId = 0;
};

class InventoryModel {
public:
	/** ItemId.KINAH: the kinah item every storage keeps apart from its stacks (Storage.java:172-173) */
	static constexpr int32_t KINAH_ITEM_ID = 182400001;

	std::map<int32_t, ModelItem> items;
	std::vector<std::string> decodeFailures;
	/** the last SM_CUBE_UPDATE(cubeSize) item count per StorageType, for the message of a mismatch */
	std::map<int32_t, int32_t> lastCubeUpdateCount;

	/**
	 * Follows `session`'s recorder from its first packet on: the items are cleared and the next sync() applies everything the session recorded
	 * (a new session after a relog starts over with its enter-world SM_INVENTORY_INFO). decodeFailures and lastCubeUpdateCount are kept.
	 */
	void follow(const GameSession* session);
	/** follow() over any packet list that only grows - a session's recorder, or a test's vector */
	void followPackets(const std::vector<GameSession::Packet>* packets);

	/** applies the packets recorded since the last call */
	void sync();

	/** applies one packet (the ones that are not item packets are ignored); `index` names it in a decode failure */
	void apply(const GameSession::Packet& packet, size_t index);

	/** the stacks that take a cube slot: in the cube, not equipped, not the kinah (Storage keeps the kinah item apart, Storage.java:172-173) */
	std::vector<ModelItem> cubeStacks() const;
	/** the unequipped stacks of an item id in one storage */
	std::vector<ModelItem> byItemId(int32_t itemId, int32_t location = ModelItem::CUBE) const;
	std::optional<ModelItem> byObjectId(int32_t objectId) const;
	/** the equipped item of that id, if any */
	std::optional<ModelItem> equipped(int32_t itemId) const;
	/** the count of the cube's kinah item, 0 before the client got one */
	int64_t kinah() const;
	/** one "object:item x count @location (equipped mask) slot N" per item, for a failure message */
	std::string describe() const;

private:
	void put(const decoders::InventoryItem& item, int32_t location);
	void update(const decoders::InventoryItem& item);

	const std::vector<GameSession::Packet>* source = nullptr;
	size_t scanned = 0;
};

} // namespace aion::gameserver::scenario
