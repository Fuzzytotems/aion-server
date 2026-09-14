#include "aion/gameserver/model/broker/BrokerPlayerCache.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/model/gameobjects/BrokerItem.h"

namespace aion::gameserver::model::broker {

BrokerPlayerCache::BrokerPlayerCache()
	: brokerListCache(runtime::RcArrayList<runtime::Ref<gameobjects::BrokerItem>>::create(AION_LOCK_CLASS(BrokerPlayerCache::brokerListCache))),
	  itemList(runtime::RcArrayList<int32_t>::create(AION_LOCK_CLASS(BrokerPlayerCache::itemList))) {
}

runtime::Ref<BrokerPlayerCache> BrokerPlayerCache::create() {
	return runtime::makeRef<BrokerPlayerCache>();
}

void BrokerPlayerCache::setBrokerListCache(runtime::Ptr<runtime::RcArrayList<runtime::Ref<gameobjects::BrokerItem>>> value) {
	brokerListCache.set(value);
}

void BrokerPlayerCache::removeFromCache(gameobjects::BrokerItem& item) {
	AION_UNPORTED();
}

void BrokerPlayerCache::setSearchItemsList(runtime::Ptr<runtime::RcArrayList<int32_t>> value) {
	itemList.set(value);
}

BrokerPlayerCache::~BrokerPlayerCache() = default;

} // namespace aion::gameserver::model::broker
