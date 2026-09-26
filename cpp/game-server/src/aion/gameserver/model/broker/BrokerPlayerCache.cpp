#include "aion/gameserver/model/broker/BrokerPlayerCache.h"

#include <utility>

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
	// Java: brokerListCache = brokerListCache.stream().filter(i -> !i.equals(item)).toList() (BrokerItem keeps Object.equals: identity)
	runtime::Ref<runtime::RcArrayList<runtime::Ref<gameobjects::BrokerItem>>> filtered =
		runtime::RcArrayList<runtime::Ref<gameobjects::BrokerItem>>::create(AION_LOCK_CLASS(BrokerPlayerCache::brokerListCache));
	for (runtime::Ptr<gameobjects::BrokerItem> i : *brokerListCache.get()) {
		if (i.rawPointer() != &item)
			filtered->add(runtime::Ref<gameobjects::BrokerItem>(i));
	}
	brokerListCache.set(std::move(filtered));
}

void BrokerPlayerCache::setSearchItemsList(runtime::Ptr<runtime::RcArrayList<int32_t>> value) {
	itemList.set(value);
}

BrokerPlayerCache::~BrokerPlayerCache() = default;

} // namespace aion::gameserver::model::broker
