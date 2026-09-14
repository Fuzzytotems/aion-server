#include "aion/gameserver/network/aion/serverpackets/SM_BROKER_SERVICE.h"

#include "aion/gameserver/model/gameobjects/BrokerItem.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"
#include "aion/gameserver/runtime/base/Unported.h"

namespace aion::gameserver::network::aion::serverpackets {

namespace {

/** Java: the lambda of SETTLED_ITEMS_DYNAMIC_BODY_PART_SIZE_CALCULATOR (SM_BROKER_SERVICE.java:23, key SM_BROKER_SERVICE@L23:4) */
struct SettledItemsDynamicBodyPartSizeCalculator : runtime::TaskStruct {
	int32_t operator()(model::gameobjects::BrokerItem& item) const {
		AION_UNPORTED();
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
	AION_UNPORTED();
}

void SM_BROKER_SERVICE::writeCancelRegisteredItem() {
	AION_UNPORTED();
}

void SM_BROKER_SERVICE::writeSearchedItems() {
	AION_UNPORTED();
}

void SM_BROKER_SERVICE::writeRegisteredItems() {
	AION_UNPORTED();
}

void SM_BROKER_SERVICE::writeRegisterItem() {
	AION_UNPORTED();
}

void SM_BROKER_SERVICE::writeShowSettledIcon() {
	AION_UNPORTED();
}

void SM_BROKER_SERVICE::writeRemoveSettledIcon() {
	AION_UNPORTED();
}

void SM_BROKER_SERVICE::writeShowSettledItems() {
	AION_UNPORTED();
}

void SM_BROKER_SERVICE::writeShowSellWindow() {
	AION_UNPORTED();
}

void SM_BROKER_SERVICE::writeRegisteredItemInfo(model::gameobjects::BrokerItem& brokerItem) {
	AION_UNPORTED();
}

void SM_BROKER_SERVICE::writeItemInfo(model::gameobjects::BrokerItem& brokerItem) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::network::aion::serverpackets
