#include "InventoryModel.h"

#include "decoders/PacketDecoders.h"

namespace aion::gameserver::scenario {

void InventoryModel::follow(const GameSession* session) {
	followPackets(session != nullptr ? &session->recorded() : nullptr);
}

void InventoryModel::followPackets(const std::vector<GameSession::Packet>* packets) {
	source = packets;
	scanned = 0;
	items.clear();
}

void InventoryModel::sync() {
	if (source == nullptr)
		return;
	for (; scanned < source->size(); scanned++)
		apply((*source)[scanned], scanned);
}

std::vector<ModelItem> InventoryModel::cubeStacks() const {
	std::vector<ModelItem> stacks;
	for (const auto& [id, item] : items)
		if (item.location == ModelItem::CUBE && item.equippedSlot == 0 && item.itemId != KINAH_ITEM_ID)
			stacks.push_back(item);
	return stacks;
}

std::vector<ModelItem> InventoryModel::byItemId(int32_t itemId, int32_t location) const {
	std::vector<ModelItem> found;
	for (const auto& [id, item] : items)
		if (item.itemId == itemId && item.location == location && item.equippedSlot == 0)
			found.push_back(item);
	return found;
}

std::optional<ModelItem> InventoryModel::byObjectId(int32_t objectId) const {
	const auto found = items.find(objectId);
	if (found == items.end())
		return std::nullopt;
	return found->second;
}

std::optional<ModelItem> InventoryModel::equipped(int32_t itemId) const {
	for (const auto& [id, item] : items)
		if (item.itemId == itemId && item.equippedSlot != 0)
			return item;
	return std::nullopt;
}

int64_t InventoryModel::kinah() const {
	for (const auto& [id, item] : items)
		if (item.itemId == KINAH_ITEM_ID && item.location == ModelItem::CUBE)
			return item.count;
	return 0;
}

std::string InventoryModel::describe() const {
	std::string text;
	for (const auto& [id, item] : items) {
		if (!text.empty())
			text += "; ";
		text += std::to_string(id) + ":" + std::to_string(item.itemId) + "x" + std::to_string(item.count) + "@" + std::to_string(item.location) +
		        (item.equippedSlot != 0 ? "(equipped " + std::to_string(item.equippedSlot) + ")" : "") + " slot " + std::to_string(item.slot);
	}
	return text;
}

void InventoryModel::put(const decoders::InventoryItem& item, int32_t location) {
	ModelItem& model = items[item.objectId];
	model.objectId = item.objectId;
	if (item.templateId != 0)
		model.itemId = item.templateId;
	model.location = location;
	if (item.general)
		model.count = item.general->count;
	model.equippedSlot = item.equippedSlotBlob.value_or(0);
	model.slot = item.equipmentSlot;
	if (item.enchant)
		model.godStoneId = item.enchant->godStoneId;
}

void InventoryModel::update(const decoders::InventoryItem& item) {
	const auto found = items.find(item.objectId);
	if (found == items.end()) {
		decodeFailures.push_back("an update of object " + std::to_string(item.objectId) + ", which the client does not have");
		return;
	}
	if (item.general)
		found->second.count = item.general->count;
	if (item.equippedSlotBlob)
		found->second.equippedSlot = *item.equippedSlotBlob;
	if (item.enchant)
		found->second.godStoneId = item.enchant->godStoneId;
}

void InventoryModel::apply(const GameSession::Packet& packet, size_t index) {
	try {
		if (packet.name == "SM_INVENTORY_INFO") {
			const decoders::InventoryInfo info = decoders::decodeInventoryInfo(packet.data);
			if (info.firstPacket)
				std::erase_if(items, [](const auto& entry) { return entry.second.location == ModelItem::CUBE; });
			for (const decoders::InventoryItem& item : info.items)
				put(item, ModelItem::CUBE);
		} else if (packet.name == "SM_WAREHOUSE_INFO") {
			const decoders::WarehouseInfo info = decoders::decodeWarehouseInfo(packet.data);
			if (info.warehouseType != ModelItem::REGULAR_WAREHOUSE)
				return;
			if (info.firstPacket)
				std::erase_if(items, [](const auto& entry) { return entry.second.location == ModelItem::REGULAR_WAREHOUSE; });
			for (const decoders::InventoryItem& item : info.items)
				put(item, ModelItem::REGULAR_WAREHOUSE);
		} else if (packet.name == "SM_INVENTORY_ADD_ITEM") {
			for (const decoders::InventoryItem& item : decoders::decodeInventoryAddItem(packet.data).items)
				put(item, ModelItem::CUBE);
		} else if (packet.name == "SM_INVENTORY_UPDATE_ITEM") {
			update(decoders::decodeInventoryUpdateItem(packet.data).item);
		} else if (packet.name == "SM_DELETE_ITEM") {
			items.erase(decoders::decodeDeleteItem(packet.data).objectId);
		} else if (packet.name == "SM_WAREHOUSE_ADD_ITEM") {
			const decoders::WarehouseAddItem add = decoders::decodeWarehouseAddItem(packet.data);
			for (const decoders::InventoryItem& item : add.items)
				put(item, add.warehouseType);
		} else if (packet.name == "SM_WAREHOUSE_UPDATE_ITEM") {
			update(decoders::decodeWarehouseUpdateItem(packet.data).item);
		} else if (packet.name == "SM_DELETE_WAREHOUSE_ITEM") {
			items.erase(decoders::decodeDeleteWarehouseItem(packet.data).objectId);
		} else if (packet.name == "SM_CUBE_UPDATE") {
			const decoders::CubeUpdate cube = decoders::decodeCubeUpdate(packet.data);
			if (cube.action == 0)
				lastCubeUpdateCount[cube.actionValue] = cube.itemsCount;
		}
	} catch (const decoders::DecodeError& error) {
		decodeFailures.push_back(packet.name + " at " + std::to_string(index) + ": " + error.what());
	}
}

} // namespace aion::gameserver::scenario
