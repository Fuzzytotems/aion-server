#pragma once

#include <cstdint>
#include <vector>

#include "aion/gameserver/runtime/collections/Rc.h"
#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/gameserver/model/broker/fwd.h"
#include "aion/gameserver/model/gameobjects/fwd.h"

namespace aion::gameserver::model::broker {

/**
 * S0c declaration header (docs/design/hub-headers.md §3.5), written for the member-type guards waiting for it at the spine freeze.
 *
 * @author ATracer
 */
class BrokerPlayerCache : public runtime::RefCounted {
	AION_MAKE_REF_FRIEND
private:
	runtime::Field<runtime::Ref<runtime::RcArrayList<runtime::Ref<gameobjects::BrokerItem>>>> brokerListCache{};
	runtime::Field<int32_t> brokerMaskCache{};
	runtime::Field<int8_t> brokerSoftTypeCache{};
	runtime::Field<int32_t> brokerStartPageCache{};
	runtime::Field<runtime::Ref<runtime::RcArrayList<int32_t>>> itemList{};

protected:
	/** Java field initializers: brokerListCache = Collections.emptyList() (an empty list), itemList = new ArrayList<>() */
	BrokerPlayerCache();

public:
	static runtime::Ref<BrokerPlayerCache> create();

	runtime::Ptr<runtime::RcArrayList<runtime::Ref<gameobjects::BrokerItem>>> getBrokerListCache() const { return brokerListCache.get(); }

	/** @param brokerListCache the list to keep (Java stores the caller's list) */
	void setBrokerListCache(runtime::Ptr<runtime::RcArrayList<runtime::Ref<gameobjects::BrokerItem>>> brokerListCache);

	void removeFromCache(gameobjects::BrokerItem& item);

	int32_t getBrokerMaskCache() const { return this->brokerMaskCache.get(); }

	void setBrokerMaskCache(int32_t value) { this->brokerMaskCache.set(value); }

	int8_t getBrokerSortTypeCache() const { return this->brokerSoftTypeCache.get(); }

	void setBrokerSortTypeCache(int8_t value) { this->brokerSoftTypeCache.set(value); }

	int32_t getBrokerStartPageCache() const { return this->brokerStartPageCache.get(); }

	runtime::Ptr<runtime::RcArrayList<int32_t>> getSearchItemList() const { return itemList.get(); }

	void setBrokerStartPageCache(int32_t value) { this->brokerStartPageCache.set(value); }

	/** @param itemList the list to keep (Java stores the caller's list) */
	void setSearchItemsList(runtime::Ptr<runtime::RcArrayList<int32_t>> itemList);

protected:
	~BrokerPlayerCache() override;
};

} // namespace aion::gameserver::model::broker
