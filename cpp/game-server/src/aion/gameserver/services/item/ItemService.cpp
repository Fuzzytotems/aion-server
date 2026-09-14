#include "aion/gameserver/services/item/ItemService.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/commons/logging/LoggerFactory.h"

namespace aion::gameserver::services::item {

static const auto log = commons::logging::LoggerFactory::getLogger("ITEM_LOG");

ItemService::ItemUpdatePredicate::ItemUpdatePredicate(ItemPacketService_ItemAddType value, ItemPacketService_ItemUpdateType itemUpdateTypeValue)
	: itemUpdateType(itemUpdateTypeValue), itemAddType(value) {
}

runtime::Ref<ItemService::ItemUpdatePredicate> ItemService::ItemUpdatePredicate::create(ItemPacketService_ItemAddType value, ItemPacketService_ItemUpdateType itemUpdateTypeValue) {
	return runtime::makeRef<ItemService::ItemUpdatePredicate>(value, itemUpdateTypeValue);
}

ItemService::ItemUpdatePredicate::ItemUpdatePredicate()
	: ItemUpdatePredicate(ItemPacketService_ItemAddType::ITEM_COLLECT, ItemPacketService_ItemUpdateType::INC_ITEM_COLLECT) {
}

runtime::Ref<ItemService::ItemUpdatePredicate> ItemService::ItemUpdatePredicate::create() {
	return runtime::makeRef<ItemService::ItemUpdatePredicate>();
}

ItemPacketService_ItemUpdateType ItemService::ItemUpdatePredicate::getUpdateType(model::gameobjects::Item& item, bool isIncrease) {
	AION_UNPORTED();
}

bool ItemService::ItemUpdatePredicate::changeItem(model::gameobjects::Item&) {
	return true;
}

ItemService::ItemUpdatePredicate::~ItemUpdatePredicate() = default;

// Java: DEFAULT_UPDATE_PREDICATE = new ItemUpdatePredicate(ItemAddType.ITEM_COLLECT, ItemUpdateType.INC_ITEM_COLLECT)
const runtime::Ref<ItemService::ItemUpdatePredicate>& ItemService::DEFAULT_UPDATE_PREDICATE =
	*new runtime::Ref<ItemService::ItemUpdatePredicate>(ItemUpdatePredicate::create(ItemPacketService_ItemAddType::ITEM_COLLECT,
		ItemPacketService_ItemUpdateType::INC_ITEM_COLLECT));

int64_t ItemService::addItem(model::gameobjects::player::Player& player, int32_t itemId, int64_t count, bool allowInventoryOverflow) {
	AION_UNPORTED();
}

int64_t ItemService::addItem(model::gameobjects::player::Player& player, int32_t itemId, int64_t count) {
	AION_UNPORTED();
}

int64_t ItemService::addItem(model::gameobjects::player::Player& player, int32_t itemId, int64_t count, bool allowInventoryOverflow, ItemService::ItemUpdatePredicate& predicate) {
	AION_UNPORTED();
}

int64_t ItemService::addItem(model::gameobjects::player::Player& player, model::gameobjects::Item& sourceItem) {
	AION_UNPORTED();
}

int64_t ItemService::addItem(model::gameobjects::player::Player& player, model::gameobjects::Item& sourceItem, int64_t count) {
	AION_UNPORTED();
}

int64_t ItemService::addItem(model::gameobjects::player::Player& player, model::gameobjects::Item& sourceItem, int64_t count, bool allowInventoryOverflow, ItemService::ItemUpdatePredicate& predicate) {
	AION_UNPORTED();
}

int64_t ItemService::addItem(model::gameobjects::player::Player& player, int32_t itemId, int64_t count, runtime::Ptr<model::gameobjects::Item> sourceItem, bool allowInventoryOverflow, ItemService::ItemUpdatePredicate& predicate) {
	AION_UNPORTED();
}

int64_t ItemService::addNonStackableItem(model::gameobjects::player::Player& player, const model::templates::item::ItemTemplate* itemTemplate, int64_t count, runtime::Ptr<model::gameobjects::Item> sourceItem, bool allowInventoryOverflow, ItemService::ItemUpdatePredicate& predicate) {
	AION_UNPORTED();
}

void ItemService::copyItemInfo(model::gameobjects::Item& sourceItem, model::gameobjects::Item& newItem) {
	AION_UNPORTED();
}

int64_t ItemService::addStackableItem(model::gameobjects::player::Player& player, const model::templates::item::ItemTemplate* itemTemplate, int64_t count, bool allowInventoryOverflow, ItemService::ItemUpdatePredicate& predicate) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::services::item
