#pragma once

#include <cstdint>

#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/model/trade/fwd.h"

namespace aion::gameserver::model::trade {

/**
 * S0c declaration header (docs/design/hub-headers.md §3.5), written for the member-type guards waiting for it at the spine freeze.
 *
 * @author ATracer
 */
class ExchangeItem : public runtime::RefCounted {
	AION_MAKE_REF_FRIEND
private:
	const int32_t itemObjId;
	runtime::Field<int64_t> itemCount{};
	runtime::Field<runtime::Ref<gameobjects::Item>> item{};

protected:
	/** Used when exchange item != original item */
	ExchangeItem(int32_t itemObjId, int64_t itemCount, gameobjects::Item& item);

public:
	static runtime::Ref<ExchangeItem> create(int32_t value, int64_t itemCountValue, gameobjects::Item& itemValue);

	void setItem(runtime::Ptr<gameobjects::Item> item);

	void addCount(int64_t countToAdd);

	runtime::Ptr<gameobjects::Item> getItem() const { return this->item.get(); }

	int32_t getItemObjId() const { return this->itemObjId; }

	int64_t getItemCount() const { return this->itemCount.get(); }

protected:
	~ExchangeItem() override;
};

} // namespace aion::gameserver::model::trade
