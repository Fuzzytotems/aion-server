#include "aion/gameserver/services/item/ItemService.h"

#include <memory>
#include <string>
#include <vector>

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/gameserver/configs/main/LoggingConfig.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/ItemData.h"
#include "aion/gameserver/dataholders/loadingutils/EnumTraits.h"
#include "aion/gameserver/model/gameobjects/Item.h"
#include "aion/gameserver/model/gameobjects/player/Equipment.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/items/GodStone.h"
#include "aion/gameserver/model/items/IdianStone.h"
#include "aion/gameserver/model/items/ManaStone.h"
#include "aion/gameserver/model/items/storage/Storage.h"
#include "aion/gameserver/model/templates/item/ItemTemplate.h"
#include "aion/gameserver/model/templates/item/enums/ItemGroup.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/services/item/ItemFactory.h"
#include "aion/gameserver/services/item/ItemPacketService_ItemUpdateTypeInfo.h"
#include "aion/gameserver/services/item/ItemSocketService.h"
#include "aion/gameserver/taskmanager/tasks/ExpireTimerTask.h"
#include "aion/gameserver/utils/PacketSendUtility.h"

namespace aion::gameserver::services::item {

static const auto log = commons::logging::LoggerFactory::getLogger("ITEM_LOG");

namespace {

using model::gameobjects::Item;
using model::gameobjects::player::Player;
using model::items::storage::Storage;
using model::templates::item::ItemTemplate;
using network::aion::serverpackets::SM_SYSTEM_MESSAGE;
using runtime::Ptr;
using runtime::Ref;
using utils::PacketSendUtility;

} // namespace

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
	if (item.getItemTemplate()->isKinah())
		return getKinahUpdateTypeFromAddType(itemAddType, isIncrease);
	return itemUpdateType;
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
	return addItem(player, itemId, count, nullptr, allowInventoryOverflow, *DEFAULT_UPDATE_PREDICATE);
}

int64_t ItemService::addItem(model::gameobjects::player::Player& player, int32_t itemId, int64_t count) {
	return addItem(player, itemId, count, nullptr, false, *DEFAULT_UPDATE_PREDICATE);
}

int64_t ItemService::addItem(model::gameobjects::player::Player& player, int32_t itemId, int64_t count, bool allowInventoryOverflow, ItemService::ItemUpdatePredicate& predicate) {
	return addItem(player, itemId, count, nullptr, allowInventoryOverflow, predicate);
}

int64_t ItemService::addItem(model::gameobjects::player::Player& player, model::gameobjects::Item& sourceItem) {
	return addItem(player, sourceItem.getItemId(), sourceItem.getItemCount(), Ptr<Item>(sourceItem), true, *DEFAULT_UPDATE_PREDICATE);
}

int64_t ItemService::addItem(model::gameobjects::player::Player& player, model::gameobjects::Item& sourceItem, int64_t count) {
	return addItem(player, sourceItem.getItemId(), count, Ptr<Item>(sourceItem), false, *DEFAULT_UPDATE_PREDICATE);
}

int64_t ItemService::addItem(model::gameobjects::player::Player& player, model::gameobjects::Item& sourceItem, int64_t count, bool allowInventoryOverflow, ItemService::ItemUpdatePredicate& predicate) {
	return addItem(player, sourceItem.getItemId(), count, Ptr<Item>(sourceItem), allowInventoryOverflow, predicate);
}

int64_t ItemService::addItem(model::gameobjects::player::Player& player, int32_t itemId, int64_t count, runtime::Ptr<model::gameobjects::Item> sourceItem, bool allowInventoryOverflow, ItemService::ItemUpdatePredicate& predicate) {
	if (count <= 0)
		return 0;

	const ItemTemplate* itemTemplate = dataholders::DataManager::ITEM_DATA->getItemTemplate(itemId);
	// Java: Objects.requireNonNull(itemTemplate, "No item with id " + itemId); requireNonNull(predicate, ...) - a reference is never null
	if (itemTemplate == nullptr)
		throw runtime::NullPointerException("No item with id " + std::to_string(itemId));

	if (configs::main::LoggingConfig::LOG_ITEM.load())
		log.info("Item: " + std::to_string(itemTemplate->getTemplateId()) + " [" + itemTemplate->getName() + "] added to player " + player.getName()
			+ " (count: " + std::to_string(count) + ") (type: " + std::string(xml::enumName(predicate.getAddType())) + ")");

	Storage& inventory = player.getInventory();
	if (itemTemplate->isKinah()) {
		// quests do not add here
		inventory.increaseKinah(count);
		return 0;
	}

	if (itemTemplate->isStackable())
		count = addStackableItem(player, itemTemplate, count, allowInventoryOverflow, predicate);
	else
		count = addNonStackableItem(player, itemTemplate, count, sourceItem, allowInventoryOverflow, predicate);

	if (count > 0 && inventory.isFull(itemTemplate->getExtraInventoryId()))
		PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_MSG_DICE_INVEN_ERROR());

	return count;
}

int64_t ItemService::addNonStackableItem(model::gameobjects::player::Player& player, const model::templates::item::ItemTemplate* itemTemplate, int64_t count, runtime::Ptr<model::gameobjects::Item> sourceItem, bool allowInventoryOverflow, ItemService::ItemUpdatePredicate& predicate) {
	Storage& inventory = player.getInventory();
	while ((allowInventoryOverflow || !inventory.isFull(itemTemplate->getExtraInventoryId())) && count > 0) {
		Ref<Item> newItem = ItemFactory::newItem(itemTemplate->getTemplateId());

		taskmanager::tasks::ExpireTimerTask::getInstance().registerExpirable(*newItem, player);
		if (sourceItem) {
			copyItemInfo(*sourceItem, *newItem);
		}
		predicate.changeItem(*newItem);
		inventory.add(*newItem, predicate.getAddType());
		count--;
	}
	return count;
}

/**
 * Deviation (docs/deviations/P5-07.md): Java hands the source's IdianStone object itself to the new item (`newItem.setIdianStone(
 * sourceItem.getIdianStone())`), so both items share one stone whose back reference and persisted item id stay the source's. A C++ IdianStone
 * is a part of exactly one Item, so the new item gets a stone of its own with the source stone's item id, polish number and polish charge, NEW
 * like the mana stones and the godstone this method copies.
 */
void ItemService::copyItemInfo(model::gameobjects::Item& sourceItem, model::gameobjects::Item& newItem) {
	newItem.setOptionalSockets(sourceItem.getOptionalSockets());
	newItem.setItemCreator(sourceItem.getItemCreator());
	if (sourceItem.hasManaStones()) {
		for (const Ptr<model::items::ManaStone>& manaStone : sourceItem.getItemStones()->snapshot())
			ItemSocketService::addManaStone(Ptr<Item>(newItem), manaStone->getItemId(), false);
	}
	if (sourceItem.getGodStone())
		newItem.addGodStone(sourceItem.getGodStone()->getItemId(), sourceItem.getGodStone()->getActivatedCount());
	newItem.setEnchantLevel(sourceItem.getEnchantLevel());
	newItem.setAmplified(sourceItem.isAmplified());
	newItem.setBuffSkill(sourceItem.getBuffSkill());
	newItem.setTempering(sourceItem.getTempering());
	newItem.setSoulBound(sourceItem.isSoulBound());
	newItem.setTuneCount(sourceItem.getTuneCount());
	newItem.setBonusStats(sourceItem.getBonusStatsId(), true);
	if (Ptr<model::items::IdianStone> idianStone = sourceItem.getIdianStone())
		newItem.setIdianStone(std::make_unique<model::items::IdianStone>(idianStone->getItemId(), model::gameobjects::Persistable::PersistentState::NEW,
			newItem, idianStone->getPolishNumber(), idianStone->getPolishCharge()));
	else
		newItem.setIdianStone(nullptr);
	newItem.setItemColor(sourceItem.getItemColor());
	newItem.setEnchantBonus(sourceItem.getEnchantBonus());
	newItem.setItemSkinTemplate(sourceItem.getItemSkinTemplate());
}

int64_t ItemService::addStackableItem(model::gameobjects::player::Player& player, const model::templates::item::ItemTemplate* itemTemplate, int64_t count, bool allowInventoryOverflow, ItemService::ItemUpdatePredicate& predicate) {
	std::vector<Ptr<Item>> items;
	// dirty & hacky check for arrows and shards...
	if (itemTemplate->getItemGroup() == model::templates::item::enums::ItemGroup::POWER_SHARDS) {
		model::gameobjects::player::Equipment& equipment = player.getEquipment();
		items = equipment.getEquippedItemsByItemId(itemTemplate->getTemplateId());
		for (const Ptr<Item>& item : items) {
			if (count == 0) {
				break;
			}
			count = equipment.increaseEquippedItemCount(*item, count);
		}
	}

	Storage& inventory = player.getInventory();
	items = inventory.getItemsByItemId(itemTemplate->getTemplateId());
	for (const Ptr<Item>& item : items) {
		if (count == 0) {
			break;
		}
		count = inventory.increaseItemCount(*item, count, predicate.getUpdateType(*item, true));
	}

	while (count > 0 && (allowInventoryOverflow || !inventory.isFull(itemTemplate->getExtraInventoryId()))) {
		Ref<Item> newItem = ItemFactory::newItem(itemTemplate->getTemplateId(), count);
		count -= newItem->getItemCount();
		inventory.add(*newItem, predicate.getAddType());
	}
	return count;
}

} // namespace aion::gameserver::services::item
