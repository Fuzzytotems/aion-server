#include "aion/gameserver/network/aion/serverpackets/SM_BROKER_SERVICE.h"

#include <vector>

#include "aion/commons/utils/Exception.h"
#include "aion/commons/utils/StringUtils.h"
#include "aion/commons/utils/TimeUtils.h"
#include "aion/gameserver/model/gameobjects/BrokerItem.h"
#include "aion/gameserver/model/gameobjects/Item.h"
#include "aion/gameserver/model/templates/item/ItemTemplate.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"
#include "aion/gameserver/network/aion/iteminfo/EnchantInfoBlobEntry.h"
#include "aion/gameserver/network/aion/iteminfo/ItemBlobEntry.h"
#include "aion/gameserver/network/aion/iteminfo/ItemInfoBlob.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/services/player/PlayerService.h"

namespace aion::gameserver::network::aion::serverpackets {

namespace {

/** Java: BrokerPacketType.getId() in ordinal order - SEARCHED_ITEMS(0) .. SHOW_SETTLED_ICON(5), SETTLED_ITEMS(5), REMOVE_SETTLED_ICON(6), SHOW_SELL_WINDOW(7) */
int32_t brokerPacketTypeId(SM_BROKER_SERVICE_BrokerPacketType type) {
	static constexpr int32_t IDS[] = {0, 1, 2, 3, 4, 5, 5, 6, 7};
	return IDS[static_cast<size_t>(type)];
}

/** Java: timestamp.getTime() of a Timestamp Java dereferences (NullPointerException for null) */
int64_t timeOf(const std::optional<commons::database::Timestamp>& timestamp, std::string_view name) {
	if (!timestamp)
		throw runtime::NullPointerException(std::string(name) + " is null");
	return timestamp->time_since_epoch().count();
}

} // namespace

namespace {

/** Java: the lambda of SETTLED_ITEMS_DYNAMIC_BODY_PART_SIZE_CALCULATOR (SM_BROKER_SERVICE.java:23, key SM_BROKER_SERVICE@L23:4) */
struct SettledItemsDynamicBodyPartSizeCalculator : runtime::TaskStruct {
	int32_t operator()(model::gameobjects::BrokerItem& item) const {
		// Java: item.getItemCreator() == null ? 2 : item.getItemCreator().length() * 2 + 2 (a null creator is "" in C++, which gives the same 2)
		return 32 + iteminfo::EnchantInfoBlobEntry::SIZE + commons::utils::StringUtils::utf16Length(item.getItemCreator()) * 2 + 2;
	}
};

} // namespace

const runtime::PinnedCallback<int32_t(model::gameobjects::BrokerItem&)> SM_BROKER_SERVICE::SETTLED_ITEMS_DYNAMIC_BODY_PART_SIZE_CALCULATOR{
	SettledItemsDynamicBodyPartSizeCalculator{}};

SM_BROKER_SERVICE::SM_BROKER_SERVICE(model::gameobjects::BrokerItem& brokerItem, int32_t messageValue, int32_t itemsCountValue)
	: AionServerPacket(opcodeOf<SM_BROKER_SERVICE>), type(BrokerPacketType::REGISTER_ITEM),
	  brokerItems{runtime::Ref<model::gameobjects::BrokerItem>(brokerItem)}, itemsCount(itemsCountValue), message(messageValue) {
}

SM_BROKER_SERVICE::SM_BROKER_SERVICE(int32_t messageValue)
	: AionServerPacket(opcodeOf<SM_BROKER_SERVICE>), type(BrokerPacketType::REGISTER_ITEM), message(messageValue) {
}

SM_BROKER_SERVICE::SM_BROKER_SERVICE(const std::vector<runtime::Ptr<model::gameobjects::BrokerItem>>& brokerItemsValue)
	: AionServerPacket(opcodeOf<SM_BROKER_SERVICE>), type(BrokerPacketType::REGISTERED_ITEMS),
	  brokerItems(brokerItemsValue.begin(), brokerItemsValue.end()) {
}

SM_BROKER_SERVICE::SM_BROKER_SERVICE(const std::vector<runtime::Ptr<model::gameobjects::BrokerItem>>& brokerItemsValue, int32_t totalItemCountValue,
	int32_t pageIndexValue, int64_t settledKinahValue)
	: AionServerPacket(opcodeOf<SM_BROKER_SERVICE>), type(BrokerPacketType::SETTLED_ITEMS),
	  brokerItems(brokerItemsValue.begin(), brokerItemsValue.end()), totalItemCount(totalItemCountValue), pageIndex(pageIndexValue),
	  settledKinah(settledKinahValue) {
}

SM_BROKER_SERVICE::SM_BROKER_SERVICE(const std::vector<runtime::Ptr<model::gameobjects::BrokerItem>>& brokerItemsValue, int32_t itemsCountValue,
	int32_t startPageValue)
	: AionServerPacket(opcodeOf<SM_BROKER_SERVICE>), type(BrokerPacketType::SEARCHED_ITEMS),
	  brokerItems(brokerItemsValue.begin(), brokerItemsValue.end()), itemsCount(itemsCountValue), startPage(startPageValue) {
}

SM_BROKER_SERVICE::SM_BROKER_SERVICE(bool showSettledIcon, int64_t settledKinahValue)
	: AionServerPacket(opcodeOf<SM_BROKER_SERVICE>),
	  type(showSettledIcon ? BrokerPacketType::SHOW_SETTLED_ICON : BrokerPacketType::REMOVE_SETTLED_ICON), settledKinah(settledKinahValue) {
}

SM_BROKER_SERVICE::SM_BROKER_SERVICE(int8_t unkValue, int32_t itemIdValue)
	: AionServerPacket(opcodeOf<SM_BROKER_SERVICE>), type(BrokerPacketType::CANCEL_REGISTERED_ITEM), itemId(itemIdValue), unk(unkValue) {
}

SM_BROKER_SERVICE::SM_BROKER_SERVICE(int8_t unkValue, int32_t itemIdValue, int64_t currentLowValue, int64_t currentHighValue)
	: AionServerPacket(opcodeOf<SM_BROKER_SERVICE>), type(BrokerPacketType::SHOW_SELL_WINDOW), currentLow(currentLowValue),
	  currentHigh(currentHighValue), itemId(itemIdValue), unk(unkValue) {
}

SM_BROKER_SERVICE::~SM_BROKER_SERVICE() = default;

void SM_BROKER_SERVICE::writeImpl(AionConnection* con) {
	switch (type) {
		case BrokerPacketType::SEARCHED_ITEMS:
			writeSearchedItems();
			break;
		case BrokerPacketType::REGISTERED_ITEMS:
			writeRegisteredItems();
			break;
		case BrokerPacketType::REGISTER_ITEM:
			writeRegisterItem();
			break;
		case BrokerPacketType::CANCEL_REGISTERED_ITEM:
			writeCancelRegisteredItem();
			break;
		case BrokerPacketType::SHOW_SETTLED_ICON:
			writeShowSettledIcon();
			break;
		case BrokerPacketType::REMOVE_SETTLED_ICON:
			writeRemoveSettledIcon();
			break;
		case BrokerPacketType::SETTLED_ITEMS:
			writeShowSettledItems();
			break;
		case BrokerPacketType::SHOW_SELL_WINDOW:
			writeShowSellWindow();
			break;
		default:
			break;
	}
}

void SM_BROKER_SERVICE::writeCancelRegisteredItem() {
	writeC(brokerPacketTypeId(type));
	writeC(unk);
	writeD(itemId);
}

void SM_BROKER_SERVICE::writeSearchedItems() {
	writeC(brokerPacketTypeId(type));
	writeD(itemsCount);
	writeC(0);
	writeH(startPage);
	writeH(static_cast<int32_t>(brokerItems.size()));
	for (const runtime::Ref<model::gameobjects::BrokerItem>& item : brokerItems) {
		writeItemInfo(*item);
	}
}

void SM_BROKER_SERVICE::writeRegisteredItems() {
	writeC(brokerPacketTypeId(type));
	writeD(0x00);
	writeH(static_cast<int32_t>(brokerItems.size())); // you can register a max of 15 items, so 0x0F
	for (const runtime::Ref<model::gameobjects::BrokerItem>& brokerItem : brokerItems) {
		writeRegisteredItemInfo(*brokerItem);
	}
}

void SM_BROKER_SERVICE::writeRegisterItem() {
	writeC(brokerPacketTypeId(type));
	writeC(message);
	if (message == 0) {
		writeC(itemsCount + 1); // item pos in list
		// Java: brokerItems.getFirst() - only SM_BROKER_SERVICE(int message) leaves the list empty, and there Java's field is null (List.of(brokerItem)
		// always has one element), so the call throws NullPointerException
		if (brokerItems.empty())
			throw runtime::NullPointerException("brokerItems is null");
		writeRegisteredItemInfo(*brokerItems.front());
	} else {
		writeB(std::vector<uint8_t>(174));
		writeH(255); // right after creatorName string
		writeB(std::vector<uint8_t>(7));
	}
}

void SM_BROKER_SERVICE::writeShowSettledIcon() {
	writeC(brokerPacketTypeId(type));
	writeQ(settledKinah);
	writeD(0x00);
	writeH(0x00);
	writeH(0x01);
	writeC(0x00);
}

void SM_BROKER_SERVICE::writeRemoveSettledIcon() {
	writeH(brokerPacketTypeId(type));
}

void SM_BROKER_SERVICE::writeShowSettledItems() {
	writeC(brokerPacketTypeId(type));
	writeQ(settledKinah);
	writeD(totalItemCount); // total item count to determine total page count
	writeH(pageIndex); // zero-based index of the currently selected page
	writeC(0); // 1 clears the list (no items must be sent)
	writeH(static_cast<int32_t>(brokerItems.size())); // items sent in this packet (client will request items of unsent pages when needed)
	for (const runtime::Ref<model::gameobjects::BrokerItem>& settledItem : brokerItems) {
		writeD(settledItem->getItemId());
		writeQ(settledItem->isSold() ? settledItem->getPrice() * settledItem->getItemCount() : 0);
		writeQ(settledItem->getItemCount());
		writeQ(settledItem->getItemCount());
		writeD(static_cast<int32_t>(timeOf(settledItem->getSettleTime(), "BrokerItem settleTime") / 60000));
		if (!settledItem->getItem()) // sold items are null because they are not in item_location 126 (broker) anymore
			writeB(std::vector<uint8_t>(static_cast<size_t>(iteminfo::EnchantInfoBlobEntry::SIZE)));
		else
			iteminfo::EnchantInfoBlobEntry::writeInfo(getBuf(), *settledItem->getItem());
		writeS(settledItem->getItemCreator());
	}
}

void SM_BROKER_SERVICE::writeShowSellWindow() {
	writeC(brokerPacketTypeId(type));
	writeC(unk);
	writeD(itemId);
	writeD(0);
	writeD(0);
	writeC(3); // 7dayAverage
	writeQ(currentLow); // currentLow
	writeQ(currentHigh); // currentHigh
}

void SM_BROKER_SERVICE::writeRegisteredItemInfo(model::gameobjects::BrokerItem& brokerItem) {
	using ItemBlobType = iteminfo::ItemInfoBlob::ItemBlobType;
	runtime::Ptr<model::gameobjects::Item> item = brokerItem.getItem();
	writeD(brokerItem.getItemUniqueId());
	writeD(brokerItem.getItemId());
	writeQ(brokerItem.getPrice() * brokerItem.getItemCount());
	writeQ(item->getItemCount());
	writeQ(item->getItemCount());
	// Java: (int) TimeUnit.MILLISECONDS.toDays(expireTime - now), truncated towards zero
	int32_t daysLeft =
		static_cast<int32_t>((timeOf(brokerItem.getExpireTime(), "BrokerItem expireTime") - commons::utils::currentTimeMillis()) / 86400000);
	writeC(daysLeft);
	iteminfo::EnchantInfoBlobEntry::writeInfo(getBuf(), *item);
	// ItemInfoBlob.newBlobEntry(ItemBlobType.PREMIUM_OPTION, null, item).writeThisBlob(getBuf());
	writeS(brokerItem.getItemCreator());
	writeH(0);
	writeC(0);
	iteminfo::ItemInfoBlob::newBlobEntry(ItemBlobType::POLISH_INFO, nullptr, *item)->writeThisBlob(getBuf());
	iteminfo::ItemInfoBlob::newBlobEntry(ItemBlobType::WRAP_INFO, nullptr, *item)->writeThisBlob(getBuf());
	writeC(brokerItem.isSplittingAvailable() ? 1 : 0);
}

void SM_BROKER_SERVICE::writeItemInfo(model::gameobjects::BrokerItem& brokerItem) {
	using ItemBlobType = iteminfo::ItemInfoBlob::ItemBlobType;
	runtime::Ptr<model::gameobjects::Item> item = brokerItem.getItem();
	writeD(item->getObjectId());
	writeD(item->getItemTemplate()->getTemplateId());
	writeQ(brokerItem.getPrice() * brokerItem.getItemCount());
	writeQ(brokerItem.getAveragePrice()); // AWR price
	writeQ(item->getItemCount());
	iteminfo::EnchantInfoBlobEntry::writeInfo(getBuf(), *item);
	// ItemInfoBlob.newBlobEntry(ItemBlobType.PREMIUM_OPTION, null, item).writeThisBlob(getBuf());
	writeS(services::player::PlayerService::getPlayerName(brokerItem.getSellerId()).value_or("")); // Java null: ""
	writeS(brokerItem.getItemCreator()); // creator
	writeH(0);
	writeC(0);
	iteminfo::ItemInfoBlob::newBlobEntry(ItemBlobType::POLISH_INFO, nullptr, *item)->writeThisBlob(getBuf());
	iteminfo::ItemInfoBlob::newBlobEntry(ItemBlobType::WRAP_INFO, nullptr, *item)->writeThisBlob(getBuf());
	writeC(brokerItem.isSplittingAvailable() ? 1 : 0);
}

} // namespace aion::gameserver::network::aion::serverpackets
