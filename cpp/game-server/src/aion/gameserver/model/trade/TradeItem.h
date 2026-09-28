#pragma once

#include <cstdint>

#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/gameserver/model/templates/item/fwd.h"
#include "aion/gameserver/model/trade/fwd.h"

namespace aion::gameserver::model::trade {

/**
 * S0c declaration header (docs/design/hub-headers.md §3.5), written for the member-type guards waiting for it at the spine freeze.
 *
 * @author ATracer, Neon
 */
class TradeItem : public runtime::RefCounted {
	AION_MAKE_REF_FRIEND
private:
	const int32_t itemId;

protected:
	runtime::Field<int64_t> count{};

	TradeItem(int32_t itemId, int64_t count);

public:
	static runtime::Ref<TradeItem> create(int32_t value, int64_t countValue);

	const templates::item::ItemTemplate* getItemTemplate();

	int32_t getItemId() const { return this->itemId; }

	int64_t getCount() const { return this->count.get(); }

protected:
	~TradeItem() override;
};

} // namespace aion::gameserver::model::trade
